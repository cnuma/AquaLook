#include <Arduino.h>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <nvs.h>

#include "ConfigManager.h"

namespace {
constexpr const char* CONFIG_KEY = "config";
constexpr const char* WIFI_RESET_SENTINEL = "__AQUALOOK_WIFI_RESET__";
constexpr uint32_t NVS_MAGIC = 0x414C4F4BUL;
constexpr size_t CONFIG_BLOB_MIN_SIZE = 4096U;
constexpr size_t CONFIG_BACKUP_MAX_SIZE = 16384U;

struct PersistedPrefix {
    uint32_t magic;
    uint16_t schema;
    uint16_t payloadSize;
    CfgWifi wifi;
};

static_assert(offsetof(PersistedPrefix, wifi) == 8U,
              "Le prefixe persiste WiFi doit rester apres l'entete NVS");

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

const void* prepareConfigValue(const char* key,
                               const void* value,
                               size_t length,
                               void*& ownedCopy) {
    ownedCopy = nullptr;

    if (!key || !value || std::strcmp(key, CONFIG_KEY) != 0 ||
        length < sizeof(PersistedPrefix) + sizeof(uint32_t)) {
        return value;
    }

    const PersistedPrefix* prefix = static_cast<const PersistedPrefix*>(value);
    if (prefix->magic != NVS_MAGIC ||
        prefix->schema != CFG_NVS_SCHEMA ||
        prefix->payloadSize != length ||
        std::strcmp(prefix->wifi.ssid, WIFI_RESET_SENTINEL) != 0) {
        return value;
    }

    uint8_t* copy = static_cast<uint8_t*>(std::malloc(length));
    if (!copy) {
        Serial.printf("[NVS-DIAG] wifi reset abort malloc failed len=%u\n",
                      (unsigned)length);
        return value;
    }

    std::memcpy(copy, value, length);
    PersistedPrefix* writable = reinterpret_cast<PersistedPrefix*>(copy);
    writable->wifi.ssid[0] = '\0';
    writable->wifi.password[0] = '\0';
    std::memset(writable->wifi.ssid + 1, 0, sizeof(writable->wifi.ssid) - 1U);
    std::memset(writable->wifi.password + 1, 0, sizeof(writable->wifi.password) - 1U);

    const uint32_t crc = crc32Bytes(copy, length - sizeof(uint32_t));
    std::memcpy(copy + length - sizeof(uint32_t), &crc, sizeof(crc));

    ownedCopy = copy;
    Serial.printf("[NVS-DIAG] wifi reset marker applied: SSID/PWD cleared, len=%u\n",
                  (unsigned)length);
    return copy;
}
}

extern "C" esp_err_t __real_nvs_set_blob(nvs_handle_t handle,
                                          const char* key,
                                          const void* value,
                                          size_t length);

extern "C" esp_err_t __wrap_nvs_set_blob(nvs_handle_t handle,
                                          const char* key,
                                          const void* value,
                                          size_t length) {
    void* ownedValue = nullptr;
    const void* valueToWrite = prepareConfigValue(key, value, length, ownedValue);

    const esp_err_t firstErr =
        __real_nvs_set_blob(handle, key, valueToWrite, length);

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
        valueToWrite == nullptr ||
        length < CONFIG_BLOB_MIN_SIZE ||
        std::strcmp(key, CONFIG_KEY) != 0) {
        std::free(ownedValue);
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
            std::free(ownedValue);
            return firstErr;
        }

        previous = std::malloc(previousLength);
        if (previous == nullptr) {
            Serial.printf("[NVS-DIAG] recovery abort malloc failed\n");
            std::free(ownedValue);
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
        std::free(ownedValue);
        return firstErr;
    }

    err = __real_nvs_set_blob(handle, key, valueToWrite, length);
    Serial.printf(
        "[NVS-DIAG] recovery rewrite err=%d len=%u heap=%u\n",
        (int)err,
        (unsigned)length,
        (unsigned)ESP.getFreeHeap()
    );
    if (err == ESP_OK) {
        std::free(previous);
        std::free(ownedValue);
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
    std::free(ownedValue);
    return err;
}
