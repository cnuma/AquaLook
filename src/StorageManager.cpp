#include "StorageManager.h"

#include "EventLog.h"
#include "FaultManager.h"

namespace {
constexpr uint32_t SD_HEALTH_CHECK_INTERVAL_MS = 2000;
StorageManager* g_registeredStorage = nullptr;
}

void storageHealthUpdate() {
    if (g_registeredStorage) g_registeredStorage->update();
}

bool StorageManager::lockSd(TickType_t waitTicks) {
    return _sdMutex && xSemaphoreTake(_sdMutex, waitTicks) == pdTRUE;
}

void StorageManager::unlockSd() {
    if (_sdMutex) xSemaphoreGive(_sdMutex);
}

void StorageManager::begin() {
    g_registeredStorage = this;

    if (!_sdMutex) {
        _sdMutex = xSemaphoreCreateMutex();
        if (!_sdMutex) {
            _status = StorageStatus::SD_UNAVAILABLE;
            FaultManager::setActive(FaultId::STORAGE_SD, true);
            EventLog::log(LOG_ERROR, "Stockage: mutex SD impossible a creer");
            return;
        }
    }

    end();

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    const SdSpiConfig sdConfig(
        SD_CS_PIN,
        SHARED_SPI,
        SD_SCK_MHZ(0),
        &_softSpi
    );

    if (!lockSd(pdMS_TO_TICKS(50))) {
        _status = StorageStatus::SD_UNAVAILABLE;
        FaultManager::setActive(FaultId::STORAGE_SD, true);
        EventLog::log(LOG_ERROR, "Stockage: bus SD occupe au montage");
        return;
    }

    const bool mounted = _sd.begin(sdConfig);
    if (!mounted) {
        unlockSd();
        _status = StorageStatus::SD_UNAVAILABLE;
        FaultManager::setActive(FaultId::STORAGE_SD, true);
        EventLog::log(
            LOG_WARN,
            "Stockage: carte SD absente, illisible ou corrompue"
        );
        return;
    }

    if (!_sd.card() || !_sd.vol()) {
        _sd.end();
        unlockSd();
        _status = StorageStatus::SD_UNAVAILABLE;
        FaultManager::setActive(FaultId::STORAGE_SD, true);
        EventLog::log(
            LOG_WARN,
            "Stockage: carte detectee sans volume exploitable (format ou corruption possible)"
        );
        return;
    }

    _cardType = _sd.card()->type();
    _cardSizeBytes =
        static_cast<uint64_t>(_sd.card()->sectorCount()) * 512ULL;

    const uint64_t bytesPerCluster = _sd.vol()->bytesPerCluster();
    const uint64_t clusterCount = _sd.vol()->clusterCount();
    _totalBytes = clusterCount * bytesPerCluster;

    // Ne pas appeler freeClusterCount() au demarrage : sur une carte de
    // grande capacite en SPI logiciel, le parcours complet de la FAT peut
    // bloquer le boot pendant une duree excessive.
    _usedBytes = 0;
    _sdAvailable = true;
    _lastHealthCheckMs = millis();

    const bool hasWebAssets =
        _sd.exists("/www") && _sd.exists("/www/index.html");
    unlockSd();

    if (!hasWebAssets) {
        _status = StorageStatus::WEB_ASSETS_MISSING;
        FaultManager::setActive(FaultId::STORAGE_SD, true);
        EventLog::log(
            LOG_WARN,
            "Stockage: SD montee mais ressources Web absentes (/www/index.html introuvable)"
        );
    } else {
        _status = StorageStatus::READY;
        FaultManager::setActive(FaultId::STORAGE_SD, false);
        EventLog::log(
            LOG_INFO,
            "Stockage: ressources Web SD validees dans /www"
        );
    }

    EventLog::log(
        LOG_INFO,
        "Stockage: SD montee en SPI logiciel type=%s capacite=%llu Mo total=%llu Mo",
        cardTypeName(),
        static_cast<unsigned long long>(_cardSizeBytes / (1024ULL * 1024ULL)),
        static_cast<unsigned long long>(_totalBytes / (1024ULL * 1024ULL))
    );
}

void StorageManager::end() {
    const bool locked = !_sdMutex || lockSd(pdMS_TO_TICKS(50));
    if (locked) {
        _sd.end();
        if (_sdMutex) unlockSd();
    } else {
        EventLog::log(LOG_WARN, "Stockage: arret SD differe, bus occupe");
    }

    _sdAvailable = false;
    _status = StorageStatus::NOT_INITIALIZED;
    _cardType = 0;
    _cardSizeBytes = 0;
    _totalBytes = 0;
    _usedBytes = 0;
    _lastHealthCheckMs = 0;
}

