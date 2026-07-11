#include "EquipmentRuntimeConfigStore.h"

#include <Preferences.h>
#include <cstddef>
#include <cstring>

#include "ConfigManager.h"
#include "EventLog.h"

namespace AquaLook { namespace Runtime {
namespace {

constexpr uint32_t EQUIPMENT_CONFIG_MAGIC = 0x45514346UL; // EQCF
constexpr uint16_t EQUIPMENT_CONFIG_STORE_SCHEMA = 1U;
constexpr const char* EQUIPMENT_CONFIG_NVS_KEY = "equipCfg";

struct PersistedEquipmentRuntimeConfig {
    uint32_t magic;
    uint16_t schema;
    uint16_t payloadSize;
    EquipmentRuntimeConfig config;
    uint32_t crc32;
};

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t index = 0U; index < len; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

bool structurallySafe(const EquipmentRuntimeConfig& config) {
    const uint8_t mode = static_cast<uint8_t>(config.pump.mode);
    if (mode > static_cast<uint8_t>(EquipmentControlMode::PHYSICAL)) return false;
    if (config.pump.startupDelayMs > 30000U ||
        config.pump.shutdownDelayMs > 30000U) return false;
    if (config.pump.minOnSec > 3600U || config.pump.minOffSec > 3600U) return false;
    if (config.pump.mode == EquipmentControlMode::PHYSICAL &&
        config.pump.relayAssignmentIndex == EquipmentModel::INVALID_INDEX) {
        return false;
    }
    return true;
}

} // namespace

EquipmentRuntimeConfigStore::EquipmentRuntimeConfigStore()
    : _config(makeSafeDefaultEquipmentRuntimeConfig()),
      _loaded(false),
      _safeDefaults(true),
      _lastStatus("not_started") {}

bool EquipmentRuntimeConfigStore::begin() {
    if (load()) return true;

    applySafeDefaults("missing_or_invalid_safe_defaults");
    if (!save(_config)) {
        EventLog::log(
            LOG_ERROR,
            "Equipment config: defauts surs actifs, persistance impossible"
        );
        return false;
    }

    _safeDefaults = true;
    _lastStatus = "safe_defaults_created";
    EventLog::log(
        LOG_WARN,
        "Equipment config: absente/invalide, defauts surs persistes mode=disabled"
    );
    return true;
}

bool EquipmentRuntimeConfigStore::load() {
    Preferences preferences;
    if (!preferences.begin(CFG_NVS_NAMESPACE, true)) {
        applySafeDefaults("nvs_open_failed");
        return false;
    }

    const size_t storedLength = preferences.getBytesLength(EQUIPMENT_CONFIG_NVS_KEY);
    if (storedLength != sizeof(PersistedEquipmentRuntimeConfig)) {
        preferences.end();
        applySafeDefaults(storedLength == 0U ? "not_found" : "size_mismatch");
        return false;
    }

    PersistedEquipmentRuntimeConfig blob{};
    const size_t bytesRead = preferences.getBytes(
        EQUIPMENT_CONFIG_NVS_KEY,
        &blob,
        sizeof(blob)
    );
    preferences.end();

    bool valid = bytesRead == sizeof(blob) &&
                 blob.magic == EQUIPMENT_CONFIG_MAGIC &&
                 blob.schema == EQUIPMENT_CONFIG_STORE_SCHEMA &&
                 blob.payloadSize == sizeof(blob);

    if (valid) {
        const uint32_t expectedCrc = crc32Bytes(
            reinterpret_cast<const uint8_t*>(&blob),
            offsetof(PersistedEquipmentRuntimeConfig, crc32)
        );
        valid = expectedCrc == blob.crc32;
    }

    if (valid) valid = structurallySafe(blob.config);

    if (!valid) {
        applySafeDefaults("header_crc_or_payload_invalid");
        EventLog::log(LOG_ERROR, "Equipment config: bloc NVS invalide, mode sur disabled");
        return false;
    }

    _config = blob.config;
    _loaded = true;
    _safeDefaults = false;
    _lastStatus = "loaded";

    EventLog::log(
        LOG_INFO,
        "Equipment config: chargee mode=%s enabled=%s assignment=%u delays=%u/%u",
        equipmentControlModeName(_config.pump.mode),
        _config.pump.enabled ? "yes" : "no",
        static_cast<unsigned>(_config.pump.relayAssignmentIndex),
        static_cast<unsigned>(_config.pump.startupDelayMs),
        static_cast<unsigned>(_config.pump.shutdownDelayMs)
    );
    return true;
}

bool EquipmentRuntimeConfigStore::save(const EquipmentRuntimeConfig& config) {
    if (!structurallySafe(config)) {
        _lastStatus = "save_rejected_invalid";
        EventLog::log(LOG_WARN, "Equipment config: sauvegarde refusee, configuration invalide");
        return false;
    }

    PersistedEquipmentRuntimeConfig blob{};
    blob.magic = EQUIPMENT_CONFIG_MAGIC;
    blob.schema = EQUIPMENT_CONFIG_STORE_SCHEMA;
    blob.payloadSize = sizeof(blob);
    blob.config = config;
    blob.crc32 = crc32Bytes(
        reinterpret_cast<const uint8_t*>(&blob),
        offsetof(PersistedEquipmentRuntimeConfig, crc32)
    );

    Preferences preferences;
    if (!preferences.begin(CFG_NVS_NAMESPACE, false)) {
        _lastStatus = "nvs_open_write_failed";
        return false;
    }

    const size_t written = preferences.putBytes(
        EQUIPMENT_CONFIG_NVS_KEY,
        &blob,
        sizeof(blob)
    );
    preferences.end();

    if (written != sizeof(blob)) {
        _lastStatus = "nvs_write_incomplete";
        EventLog::log(
            LOG_ERROR,
            "Equipment config: ecriture NVS incomplete (%u/%u)",
            static_cast<unsigned>(written),
            static_cast<unsigned>(sizeof(blob))
        );
        return false;
    }

    _config = config;
    _loaded = true;
    _safeDefaults = false;
    _lastStatus = "saved";
    EventLog::log(
        LOG_INFO,
        "Equipment config: sauvegarde NVS OK (%u octets, schema %u)",
        static_cast<unsigned>(written),
        static_cast<unsigned>(EQUIPMENT_CONFIG_STORE_SCHEMA)
    );
    return true;
}

bool EquipmentRuntimeConfigStore::reset() {
    Preferences preferences;
    if (!preferences.begin(CFG_NVS_NAMESPACE, false)) {
        _lastStatus = "reset_nvs_open_failed";
        return false;
    }

    const bool removed = preferences.remove(EQUIPMENT_CONFIG_NVS_KEY);
    preferences.end();
    applySafeDefaults(removed ? "reset" : "reset_key_absent");

    EventLog::log(
        removed ? LOG_INFO : LOG_WARN,
        removed
            ? "Equipment config: cle NVS effacee, mode disabled"
            : "Equipment config: cle NVS absente, mode disabled"
    );
    return removed;
}

const EquipmentRuntimeConfig& EquipmentRuntimeConfigStore::config() const {
    return _config;
}

bool EquipmentRuntimeConfigStore::isLoaded() const {
    return _loaded;
}

bool EquipmentRuntimeConfigStore::usedSafeDefaults() const {
    return _safeDefaults;
}

const char* EquipmentRuntimeConfigStore::lastStatus() const {
    return _lastStatus;
}

void EquipmentRuntimeConfigStore::applySafeDefaults(const char* status) {
    _config = makeSafeDefaultEquipmentRuntimeConfig();
    _loaded = false;
    _safeDefaults = true;
    _lastStatus = status;
}

}} // namespace AquaLook::Runtime
