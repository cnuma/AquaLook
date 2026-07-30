#include "StorageManager.h"

#include "EventLog.h"
#include "FaultManager.h"
#include "IncidentManager.h"

namespace {
constexpr uint32_t SD_HEALTH_CHECK_INTERVAL_MS = 2000U;
constexpr uint8_t SD_HEALTH_FAILURE_CONFIRMATIONS = 2U;
constexpr uint8_t SD_RECOVERY_MAX_ATTEMPTS = 5U;
constexpr uint32_t SD_SLOW_RECOVERY_INTERVAL_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t SD_WEB_DRAIN_WARNING_MS = 15000U;
constexpr uint32_t SD_RECOVERY_TASK_STACK = 4096U;
constexpr UBaseType_t SD_RECOVERY_TASK_PRIORITY = 1U;
constexpr BaseType_t SD_RECOVERY_TASK_CORE = 0;

const uint32_t SD_RECOVERY_DELAYS_MS[SD_RECOVERY_MAX_ATTEMPTS] = {
    2000U,
    5000U,
    10000U,
    30000U,
    60000U
};

StorageManager* g_registeredStorage = nullptr;

bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}
}

void storageHealthUpdate() {
    if (g_registeredStorage) g_registeredStorage->update();
}

void StorageManager::begin() {
    g_registeredStorage = this;
    IncidentManager::begin();

    _sd.end();
    resetCardMetadata();

    _status = StorageStatus::NOT_INITIALIZED;
    _recoveryState = StorageRecoveryState::IDLE;
    _lastHealthCheckMs = 0;
    _healthFailureCount = 0;
    _recoveryAttempt = 0;
    _nextRecoveryAttemptMs = 0;
    _unavailableSinceMs = 0;
    _restartRecommended = false;
    _slowRecoveryMode = false;
    _slowRecoveryCount = 0;
    _recoveryTaskResult = RecoveryTaskResult::NONE;
    _recoveryTaskHandle = nullptr;
    resetWebReadState();
    _lastMountFailureReason = "not_attempted";

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    if (mountSd(true)) {
        FaultManager::setActive(FaultId::STORAGE_SD, false);

        const PersistentIncidentSnapshot incident = IncidentManager::storageSd();
        if (incident.state == IncidentState::ACTIVE) {
            IncidentManager::recoverStorageSd("available_after_reboot");
        }

        logMounted(false, 0);
        return;
    }

    FaultManager::setActive(FaultId::STORAGE_SD, true);
    IncidentManager::activateStorageSd(_lastMountFailureReason);

    EventLog::log(
        LOG_WARN,
        "Stockage: montage SD initial echoue raison=%s",
        _lastMountFailureReason
    );

    _unavailableSinceMs = millis();
    scheduleRecovery(_unavailableSinceMs);
}

void StorageManager::end() {
    if (_recoveryTaskResult == RecoveryTaskResult::RUNNING) {
        EventLog::log(
            LOG_WARN,
            "Stockage: fermeture ignoree pendant une tentative de remontage"
        );
        return;
    }

    uint32_t activeWebReads = 0;
    bool healthCheckActive = false;
    portENTER_CRITICAL(&_webReadMux);
    activeWebReads = _activeWebReads;
    healthCheckActive = _healthCheckActive;
    portEXIT_CRITICAL(&_webReadMux);

    if (activeWebReads != 0U || healthCheckActive) {
        EventLog::log(
            LOG_WARN,
            "Stockage: fermeture ignoree lectures Web=%lu controle=%s",
            static_cast<unsigned long>(activeWebReads),
            healthCheckActive ? "actif" : "inactif"
        );
        return;
    }

    _sd.end();
    resetCardMetadata();

    _status = StorageStatus::NOT_INITIALIZED;
    _recoveryState = StorageRecoveryState::IDLE;
    _lastHealthCheckMs = 0;
    _healthFailureCount = 0;
    _recoveryAttempt = 0;
    _nextRecoveryAttemptMs = 0;
    _unavailableSinceMs = 0;
    _restartRecommended = false;
    _slowRecoveryMode = false;
    _slowRecoveryCount = 0;
    _recoveryTaskResult = RecoveryTaskResult::NONE;
    _recoveryTaskHandle = nullptr;
    resetWebReadState();
    _lastMountFailureReason = "not_attempted";

    if (g_registeredStorage == this) {
        g_registeredStorage = nullptr;
    }
}

