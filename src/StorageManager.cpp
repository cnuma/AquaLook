#include "StorageManager.h"

#include "EventLog.h"
#include "FaultManager.h"

namespace {
constexpr uint32_t SD_HEALTH_CHECK_INTERVAL_MS = 2000U;
constexpr uint8_t SD_HEALTH_FAILURE_CONFIRMATIONS = 2U;
constexpr uint8_t SD_RECOVERY_MAX_ATTEMPTS = 5U;
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
    _recoveryTaskResult = RecoveryTaskResult::NONE;
    _recoveryTaskHandle = nullptr;
    _lastMountFailureReason = "not_attempted";

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    if (mountSd(true)) {
        FaultManager::setActive(FaultId::STORAGE_SD, false);
        logMounted(false, 0);
        return;
    }

    FaultManager::setActive(FaultId::STORAGE_SD, true);
    EventLog::log(
        LOG_WARN,
        "Stockage: montage SD initial echoue raison=%s",
        _lastMountFailureReason
    );

    _unavailableSinceMs = millis();
    scheduleRecovery(_unavailableSinceMs);
}

void StorageManager::end() {
    // end() n'est pas appele pendant une tentative de remontage normale.
    // Eviter de detruire l'objet SdFat sous la tache si une fermeture externe
    // exceptionnelle arrive au meme moment.
    if (_recoveryTaskResult == RecoveryTaskResult::RUNNING) {
        EventLog::log(
            LOG_WARN,
            "Stockage: fermeture ignoree pendant une tentative de remontage"
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
    _recoveryTaskResult = RecoveryTaskResult::NONE;
    _recoveryTaskHandle = nullptr;
    _lastMountFailureReason = "not_attempted";

    if (g_registeredStorage == this) {
        g_registeredStorage = nullptr;
    }
}

void StorageManager::update() {
    const uint32_t nowMs = millis();

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

    if (_recoveryState == StorageRecoveryState::FAILED) return;

    if (!_sdAvailable || _status != StorageStatus::READY) return;

    if (nowMs - _lastHealthCheckMs < SD_HEALTH_CHECK_INTERVAL_MS) return;
    _lastHealthCheckMs = nowMs;

    if (_sd.exists("/www/index.html")) {
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

    markUnavailable(
        StorageStatus::READ_ERROR,
        "health_check_failed",
        "/www/index.html"
    );
}

bool StorageManager::existsOnSd(const char* path) {
    return _sdAvailable &&
           _status == StorageStatus::READY &&
           path &&
           path[0] == '/' &&
           _sd.exists(path);
}

bool StorageManager::openRead(const char* path, FsFile& file) {
    if (!_sdAvailable ||
        _status != StorageStatus::READY ||
        !path ||
        path[0] != '/') {
        return false;
    }

    if (file.isOpen()) file.close();
    file = _sd.open(path, O_RDONLY);
    return file.isOpen();
}

void StorageManager::reportReadError(const char* path) {
    if (_recoveryState != StorageRecoveryState::IDLE ||
        !_sdAvailable ||
        _status != StorageStatus::READY) {
        return;
    }

    markUnavailable(
        StorageStatus::READ_ERROR,
        "read_error",
        path
    );
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
    _lastMountFailureReason = reason ? reason : "unknown";

    FaultManager::setActive(FaultId::STORAGE_SD, true);

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
        _recoveryState = StorageRecoveryState::FAILED;
        _restartRecommended = true;

        EventLog::log(
            LOG_ERROR,
            "Stockage: echec apres %u essais, redemarrage conseille",
            static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS)
        );
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

void StorageManager::startRecoveryTask(uint32_t nowMs) {
    (void)nowMs;

    if (_recoveryTaskResult == RecoveryTaskResult::RUNNING) return;

    _recoveryState = StorageRecoveryState::RETRYING;
    _recoveryAttempt++;
    _recoveryTaskResult = RecoveryTaskResult::RUNNING;

    EventLog::log(
        LOG_INFO,
        "Stockage: tentative remontage=%u/%u tache=core0",
        static_cast<unsigned>(_recoveryAttempt),
        static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS)
    );

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

        // Publication finale uniquement depuis la boucle principale : le Web
        // ne peut pas acceder a SdFat pendant que la tache monte le volume.
        _sdAvailable = true;
        _recoveryState = StorageRecoveryState::IDLE;
        _nextRecoveryAttemptMs = 0;
        _restartRecommended = false;

        FaultManager::setActive(FaultId::STORAGE_SD, false);
        logMounted(true, downtimeMs);
        return;
    }

    if (result == RecoveryTaskResult::START_FAILED) {
        _lastMountFailureReason = "task_start_failed";
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
            "Stockage: SD recuperee essai=%u indisponible=%lus",
            static_cast<unsigned>(_recoveryAttempt),
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
    if (_restartRecommended) {
        return "Carte SD toujours indisponible apres plusieurs essais. "
               "Verifier la carte puis redemarrer le module; "
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
        case StorageRecoveryState::FAILED:        return "failed";
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
