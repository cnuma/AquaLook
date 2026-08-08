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

    if (firstErr != ESP_ERR_NVS_NOT_ENOUGH_SPACE ||
        key == nullptr ||
        value == nullptr ||
        length < CONFIG_BLOB_MIN_SIZE ||
        std::strcmp(key, CONFIG_KEY) != 0) {
        return firstErr;
    }

    // Le remplacement d'un gros blob peut manquer d'espace temporaire car NVS
    // conserve l'ancienne valeur jusqu'au commit. Sauvegarder d'abord l'ancien
    // blob en RAM afin que la reprise reste reversible lorsqu'il est lisible.
    void* previous = nullptr;
    size_t previousLength = 0U;
    esp_err_t previousErr = nvs_get_blob(handle, key, nullptr, &previousLength);
    bool previousUnreadable = false;

    if (previousErr == ESP_OK && previousLength > 0U) {
        if (previousLength > CONFIG_BACKUP_MAX_SIZE) {
            return firstErr;
        }

        previous = std::malloc(previousLength);
        if (previous == nullptr) {
            return firstErr;
        }

        size_t readLength = previousLength;
        previousErr = nvs_get_blob(handle, key, previous, &readLength);
        if (previousErr != ESP_OK || readLength != previousLength) {
            std::free(previous);
            previous = nullptr;
            previousLength = 0U;
            previousUnreadable = true;
        }
    } else if (previousErr != ESP_ERR_NVS_NOT_FOUND) {
        // La cle existe mais NVS ne sait deja plus la relire. Il n'existe alors
        // aucune configuration valide a preserver. Autoriser sa reconstruction
        // depuis le nouveau blob complet fourni par ConfigManager.
        previousUnreadable = true;
    }

    esp_err_t err = nvs_erase_key(handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        std::free(previous);
        return firstErr;
    }

    err = __real_nvs_set_blob(handle, key, value, length);
    if (err == ESP_OK) {
        std::free(previous);
        return ESP_OK;
    }

    // La reprise a echoue : remettre l'ancien bloc lorsqu'il etait lisible.
    // Une cle deja illisible ne peut pas etre restauree et reste volontairement
    // effacee plutot que de recreer une configuration corrompue.
    if (!previousUnreadable && previous != nullptr && previousLength > 0U) {
        (void)__real_nvs_set_blob(handle, key, previous, previousLength);
    }

    std::free(previous);
    return err;
}