void StorageManager::update() {
    const uint32_t nowMs = millis();
    bool readErrorPending = false;
    bool logDrainWarning = false;
    uint32_t activeWebReads = 0;
    char pendingPath[sizeof(_pendingReadErrorPath)] = {};

    portENTER_CRITICAL(&_webReadMux);
    readErrorPending = _readErrorPending;
    activeWebReads = _activeWebReads;

    if (readErrorPending &&
        activeWebReads != 0U &&
        !_drainWarningLogged &&
        nowMs - _quarantineStartedMs >= SD_WEB_DRAIN_WARNING_MS) {
        _drainWarningLogged = true;
        logDrainWarning = true;
    }

    if (readErrorPending && activeWebReads == 0U) {
        strncpy(
            pendingPath,
            _pendingReadErrorPath[0] ? _pendingReadErrorPath : "inconnu",
            sizeof(pendingPath) - 1U
        );
        pendingPath[sizeof(pendingPath) - 1U] = '\0';
        _readErrorPending = false;
    }
    portEXIT_CRITICAL(&_webReadMux);

    if (logDrainWarning) {
        EventLog::log(
            LOG_WARN,
            "Stockage: quarantaine SD en attente lectures Web actives=%lu",
            static_cast<unsigned long>(activeWebReads)
        );
    }

    if (readErrorPending) {
        if (activeWebReads != 0U) return;

        markUnavailable(
            StorageStatus::READ_ERROR,
            "read_error",
            pendingPath
        );
        return;
    }

    if (_recoveryState == StorageRecoveryState::WAITING_RETRY) {
        if (deadlineReached(nowMs, _nextRecoveryAttemptMs)) {
            startRecoveryTask(nowMs);
        }
        return;
    }

    if (_recoveryState == StorageRecoveryState::RETRYING) {
        processRecoveryTaskResult(nowMs);
        return;
    }

    if (_recoveryState == StorageRecoveryState::FAILED) {
        if (deadlineReached(nowMs, _nextRecoveryAttemptMs)) {
            startRecoveryTask(nowMs);
        }
        return;
    }

    if (!_sdAvailable || _status != StorageStatus::READY) return;

    if (nowMs - _lastHealthCheckMs < SD_HEALTH_CHECK_INTERVAL_MS) return;
    _lastHealthCheckMs = nowMs;

    bool runHealthCheck = false;
    portENTER_CRITICAL(&_webReadMux);
    if (_activeWebReads == 0U &&
        !_healthCheckActive &&
        !_webReadQuarantined &&
        !_readErrorPending) {
        _healthCheckActive = true;
        runHealthCheck = true;
    }
    portEXIT_CRITICAL(&_webReadMux);

    if (!runHealthCheck) return;

    const bool indexAvailable = _sd.exists("/www/index.html");

    portENTER_CRITICAL(&_webReadMux);
    _healthCheckActive = false;
    portEXIT_CRITICAL(&_webReadMux);

    if (indexAvailable) {
        _healthFailureCount = 0;
        return;
    }

    if (_healthFailureCount < 0xFFU) {
        _healthFailureCount++;
    }

    if (_healthFailureCount < SD_HEALTH_FAILURE_CONFIRMATIONS) {
        EventLog::log(
            LOG_WARN,
            "Stockage: controle SD echoue confirmation=%u/%u",
            static_cast<unsigned>(_healthFailureCount),
            static_cast<unsigned>(SD_HEALTH_FAILURE_CONFIRMATIONS)
        );
        return;
    }

    reportReadError("/www/index.html");
}

bool StorageManager::existsOnSd(const char* path) {
    if (!path || path[0] != '/') return false;

    bool leaseAcquired = false;
    portENTER_CRITICAL(&_webReadMux);
    if (!_healthCheckActive &&
        !_webReadQuarantined &&
        _sdAvailable &&
        _status == StorageStatus::READY) {
        _activeWebReads++;
        leaseAcquired = true;
    }
    portEXIT_CRITICAL(&_webReadMux);

    if (!leaseAcquired) return false;

    const bool exists = _sd.exists(path);
    releaseWebRead();
    return exists;
}

bool StorageManager::openRead(const char* path, FsFile& file) {
    if (!path || path[0] != '/') {
        return false;
    }

    bool leaseAcquired = false;
    portENTER_CRITICAL(&_webReadMux);
    if (!_healthCheckActive &&
        !_webReadQuarantined &&
        _sdAvailable &&
        _status == StorageStatus::READY) {
        _activeWebReads++;
        leaseAcquired = true;
    }
    portEXIT_CRITICAL(&_webReadMux);

    if (!leaseAcquired) return false;

    if (file.isOpen()) file.close();
    file = _sd.open(path, O_RDONLY);
    if (file.isOpen()) return true;

    releaseWebRead();
    return false;
}

