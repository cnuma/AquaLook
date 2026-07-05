#pragma once

#include <Arduino.h>
#include <SdFat.h>
#include "config.h"

class StorageManager {
public:
    void begin();
    void end();

    bool isSdAvailable() const { return _sdAvailable; }
    uint8_t cardType() const { return _cardType; }
    uint64_t cardSizeBytes() const { return _cardSizeBytes; }
    uint64_t totalBytes() const { return _totalBytes; }
    uint64_t usedBytes() const { return _usedBytes; }

    bool existsOnSd(const char* path);
    const char* cardTypeName() const;

private:
    SoftSpiDriver<SD_MISO_PIN, SD_MOSI_PIN, SD_SCLK_PIN> _softSpi;
    SdFs _sd;
    bool _sdAvailable = false;
    uint8_t _cardType = 0;
    uint64_t _cardSizeBytes = 0;
    uint64_t _totalBytes = 0;
    uint64_t _usedBytes = 0;
};
