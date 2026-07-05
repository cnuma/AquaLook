#include "StorageManager.h"

#include <SD.h>
#include "config.h"
#include "EventLog.h"

void StorageManager::begin() {
    end();

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    _sdSpi.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, _sdSpi, SD_SPI_FREQUENCY)) {
        _sdSpi.end();
        EventLog::log(LOG_WARN, "Stockage: carte SD absente ou montage impossible");
        return;
    }

    _cardType = SD.cardType();
    if (_cardType == CARD_NONE) {
        EventLog::log(LOG_WARN, "Stockage: lecteur SD detecte sans carte exploitable");
        SD.end();
        _sdSpi.end();
        return;
    }

    _cardSizeBytes = SD.cardSize();
    _totalBytes = SD.totalBytes();
    _usedBytes = SD.usedBytes();
    _sdAvailable = true;

    EventLog::log(
        LOG_INFO,
        "Stockage: SD montee type=%s capacite=%llu Mo total=%llu Mo utilise=%llu Mo",
        cardTypeName(),
        static_cast<unsigned long long>(_cardSizeBytes / (1024ULL * 1024ULL)),
        static_cast<unsigned long long>(_totalBytes / (1024ULL * 1024ULL)),
        static_cast<unsigned long long>(_usedBytes / (1024ULL * 1024ULL))
    );
}

void StorageManager::end() {
    SD.end();
    _sdSpi.end();

    _sdAvailable = false;
    _cardType = CARD_NONE;
    _cardSizeBytes = 0;
    _totalBytes = 0;
    _usedBytes = 0;
}

bool StorageManager::existsOnSd(const char* path) const {
    return _sdAvailable && path && path[0] == '/' && SD.exists(path);
}

File StorageManager::openOnSd(const char* path, const char* mode) const {
    if (!_sdAvailable || !path || path[0] != '/') return File();
    return SD.open(path, mode);
}

const char* StorageManager::cardTypeName() const {
    switch (_cardType) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SDSC";
        case CARD_SDHC: return "SDHC/SDXC";
        default:        return "inconnue";
    }
}
