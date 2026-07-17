#pragma once

#include <Arduino.h>
#include <SdFat.h>
#include "config.h"

enum class StorageStatus : uint8_t {
    NOT_INITIALIZED = 0,
    READY,
    SD_UNAVAILABLE,
    WEB_ASSETS_MISSING,
    READ_ERROR
};

enum class StorageRecoveryState : uint8_t {
    IDLE = 0,
    WAITING_RETRY,
    RETRYING,
    FAILED
};

class StorageManager {
public:
    void begin();
    void end();
    void update();

    bool isSdAvailable() const { return _sdAvailable; }
    bool areWebAssetsAvailable() const {
        return _status == StorageStatus::READY && _sdAvailable;
    }

    StorageStatus status() const { return _status; }
    const char* statusCode() const;
    const char* statusMessage() const;

    StorageRecoveryState recoveryState() const { return _recoveryState; }
    const char* recoveryStateCode() const;
    uint8_t recoveryAttempt() const { return _recoveryAttempt; }
    uint8_t recoveryMaxAttempts() const;
    bool isRestartRecommended() const { return _restartRecommended; }
    bool isSlowRecoveryMode() const { return _slowRecoveryMode; }
    uint32_t unavailableSinceMs() const { return _unavailableSinceMs; }

    uint8_t cardType() const { return _cardType; }
    uint64_t cardSizeBytes() const { return _cardSizeBytes; }
    uint64_t totalBytes() const { return _totalBytes; }
    uint64_t usedBytes() const { return _usedBytes; }

    bool existsOnSd(const char* path);
    bool openRead(const char* path, FsFile& file);
    void reportReadError(const char* path);
    const char* cardTypeName() const;

private:
    enum class RecoveryTaskResult : uint8_t {
        NONE = 0,
        RUNNING,
        SUCCESS,
        FAILED,
        START_FAILED
    };

    bool mountSd(bool publishAvailability);
    void resetCardMetadata();
    void markUnavailable(StorageStatus status,
                         const char* reason,
                         const char* path);
    void scheduleRecovery(uint32_t nowMs);
    void scheduleSlowRecovery(uint32_t nowMs);
    void startRecoveryTask(uint32_t nowMs);
    void processRecoveryTaskResult(uint32_t nowMs);
    void logMounted(bool recovered, uint32_t downtimeMs);

    static void recoveryTaskEntry(void* parameter);

    SoftSpiDriver<SD_MISO_PIN, SD_MOSI_PIN, SD_SCLK_PIN> _softSpi;
    SdFs _sd;

    volatile bool _sdAvailable = false;
    volatile StorageStatus _status = StorageStatus::NOT_INITIALIZED;
    volatile StorageRecoveryState _recoveryState = StorageRecoveryState::IDLE;

    uint8_t _cardType = 0;
    uint64_t _cardSizeBytes = 0;
    uint64_t _totalBytes = 0;
    uint64_t _usedBytes = 0;

    uint32_t _lastHealthCheckMs = 0;
    uint8_t _healthFailureCount = 0;

    uint8_t _recoveryAttempt = 0;
    uint32_t _nextRecoveryAttemptMs = 0;
    uint32_t _unavailableSinceMs = 0;
    bool _restartRecommended = false;
    bool _slowRecoveryMode = false;
    uint32_t _slowRecoveryCount = 0;

    volatile RecoveryTaskResult _recoveryTaskResult = RecoveryTaskResult::NONE;
    TaskHandle_t _recoveryTaskHandle = nullptr;

    const char* _lastMountFailureReason = "not_attempted";
};