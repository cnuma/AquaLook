#include "OtaBootGuard.h"
#include "BootLoopGuard.h"

#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <cstring>

#include "EventLog.h"

namespace {
constexpr char NVS_NAMESPACE[] = "aq_ota_guard";
constexpr uint8_t MAX_BOOT_ATTEMPTS = 3U;
constexpr uint32_t VALIDATION_DELAY_MS = 45000UL;

void copyLabel(char* destination, size_t destinationSize, const char* source) {
    if (destinationSize == 0U) return;
    std::strncpy(destination, source ? source : "", destinationSize - 1U);
    destination[destinationSize - 1U] = '\0';
}

const esp_partition_t* findPartitionByLabel(const char* label) {
    if (!label || !label[0]) return nullptr;
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
}
}

bool OtaBootGuard::_pendingValidation = false;
uint32_t OtaBootGuard::_bootMs = 0U;

bool OtaBootGuard::arm(const char* previousPartitionLabel, const char* targetPartitionLabel) {
    char previousLabel[17];
    char targetLabel[17];
    copyLabel(previousLabel, sizeof(previousLabel), previousPartitionLabel);
    copyLabel(targetLabel, sizeof(targetLabel), targetPartitionLabel);
    if (previousLabel[0] == '\0' || targetLabel[0] == '\0') return false;

    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return false;

    bool ok = true;
    ok = preferences.putBool("pending", true) && ok;
    ok = (preferences.putUChar("attempts", 0U) == sizeof(uint8_t)) && ok;
    preferences.putString("prev", previousLabel);
    preferences.putString("target", targetLabel);
    preferences.end();

    EventLog::log(LOG_WARN,
                  "OTA garde: armee, precedente=%s cible=%s attempts=0/%u delaiValidationMs=%lu",
                  previousLabel, targetLabel,
                  static_cast<unsigned>(MAX_BOOT_ATTEMPTS),
                  static_cast<unsigned long>(VALIDATION_DELAY_MS));
    return ok;
}

void OtaBootGuard::onBoot() {
    _bootMs = millis();
    _pendingValidation = false;

    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return;

    const bool pending = preferences.getBool("pending", false);
    if (!pending) {
        preferences.end();
        return;
    }

    char previousLabel[17] = "";
    preferences.getString("prev", previousLabel, sizeof(previousLabel));
    char targetLabel[17] = "";
    preferences.getString("target", targetLabel, sizeof(targetLabel));

    const uint8_t attempts = preferences.getUChar("attempts", 0U) + 1U;

    if (attempts > MAX_BOOT_ATTEMPTS) {
        preferences.putBool("pending", false);
        preferences.putUChar("attempts", 0U);
        preferences.end();

        EventLog::log(LOG_ERROR,
                      "OTA garde: %u redemarrage(s) sans validation sur %s, retour vers %s",
                      static_cast<unsigned>(attempts), targetLabel, previousLabel);

        const esp_partition_t* fallback = findPartitionByLabel(previousLabel);
        if (fallback) {
            esp_ota_set_boot_partition(fallback);
        } else {
            EventLog::log(LOG_ERROR,
                          "OTA garde: partition precedente %s introuvable, retour arriere impossible",
                          previousLabel);
        }
        delay(250);
        BootLoopGuard::restartDeliberately("retour arriere OTA");
        return; // jamais atteint
    }

    preferences.putUChar("attempts", attempts);
    preferences.end();

    _pendingValidation = true;
    EventLog::log(LOG_WARN,
                  "OTA garde: demarrage sur %s en attente de validation, tentative %u/%u",
                  targetLabel, static_cast<unsigned>(attempts),
                  static_cast<unsigned>(MAX_BOOT_ATTEMPTS));
}

void OtaBootGuard::update() {
    if (!_pendingValidation) return;
    if (millis() - _bootMs < VALIDATION_DELAY_MS) return;

    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return;
    preferences.putBool("pending", false);
    preferences.putUChar("attempts", 0U);
    preferences.end();

    _pendingValidation = false;
    EventLog::log(LOG_INFO,
                  "OTA garde: firmware valide apres %lu ms de fonctionnement stable, retour arriere desarme",
                  static_cast<unsigned long>(millis() - _bootMs));
}
