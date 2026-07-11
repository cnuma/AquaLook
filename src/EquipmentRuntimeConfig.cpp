#include "EquipmentRuntimeConfig.h"

namespace AquaLook { namespace Runtime {

static constexpr uint16_t EQUIPMENT_RUNTIME_CONFIG_SCHEMA = 1U;
static constexpr uint16_t MAX_TRANSITION_DELAY_MS = 30000U;
static constexpr uint16_t MAX_MIN_STATE_SEC = 3600U;

PumpRuntimeConfig::PumpRuntimeConfig()
    : enabled(false),
      mode(EquipmentControlMode::DISABLED),
      targetIndex(0U),
      relayAssignmentIndex(EquipmentModel::INVALID_INDEX),
      startupDelayMs(500U),
      shutdownDelayMs(500U),
      minOnSec(0U),
      minOffSec(0U) {}

EquipmentRuntimeConfig::EquipmentRuntimeConfig()
    : schemaVersion(EQUIPMENT_RUNTIME_CONFIG_SCHEMA), pump() {}

EquipmentRuntimeConfigValidation::EquipmentRuntimeConfigValidation()
    : valid(false), physicalActivationAllowed(false), reason("unvalidated") {}

const char* equipmentControlModeName(EquipmentControlMode mode) {
    switch (mode) {
        case EquipmentControlMode::DISABLED: return "disabled";
        case EquipmentControlMode::SHADOW: return "shadow";
        case EquipmentControlMode::PHYSICAL: return "physical";
        default: return "invalid";
    }
}

EquipmentRuntimeConfig makeSafeDefaultEquipmentRuntimeConfig() {
    EquipmentRuntimeConfig config;
    config.schemaVersion = EQUIPMENT_RUNTIME_CONFIG_SCHEMA;
    config.pump.enabled = false;
    config.pump.mode = EquipmentControlMode::DISABLED;
    config.pump.targetIndex = 0U;
    config.pump.relayAssignmentIndex = EquipmentModel::INVALID_INDEX;
    config.pump.startupDelayMs = 500U;
    config.pump.shutdownDelayMs = 500U;
    config.pump.minOnSec = 0U;
    config.pump.minOffSec = 0U;
    return config;
}

EquipmentRuntimeConfigValidation validateEquipmentRuntimeConfig(
    const EquipmentRuntimeConfig& config,
    const RelayTopology::RelayTopologyConfig& topology
) {
    EquipmentRuntimeConfigValidation result;

    if (config.schemaVersion != EQUIPMENT_RUNTIME_CONFIG_SCHEMA) {
        result.reason = "unsupported_schema";
        return result;
    }

    const uint8_t modeValue = static_cast<uint8_t>(config.pump.mode);
    if (modeValue > static_cast<uint8_t>(EquipmentControlMode::PHYSICAL)) {
        result.reason = "invalid_pump_mode";
        return result;
    }

    if (config.pump.startupDelayMs > MAX_TRANSITION_DELAY_MS ||
        config.pump.shutdownDelayMs > MAX_TRANSITION_DELAY_MS) {
        result.reason = "pump_delay_out_of_range";
        return result;
    }

    if (config.pump.minOnSec > MAX_MIN_STATE_SEC ||
        config.pump.minOffSec > MAX_MIN_STATE_SEC) {
        result.reason = "pump_min_state_out_of_range";
        return result;
    }

    if (!config.pump.enabled || config.pump.mode == EquipmentControlMode::DISABLED) {
        result.valid = true;
        result.physicalActivationAllowed = false;
        result.reason = "pump_disabled";
        return result;
    }

    if (config.pump.mode == EquipmentControlMode::SHADOW) {
        result.valid = true;
        result.physicalActivationAllowed = false;
        result.reason = "shadow_only";
        return result;
    }

    if (config.pump.relayAssignmentIndex == EquipmentModel::INVALID_INDEX ||
        config.pump.relayAssignmentIndex >= RelayTopology::MAX_RELAY_ASSIGNMENTS) {
        result.reason = "missing_pump_assignment";
        return result;
    }

    if (!RelayTopology::validateAssignment(
            topology,
            config.pump.relayAssignmentIndex)) {
        result.reason = "invalid_pump_assignment";
        return result;
    }

    const RelayTopology::RelayAssignment& assignment =
        topology.assignments[config.pump.relayAssignmentIndex];
    if (assignment.role != RelayTopology::ROLE_PUMP ||
        assignment.targetIndex != config.pump.targetIndex) {
        result.reason = "assignment_role_mismatch";
        return result;
    }

    result.valid = true;
    result.physicalActivationAllowed = true;
    result.reason = "physical_ready";
    return result;
}

bool applyPumpConfigToModel(
    const EquipmentRuntimeConfig& config,
    EquipmentModel::EquipmentConfigSet& model,
    uint8_t nbZones,
    uint8_t pumpEquipmentIndex
) {
    if (!config.pump.enabled ||
        config.pump.mode == EquipmentControlMode::DISABLED) {
        for (uint8_t zone = 0U; zone < nbZones && zone < MAX_ZONES; ++zone) {
            model.zoneLinks[zone].pumpEquipmentIndex = EquipmentModel::INVALID_INDEX;
        }
        return true;
    }

    if (pumpEquipmentIndex >= EquipmentModel::MAX_EQUIPMENTS ||
        config.pump.relayAssignmentIndex == EquipmentModel::INVALID_INDEX) {
        return false;
    }

    EquipmentModel::EquipmentConfig& pump = model.equipments[pumpEquipmentIndex];
    pump.enabled = true;
    pump.type = EquipmentModel::EQUIP_PUMP;
    pump.targetIndex = config.pump.targetIndex;
    pump.relayAssignmentIndex = config.pump.relayAssignmentIndex;
    pump.startupDelayMs = config.pump.startupDelayMs;
    pump.shutdownDelayMs = config.pump.shutdownDelayMs;
    pump.minOnSec = config.pump.minOnSec;
    pump.minOffSec = config.pump.minOffSec;
    snprintf(pump.name, sizeof(pump.name), "Pompe %u", config.pump.targetIndex + 1U);

    for (uint8_t zone = 0U; zone < nbZones && zone < MAX_ZONES; ++zone) {
        model.zoneLinks[zone].pumpEquipmentIndex = pumpEquipmentIndex;
    }
    return true;
}

}} // namespace AquaLook::Runtime
