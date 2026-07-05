#include "StorageManager.h"

#include "EventLog.h"
#include "FaultManager.h"

namespace {
constexpr uint32_t SD_HEALTH_CHECK_INTERVAL_MS = 2000;
}

void StorageManager::begin() {
    end();

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    const SdSpiConfig sdConfig(
        SD_CS_PIN,
        SHARED_SPI,
        SD_SCK_MHZ(0),
        &_softSpi
    );

    if (!_sd.begin(sdConfig)) {
        _status = StorageStatus::SD_UNAVAILABLE;
        FaultManager::setActive(FaultId::STORAGE_SD, true);
        EventLog::log(
            LOG_WARN,
            "Stockage: carte SD absente, illisible ou corrompue"
        );
        return;
    }

    if (!_sd.card() || !_sd.vol()) {
        _status = StorageStatus::SD_UNAVAILABLE;
        FaultManager::setActive(FaultId::STORAGE_SD, true);
        EventLog::log(
            LOG_WARN,
            "Stockage: carte detectee sans volume exploitable (format ou corruption possible)"
        );
        _sd.end();
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

    if (!_sd.exists("/www") || !_sd.exists("/www/index.html")) {
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
    _sd.end();

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
    _lastHealthCheckMs = now;

    // index.html est la sentinelle minimale : il a ete valide au boot.
    // S'il disparait ensuite, la carte a ete retiree ou est devenue illisible.
    if (_sd.exists("/www/index.html")) return;

    _status = StorageStatus::READ_ERROR;
    _sdAvailable = false;
    FaultManager::setActive(FaultId::STORAGE_SD, true);
    EventLog::log(
        LOG_ERROR,
        "Stockage: carte SD retiree ou devenue illisible pendant le fonctionnement"
    );
    _sd.end();
}

bool StorageManager::existsOnSd(const char* path) {
    return _sdAvailable &&
           path &&
           path[0] == '/' &&
           _sd.exists(path);
}

bool StorageManager::openRead(const char* path, FsFile& file) {
    if (!_sdAvailable || !path || path[0] != '/') return false;

    if (file.isOpen()) file.close();
    file = _sd.open(path, O_RDONLY);
    return file.isOpen();
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
    _sd.end();
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
