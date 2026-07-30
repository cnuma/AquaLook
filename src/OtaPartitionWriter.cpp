#include "OtaPartitionWriter.h"

#include <cstring>
#include <mbedtls/sha256.h>

namespace {
constexpr size_t SHA256_HEX_LENGTH = 64U;
}

OtaPartitionWriter::OtaPartitionWriter()
    : _runningPartition(nullptr),
      _targetPartition(nullptr),
      _otaHandle(0),
      _started(false),
      _shaStarted(false),
      _shaContext(new mbedtls_sha256_context()) {
    mbedtls_sha256_init(_shaContext);
}

OtaPartitionWriter::~OtaPartitionWriter() {
    abort();
    mbedtls_sha256_free(_shaContext);
    delete _shaContext;
    _shaContext = nullptr;
}

bool OtaPartitionWriter::begin(uint32_t expectedSize) {
    abort();
    _result = Result{};
    _result.expectedSize = expectedSize;

    if (expectedSize == 0U) {
        fail(Error::INVALID_SIZE);
        return false;
    }

    _runningPartition = esp_ota_get_running_partition();
    _targetPartition = esp_ota_get_next_update_partition(nullptr);
    if (!_runningPartition || !_targetPartition) {
        fail(Error::PARTITION_NOT_FOUND);
        return false;
    }
    if (_runningPartition == _targetPartition ||
        _runningPartition->address == _targetPartition->address) {
        fail(Error::TARGET_IS_RUNNING);
        return false;
    }

    _result.partitionSize = _targetPartition->size;
    _result.partitionAddress = _targetPartition->address;
    std::strncpy(_result.partitionLabel, _targetPartition->label,
                 sizeof(_result.partitionLabel) - 1U);
    _result.partitionLabel[sizeof(_result.partitionLabel) - 1U] = '\0';

    if (expectedSize > _targetPartition->size) {
        fail(Error::IMAGE_TOO_LARGE);
        return false;
    }

    mbedtls_sha256_free(_shaContext);
    mbedtls_sha256_init(_shaContext);
    if (mbedtls_sha256_starts_ret(_shaContext, 0) != 0) {
        fail(Error::SHA256_FINALIZE_FAILED);
        return false;
    }
    _shaStarted = true;

    const esp_err_t beginError = esp_ota_begin(
        _targetPartition,
        static_cast<size_t>(expectedSize),
        &_otaHandle
    );
    if (beginError != ESP_OK) {
        fail(Error::OTA_BEGIN_FAILED, beginError);
        return false;
    }

    _started = true;
    return true;
}

bool OtaPartitionWriter::write(const uint8_t* data, size_t length) {
    if (!_started || !_shaStarted || !data || length == 0U) {
        fail(Error::WRITE_NOT_STARTED);
        return false;
    }

    if (_result.writtenSize + length > _result.expectedSize) {
        fail(Error::WRITE_OVERFLOW);
        abort();
        return false;
    }

    const esp_err_t writeError = esp_ota_write(_otaHandle, data, length);
    if (writeError != ESP_OK) {
        fail(Error::OTA_WRITE_FAILED, writeError);
        abort();
        return false;
    }

    if (mbedtls_sha256_update_ret(_shaContext, data, length) != 0) {
        fail(Error::SHA256_FINALIZE_FAILED);
        abort();
        return false;
    }

    _result.writtenSize += static_cast<uint32_t>(length);
    return true;
}

OtaPartitionWriter::Result OtaPartitionWriter::finish(const char* expectedSha256) {
    if (!_started || !_shaStarted) {
        fail(Error::WRITE_NOT_STARTED);
        return _result;
    }
    if (_result.writtenSize != _result.expectedSize) {
        fail(Error::SIZE_MISMATCH);
        abort();
        return _result;
    }
    if (!validSha256(expectedSha256)) {
        fail(Error::SHA256_MISMATCH);
        abort();
        return _result;
    }
    if (!finalizeSha256()) {
        abort();
        return _result;
    }
    if (std::strcmp(_result.calculatedSha256, expectedSha256) != 0) {
        fail(Error::SHA256_MISMATCH);
        abort();
        return _result;
    }

    const esp_err_t endError = esp_ota_end(_otaHandle);
    _started = false;
    _otaHandle = 0;
    if (endError != ESP_OK) {
        fail(Error::OTA_END_FAILED, endError);
        return _result;
    }

    _result.success = true;
    _result.error = Error::NONE;
    _result.espError = ESP_OK;
    return _result;
}

void OtaPartitionWriter::abort() {
    if (_started) {
        esp_ota_abort(_otaHandle);
    }
    _started = false;
    _otaHandle = 0;
    _runningPartition = nullptr;
    _targetPartition = nullptr;
    if (_shaStarted) {
        mbedtls_sha256_free(_shaContext);
        mbedtls_sha256_init(_shaContext);
    }
    _shaStarted = false;
}

bool OtaPartitionWriter::active() const {
    return _started;
}

const OtaPartitionWriter::Result& OtaPartitionWriter::result() const {
    return _result;
}

const char* OtaPartitionWriter::errorName(Error error) {
    switch (error) {
        case Error::NONE: return "none";
        case Error::INVALID_SIZE: return "invalid-size";
        case Error::PARTITION_NOT_FOUND: return "partition-not-found";
        case Error::TARGET_IS_RUNNING: return "target-is-running";
        case Error::IMAGE_TOO_LARGE: return "image-too-large";
        case Error::OTA_BEGIN_FAILED: return "ota-begin-failed";
        case Error::WRITE_NOT_STARTED: return "write-not-started";
        case Error::WRITE_OVERFLOW: return "write-overflow";
        case Error::OTA_WRITE_FAILED: return "ota-write-failed";
        case Error::SIZE_MISMATCH: return "size-mismatch";
        case Error::SHA256_FINALIZE_FAILED: return "sha256-failed";
        case Error::SHA256_MISMATCH: return "sha256-mismatch";
        case Error::OTA_END_FAILED: return "ota-end-failed";
    }
    return "unknown";
}

void OtaPartitionWriter::fail(Error error, esp_err_t espError) {
    _result.success = false;
    _result.error = error;
    _result.espError = espError;
}

bool OtaPartitionWriter::finalizeSha256() {
    unsigned char digest[32] = {};
    if (mbedtls_sha256_finish_ret(_shaContext, digest) != 0) {
        fail(Error::SHA256_FINALIZE_FAILED);
        return false;
    }
    _shaStarted = false;
    digestToHex(digest, _result.calculatedSha256);
    return true;
}

bool OtaPartitionWriter::validSha256(const char* value) {
    if (!value || std::strlen(value) != SHA256_HEX_LENGTH) return false;
    for (size_t index = 0U; index < SHA256_HEX_LENGTH; ++index) {
        const char c = value[index];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

void OtaPartitionWriter::digestToHex(const unsigned char digest[32], char output[65]) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    for (size_t index = 0U; index < 32U; ++index) {
        output[index * 2U] = HEX_DIGITS[(digest[index] >> 4U) & 0x0FU];
        output[index * 2U + 1U] = HEX_DIGITS[digest[index] & 0x0FU];
    }
    output[64] = '\0';
}