void StorageManager::update() {
    if (!_sdAvailable || _status != StorageStatus::READY) return;

    const uint32_t now = millis();
    if (now - _lastHealthCheckMs < SD_HEALTH_CHECK_INTERVAL_MS) return;

    const StorageAccessResult probe = probeOnSd("/www/index.html");
    if (probe == StorageAccessResult::BUSY) {
        // Une lecture Web est en cours : ne pas transformer une contention
        // temporaire du bus en panne de carte. Le controle sera rejoue plus tard.
        return;
    }

    _lastHealthCheckMs = now;
    if (probe == StorageAccessResult::FOUND) return;

    _status = StorageStatus::READ_ERROR;
    _sdAvailable = false;
    FaultManager::setActive(FaultId::STORAGE_SD, true);
    EventLog::log(
        LOG_ERROR,
        "Stockage: carte SD retiree ou devenue illisible pendant le fonctionnement"
    );

    if (lockSd(0)) {
        _sd.end();
        unlockSd();
    }
}

StorageAccessResult StorageManager::probeOnSd(const char* path) {
    if (!_sdAvailable || !path || path[0] != '/') {
        return StorageAccessResult::ERROR;
    }
    if (!lockSd(0)) return StorageAccessResult::BUSY;

    const bool exists = _sd.exists(path);
    unlockSd();
    return exists ? StorageAccessResult::FOUND : StorageAccessResult::NOT_FOUND;
}

StorageAccessResult StorageManager::openRead(const char* path, FsFile& file) {
    if (!_sdAvailable || !path || path[0] != '/') {
        return StorageAccessResult::ERROR;
    }
    if (!lockSd(0)) return StorageAccessResult::BUSY;

    if (file.isOpen()) file.close();

    if (!_sd.exists(path)) {
        unlockSd();
        return StorageAccessResult::NOT_FOUND;
    }

    file = _sd.open(path, O_RDONLY);
    const bool opened = file.isOpen();
    unlockSd();
    return opened ? StorageAccessResult::FOUND : StorageAccessResult::ERROR;
}

StorageReadResult StorageManager::readChunk(FsFile& file, uint8_t* buffer,
                                            size_t maxLen, size_t& bytesRead) {
    bytesRead = 0;
    if (!_sdAvailable || !file.isOpen() || !buffer || maxLen == 0) {
        return StorageReadResult::ERROR;
    }
    if (!lockSd(0)) return StorageReadResult::BUSY;

    const int32_t count = file.read(buffer, maxLen);
    if (count < 0) {
        file.close();
        unlockSd();
        return StorageReadResult::ERROR;
    }
    if (count == 0) {
        file.close();
        unlockSd();
        return StorageReadResult::END_OF_FILE;
    }

    bytesRead = static_cast<size_t>(count);
    unlockSd();
    return StorageReadResult::DATA;
}

bool StorageManager::closeRead(FsFile& file) {
    if (!file.isOpen()) return true;
    if (!lockSd(0)) return false;
    file.close();
    unlockSd();
    return true;
}

void StorageManager::reportReadError(const char* path) {
    _status = StorageStatus::READ_ERROR;
    _sdAvailable = false;
    FaultManager::setActive(FaultId::STORAGE_SD, true);
    EventLog::log(
        LOG_ERROR,
        "Stockage: erreur de lecture SD sur %s (carte illisible ou corrompue possible)",
        path ? path : "chemin inconnu"
    );

    if (lockSd(0)) {
        _sd.end();
        unlockSd();
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
    switch (_status) {
        case StorageStatus::READY:
            return "Carte SD operationnelle, ressources Web disponibles.";
        case StorageStatus::SD_UNAVAILABLE:
            return "Carte SD absente, illisible ou corrompue. Interface de secours LittleFS utilisee.";
        case StorageStatus::WEB_ASSETS_MISSING:
            return "Carte SD montee, mais /www/index.html est absent. Interface de secours LittleFS utilisee.";
        case StorageStatus::READ_ERROR:
            return "Erreur de lecture sur la carte SD. Carte retiree, illisible ou corrompue possible; interface de secours utilisee.";
        default:
            return "Stockage SD non initialise.";
    }
}

const char* StorageManager::cardTypeName() const {
    switch (_cardType) {
        case SD_CARD_TYPE_SD1:  return "SD1";
        case SD_CARD_TYPE_SD2:  return "SD2";
        case SD_CARD_TYPE_SDHC: return "SDHC/SDXC";
        default:                return "inconnue";
    }
}
