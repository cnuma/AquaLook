#pragma once

#include <Arduino.h>
#include <SdFat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

enum class StorageStatus : uint8_t {
    NOT_INITIALIZED = 0,
    READY,
    SD_UNAVAILABLE,
    WEB_ASSETS_MISSING,
    READ_ERROR
};

enum class StorageAccessResult : uint8_t {
    FOUND = 0,
    NOT_FOUND,
    BUSY,
    ERROR
};

enum class StorageReadResult : uint8_t {
    DATA = 0,
    END_OF_FILE,
    BUSY,
    ERROR
};

class StorageManager {
public:
    void begin();
    void end();
    void update();

    bool isSdAvailable() const { return _sdAvailable; }
    bool areWebAssetsAvailable() const {
        return _status == StorageStatus::READY;
    }
    StorageStatus status() const { return _status; }
    const char* statusCode() const;
    const char* statusMessage() const;

    uint8_t cardType() const { return _cardType; }
    uint64_t cardSizeBytes() const { return _cardSizeBytes; }
    uint64_t totalBytes() const { return _totalBytes; }
    uint64_t usedBytes() const { return _usedBytes; }

    StorageAccessResult probeOnSd(const char* path);
    StorageAccessResult openRead(const char* path, FsFile& file);
    StorageReadResult readChunk(FsFile& file, uint8_t* buffer,
                                size_t maxLen, size_t& bytesRead);
    bool closeRead(FsFile& file);
    void reportReadError(const char* path);
    const char* cardTypeName() const;

private:
    bool lockSd(TickType_t waitTicks = 0);
    void unlockSd();

    SoftSpiDriver<SD_MISO_PIN, SD_MOSI_PIN, SD_SCLK_PIN> _softSpi;
    SdFs _sd;
    SemaphoreHandle_t _sdMutex = nullptr;
    bool _sdAvailable = false;
    StorageStatus _status = StorageStatus::NOT_INITIALIZED;
    uint8_t _cardType = 0;
    uint64_t _cardSizeBytes = 0;
    uint64_t _totalBytes = 0;
    uint64_t _usedBytes = 0;
    uint32_t _lastHealthCheckMs = 0;
};