void StorageManager::releaseWebRead() {
    portENTER_CRITICAL(&_webReadMux);
    if (_activeWebReads != 0U) {
        _activeWebReads--;
    }
    portEXIT_CRITICAL(&_webReadMux);
}

void StorageManager::reportReadError(const char* path) {
    const uint32_t nowMs = millis();

    portENTER_CRITICAL(&_webReadMux);
    if (_recoveryState == StorageRecoveryState::IDLE &&
        _sdAvailable &&
        _status == StorageStatus::READY) {
        if (!_readErrorPending) {
            _quarantineStartedMs = nowMs;
            _drainWarningLogged = false;
            strncpy(
                _pendingReadErrorPath,
                path ? path : "inconnu",
                sizeof(_pendingReadErrorPath) - 1U
            );
            _pendingReadErrorPath[sizeof(_pendingReadErrorPath) - 1U] = '\0';
        }

        _webReadQuarantined = true;
        _readErrorPending = true;
    }
    portEXIT_CRITICAL(&_webReadMux);
}

void StorageManager::resetWebReadState() {
    portENTER_CRITICAL(&_webReadMux);
    _activeWebReads = 0;
    _healthCheckActive = false;
    _webReadQuarantined = false;
    _readErrorPending = false;
    _drainWarningLogged = false;
    _quarantineStartedMs = 0;
    _pendingReadErrorPath[0] = '\0';
    portEXIT_CRITICAL(&_webReadMux);
}

bool StorageManager::mountSd(bool publishAvailability) {
    _sd.end();
    resetCardMetadata();

    const SdSpiConfig sdConfig(
        SD_CS_PIN,
        SHARED_SPI,
        SD_SCK_MHZ(0),
        &_softSpi
    );

    if (!_sd.begin(sdConfig)) {
        _status = StorageStatus::SD_UNAVAILABLE;
        _lastMountFailureReason = "sd_begin_failed";
        return false;
    }

    if (!_sd.card() || !_sd.vol()) {
        _status = StorageStatus::SD_UNAVAILABLE;
        _lastMountFailureReason = "volume_unavailable";
        _sd.end();
        return false;
    }

    _cardType = _sd.card()->type();
    _cardSizeBytes =
        static_cast<uint64_t>(_sd.card()->sectorCount()) * 512ULL;

    const uint64_t bytesPerCluster = _sd.vol()->bytesPerCluster();
    const uint64_t clusterCount = _sd.vol()->clusterCount();
    _totalBytes = clusterCount * bytesPerCluster;
    _usedBytes = 0;

    if (!_sd.exists("/www") || !_sd.exists("/www/index.html")) {
        _status = StorageStatus::WEB_ASSETS_MISSING;
        _lastMountFailureReason = "web_assets_missing";
        _sd.end();
        resetCardMetadata();
        return false;
    }

    _status = StorageStatus::READY;
    _lastMountFailureReason = "none";
    _lastHealthCheckMs = millis();
    _healthFailureCount = 0;
    _sdAvailable = publishAvailability;
    return true;
}

void StorageManager::resetCardMetadata() {
    _sdAvailable = false;
    _cardType = 0;
    _cardSizeBytes = 0;
    _totalBytes = 0;
    _usedBytes = 0;
}

void StorageManager::markUnavailable(StorageStatus status,
                                     const char* reason,
                                     const char* path) {
    if (_recoveryState != StorageRecoveryState::IDLE) return;

    _status = status;
    _sdAvailable = false;
    _healthFailureCount = 0;
    _restartRecommended = false;
    _slowRecoveryMode = false;
    _slowRecoveryCount = 0;
    _lastMountFailureReason = reason ? reason : "unknown";

    FaultManager::setActive(FaultId::STORAGE_SD, true);
    IncidentManager::activateStorageSd(_lastMountFailureReason);

    EventLog::log(
        LOG_ERROR,
        "Stockage: SD indisponible raison=%s chemin=%s",
        _lastMountFailureReason,
        path ? path : "inconnu"
    );

    _sd.end();
    resetCardMetadata();

    _unavailableSinceMs = millis();
    _recoveryAttempt = 0;
    _recoveryTaskResult = RecoveryTaskResult::NONE;
    scheduleRecovery(_unavailableSinceMs);
}

