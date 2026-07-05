#include "StorageManager.h"

#include "EventLog.h"

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
        EventLog::log(
            LOG_WARN,
            "Stockage: carte SD absente ou montage logiciel impossible"
        );
        return;
    }

    if (!_sd.card() || !_sd.vol()) {
        EventLog::log(
            LOG_WARN,
            "Stockage: carte detectee sans volume exploitable"
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
    _cardType = 0;
    _cardSizeBytes = 0;
    _totalBytes = 0;
    _usedBytes = 0;
}

bool StorageManager::existsOnSd(const char* path) {
    return _sdAvailable &&
           path &&
           path[0] == '/' &&
           _sd.exists(path);
}

const char* StorageManager::cardTypeName() const {
    switch (_cardType) {
        case SD_CARD_TYPE_SD1:  return "SD1";
        case SD_CARD_TYPE_SD2:  return "SD2";
        case SD_CARD_TYPE_SDHC: return "SDHC/SDXC";
        default:                return "inconnue";
    }
}
