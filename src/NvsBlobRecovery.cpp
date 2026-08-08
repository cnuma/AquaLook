#include <Arduino.h>
#include <cstdlib>
#include <cstring>
#include <nvs.h>

namespace {
constexpr const char* CONFIG_KEY = "config";
constexpr size_t CONFIG_BLOB_MIN_SIZE = 4096U;
constexpr size_t CONFIG_BACKUP_MAX_SIZE = 16384U;
}

extern "C" esp_err_t __real_nvs_set_blob(nvs_handle_t handle,
                                          const char* key,
                                          const void* value,
                                          size_t length);

extern "C" esp_err_t __wrap_nvs_set_blob(nvs_handle_t handle,
                                          const char* key,
                                          const void* value,
                                          size_t length) {
    const esp_err_t firstErr = __real_nvs_set_blob(handle, key, value, length);

    if (key != nullptr && std::strcmp(key, CONFIG_KEY) == 0) {
        Serial.printf(
            "[NVS-DIAG] set_blob key=%s len=%u firstErr=%d heap=%u\n",
            key,
            (unsigned)length,
            (int)firstErr,
            (unsigned)ESP.getFreeHeap()
        );
    }

    if (firstErr != ESP_ERR_NVS_NOT_ENOUGH_SPACE ||
        key == nullptr ||
        value == nullptr ||
        length < CONFIG_BLOB_MIN_SIZE ||
        std::strcmp(key, CONFIG_KEY) != 0) {
        return firstErr;
    }

    void* previous = nullptr;
    size_t previousLength = 0U;
    esp_err_t previousErr = nvs_get_blob(handle, key, nullptr, &previousLength);
    bool previousUnreadable = false;

    Serial.printf(
        "[NVS-DIAG] recovery begin previousProbeErr=%d previousLen=%u\n",
        (int)previousErr,
        (unsigned)previousLength
    );

    if (previousErr == ESP_OK && previousLength > 0U) {
        if (previousLength > CONFIG_BACKUP_MAX_SIZE) {
            Serial.printf(
                "[NVS-DIAG] recovery abort previousLenTooLarge=%u\n",
                (unsigned)previousLength
            );
            return firstErr;
        }

        previous = std::malloc(previousLength);
        if (previous == nullptr) {
            Serial.printf("[NVS-DIAG] recovery abort malloc failed\n");
            return firstErr;
        }

        size_t readLength = previousLength;
        previousErr = nvs_get_blob(handle, key, previous, &readLength);
        Serial.printf(
            "[NVS-DIAG] recovery read previousErr=%d requested=%u read=%u\n",
            (int)previousErr,
            (unsigned)previousLength,
            (unsigned)readLength
        );
        if (previousErr != ESP_OK || readLength != previousLength) {
            std::free(previous);
            previous = nullptr;
            previousLength = 0U;
            previousUnreadable = true;
        }
    } else if (previousErr != ESP_ERR_NVS_NOT_FOUND) {
        previousUnreadable = true;
        Serial.printf(
            "[NVS-DIAG] recovery previous unreadable err=%d; reconstruction allowed\n",
            (int)previousErr
        );
    }

    esp_err_t err = nvs_erase_key(handle, key);
    Serial.printf("[NVS-DIAG] recovery erase err=%d\n", (int)err);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        std::free(previous);
        return firstErr;
    }

    err = __real_nvs_set_blob(handle, key, value, length);
    Serial.printf(
        "[NVS-DIAG] recovery rewrite err=%d len=%u heap=%u\n",
        (int)err,
        (unsigned)length,
        (unsigned)ESP.getFreeHeap()
    );
    if (err == ESP_OK) {
        std::free(previous);
        return ESP_OK;
    }

    if (!previousUnreadable && previous != nullptr && previousLength > 0U) {
        const esp_err_t restoreErr =
            __real_nvs_set_blob(handle, key, previous, previousLength);
        Serial.printf(
            "[NVS-DIAG] recovery restore err=%d len=%u\n",
            (int)restoreErr,
            (unsigned)previousLength
        );
    } else {
        Serial.printf(
            "[NVS-DIAG] recovery no restore previousUnreadable=%s previousLen=%u\n",
            previousUnreadable ? "yes" : "no",
            (unsigned)previousLength
        );
    }

    std::free(previous);
    return err;
}