void StorageManager::scheduleRecovery(uint32_t nowMs) {
    if (_recoveryAttempt >= SD_RECOVERY_MAX_ATTEMPTS) {
        _restartRecommended = true;
        _slowRecoveryMode = true;

        IncidentManager::escalateStorageSdRecoveryFailure();

        EventLog::log(
            LOG_ERROR,
            "Stockage: echec apres %u essais, reprise lente toutes les 10min",
            static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS)
        );

        scheduleSlowRecovery(nowMs);
        return;
    }

    const uint32_t delayMs = SD_RECOVERY_DELAYS_MS[_recoveryAttempt];
    _nextRecoveryAttemptMs = nowMs + delayMs;
    _recoveryState = StorageRecoveryState::WAITING_RETRY;

    EventLog::log(
        LOG_INFO,
        "Stockage: remontage programme essai=%u/%u dans=%lus",
        static_cast<unsigned>(_recoveryAttempt + 1U),
        static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS),
        static_cast<unsigned long>(delayMs / 1000U)
    );
}

void StorageManager::scheduleSlowRecovery(uint32_t nowMs) {
    _slowRecoveryMode = true;
    _nextRecoveryAttemptMs = nowMs + SD_SLOW_RECOVERY_INTERVAL_MS;
    _recoveryState = StorageRecoveryState::FAILED;

    EventLog::log(
        LOG_INFO,
        "Stockage: prochaine tentative lente dans=10min"
    );
}

void StorageManager::startRecoveryTask(uint32_t nowMs) {
    (void)nowMs;

    if (_recoveryTaskResult == RecoveryTaskResult::RUNNING) return;

    _recoveryState = StorageRecoveryState::RETRYING;

    if (_slowRecoveryMode) {
        _slowRecoveryCount++;
    } else {
        _recoveryAttempt++;
    }

    _recoveryTaskResult = RecoveryTaskResult::RUNNING;

    if (_slowRecoveryMode) {
        EventLog::log(
            LOG_INFO,
            "Stockage: tentative lente=%lu tache=core0",
            static_cast<unsigned long>(_slowRecoveryCount)
        );
    } else {
        EventLog::log(
            LOG_INFO,
            "Stockage: tentative remontage=%u/%u tache=core0",
            static_cast<unsigned>(_recoveryAttempt),
            static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS)
        );
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        recoveryTaskEntry,
        "sd-recovery",
        SD_RECOVERY_TASK_STACK,
        this,
        SD_RECOVERY_TASK_PRIORITY,
        &_recoveryTaskHandle,
        SD_RECOVERY_TASK_CORE
    );

    if (created != pdPASS) {
        _recoveryTaskHandle = nullptr;
        _recoveryTaskResult = RecoveryTaskResult::START_FAILED;
    }
}

void StorageManager::processRecoveryTaskResult(uint32_t nowMs) {
    const RecoveryTaskResult result = _recoveryTaskResult;

    if (result == RecoveryTaskResult::NONE ||
        result == RecoveryTaskResult::RUNNING) {
        return;
    }

    _recoveryTaskResult = RecoveryTaskResult::NONE;
    _recoveryTaskHandle = nullptr;

    if (result == RecoveryTaskResult::SUCCESS) {
        const uint32_t downtimeMs = nowMs - _unavailableSinceMs;

        _sdAvailable = true;
        _recoveryState = StorageRecoveryState::IDLE;
        _nextRecoveryAttemptMs = 0;
        _restartRecommended = false;

        portENTER_CRITICAL(&_webReadMux);
        _healthCheckActive = false;
        _webReadQuarantined = false;
        _readErrorPending = false;
        _drainWarningLogged = false;
        _quarantineStartedMs = 0;
        _pendingReadErrorPath[0] = '\0';
        portEXIT_CRITICAL(&_webReadMux);

        FaultManager::setActive(FaultId::STORAGE_SD, false);
        IncidentManager::recoverStorageSd(
            _slowRecoveryMode ? "slow_recovery_success" : "recovery_success"
        );

        logMounted(true, downtimeMs);
        _slowRecoveryMode = false;
        return;
    }

    if (result == RecoveryTaskResult::START_FAILED) {
        _lastMountFailureReason = "task_start_failed";
    }

    if (_slowRecoveryMode) {
        EventLog::log(
            LOG_WARN,
            "Stockage: tentative lente=%lu echouee raison=%s",
            static_cast<unsigned long>(_slowRecoveryCount),
            _lastMountFailureReason
        );
        scheduleSlowRecovery(nowMs);
        return;
    }

    EventLog::log(
        LOG_WARN,
        "Stockage: remontage %u/%u echoue raison=%s",
        static_cast<unsigned>(_recoveryAttempt),
        static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS),
        _lastMountFailureReason
    );

    scheduleRecovery(nowMs);
}

