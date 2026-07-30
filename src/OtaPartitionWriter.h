#pragma once

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

class OtaPartitionWriter {
public:
    enum class Error : uint8_t {
        NONE = 0U,
        INVALID_SIZE,
        PARTITION_NOT_FOUND,
        TARGET_IS_RUNNING,
        IMAGE_TOO_LARGE,
        OTA_BEGIN_FAILED,
        WRITE_NOT_STARTED,
        WRITE_OVERFLOW,
        OTA_WRITE_FAILED,
        SIZE_MISMATCH,
        SHA256_INIT_FAILED,
        SHA256_UPDATE_FAILED,
        SHA256_FINALIZE_FAILED,
        SHA256_MISMATCH,
        OTA_END_FAILED
    };

    struct Result {
        bool success = false;
        Error error = Error::NONE;
        esp_err_t espError = ESP_OK;
        uint32_t expectedSize = 0U;
        uint32_t writtenSize = 0U;
        uint32_t partitionSize = 0U;
        uint32_t partitionAddress = 0U;
        char partitionLabel[17] = "";
        char calculatedSha256[65] = "";
    };

    OtaPartitionWriter();
    ~OtaPartitionWriter();

    bool begin(uint32_t expectedSize);
    bool write(const uint8_t* data, size_t length);
    Result finish(const char* expectedSha256);
    void abort();

    bool active() const;
    const Result& result() const;
    static const char* errorName(Error error);

private:
    void fail(Error error, esp_err_t espError = ESP_OK);
    bool finalizeSha256();
    static bool validSha256(const char* value);
    static void digestToHex(const unsigned char digest[32], char output[65]);

    const esp_partition_t* _runningPartition;
    const esp_partition_t* _targetPartition;
    esp_ota_handle_t _otaHandle;
    bool _started;
    bool _shaStarted;
    mbedtls_sha256_context _shaContext;
    Result _result;
};
