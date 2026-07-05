#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>

class StorageManager {
public:
    void begin();
    void end();

    bool isSdAvailable() const { return _sdAvailable; }
    uint8_t cardType() const { return _cardType; }
    uint64_t cardSizeBytes() const { return _cardSizeBytes; }
    uint64_t totalBytes() const { return _totalBytes; }
    uint64_t usedBytes() const { return _usedBytes; }

    bool existsOnSd(const char* path) const;
    File openOnSd(const char* path, const char* mode = FILE_READ) const;
    const char* cardTypeName() const;

private:
    // Le TFT utilise deja HSPI. Le lecteur microSD integre est cable
    // sur le bus VSPI standard : SCLK 18, MISO 19, MOSI 23, CS 5.
    SPIClass _sdSpi{VSPI};
    bool _sdAvailable = false;
    uint8_t _cardType = 0;
    uint64_t _cardSizeBytes = 0;
    uint64_t _totalBytes = 0;
    uint64_t _usedBytes = 0;
};