void StorageManager::recoveryTaskEntry(void* parameter) {
    StorageManager* storage = static_cast<StorageManager*>(parameter);

    if (!storage) {
        vTaskDelete(nullptr);
        return;
    }

    const bool mounted = storage->mountSd(false);
    storage->_recoveryTaskResult = mounted
        ? RecoveryTaskResult::SUCCESS
        : RecoveryTaskResult::FAILED;

    vTaskDelete(nullptr);
}

void StorageManager::logMounted(bool recovered, uint32_t downtimeMs) {
    EventLog::log(
        LOG_INFO,
        "Stockage: ressources Web SD validees dans /www"
    );

    EventLog::log(
        LOG_INFO,
        "Stockage: SD montee type=%s capacite=%llu Mo total=%llu Mo",
        cardTypeName(),
        static_cast<unsigned long long>(_cardSizeBytes / (1024ULL * 1024ULL)),
        static_cast<unsigned long long>(_totalBytes / (1024ULL * 1024ULL))
    );

    if (recovered) {
        EventLog::log(
            LOG_INFO,
            "Stockage: SD recuperee essai=%u lentes=%lu indisponible=%lus",
            static_cast<unsigned>(_recoveryAttempt),
            static_cast<unsigned long>(_slowRecoveryCount),
            static_cast<unsigned long>(downtimeMs / 1000U)
        );
    }
}

const char* StorageManager::statusCode() const {
    switch (_status) {
        case StorageStatus::READY:              return "ready";
        case StorageStatus::SD_UNAVAILABLE:     return "sd-unavailable";
        case StorageStatus::WEB_ASSETS_MISSING: return "web-assets-missing";
        case StorageStatus::READ_ERROR:         return "read-error";
        default:                                return "not-initialized";
    }
}

const char* StorageManager::statusMessage() const {
    if (_webReadQuarantined) {
        return "Carte SD en quarantaine. Les lectures Web actives sont terminees "
               "avant la recuperation automatique.";
    }

    if (_slowRecoveryMode) {
        return "Carte SD toujours indisponible apres les essais rapides. "
               "Une tentative automatique est effectuee toutes les 10 minutes; "
               "l'interface LittleFS de secours reste active.";
    }

    if (_recoveryState == StorageRecoveryState::WAITING_RETRY ||
        _recoveryState == StorageRecoveryState::RETRYING) {
        return "Carte SD indisponible. Recuperation automatique en cours; "
               "l'interface LittleFS de secours reste active.";
    }

    switch (_status) {
        case StorageStatus::READY:
            return "Carte SD operationnelle, ressources Web disponibles.";
        case StorageStatus::SD_UNAVAILABLE:
            return "Carte SD absente, illisible ou corrompue. "
                   "Interface de secours LittleFS utilisee.";
        case StorageStatus::WEB_ASSETS_MISSING:
            return "Carte SD montee, mais /www/index.html est absent. "
                   "Interface de secours LittleFS utilisee.";
        case StorageStatus::READ_ERROR:
            return "Erreur de lecture sur la carte SD. "
                   "Interface de secours LittleFS utilisee.";
        default:
            return "Stockage SD non initialise.";
    }
}

const char* StorageManager::recoveryStateCode() const {
    switch (_recoveryState) {
        case StorageRecoveryState::IDLE:          return "idle";
        case StorageRecoveryState::WAITING_RETRY: return "waiting-retry";
        case StorageRecoveryState::RETRYING:      return "retrying";
        case StorageRecoveryState::FAILED:        return "failed-slow-retry";
        default:                                  return "unknown";
    }
}

uint8_t StorageManager::recoveryMaxAttempts() const {
    return SD_RECOVERY_MAX_ATTEMPTS;
}

const char* StorageManager::cardTypeName() const {
    switch (_cardType) {
        case SD_CARD_TYPE_SD1:  return "SD1";
        case SD_CARD_TYPE_SD2:  return "SD2";
        case SD_CARD_TYPE_SDHC: return "SDHC/SDXC";
        default:                return "inconnue";
    }
}
