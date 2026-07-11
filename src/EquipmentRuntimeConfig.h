#pragma once

#include <Arduino.h>
#include "EquipmentModel.h"
#include "RelayTopology.h"

namespace AquaLook { namespace Runtime {

enum class EquipmentControlMode : uint8_t {
    DISABLED = 0,
    SHADOW = 1,
    PHYSICAL = 2
};

struct PumpRuntimeConfig {
    bool enabled;
    EquipmentControlMode mode;
    uint8_t targetIndex;
    uint8_t relayAssignmentIndex;
    uint16_t startupDelayMs;
    uint16_t shutdownDelayMs;
    uint16_t minOnSec;
    uint16_t minOffSec;

    PumpRuntimeConfig();
};

struct EquipmentRuntimeConfig {
    uint16_t schemaVersion;
    PumpRuntimeConfig pump;

    EquipmentRuntimeConfig();
};

struct EquipmentRuntimeConfigValidation {
    bool valid;
    bool physicalActivationAllowed;
    const char* reason;

    EquipmentRuntimeConfigValidation();
};

const char* equipmentControlModeName(EquipmentControlMode mode);

EquipmentRuntimeConfig makeSafeDefaultEquipmentRuntimeConfig();

EquipmentRuntimeConfigValidation validateEquipmentRuntimeConfig(
    const EquipmentRuntimeConfig& config,
    const RelayTopology::RelayTopologyConfig& topology
);

bool applyPumpConfigToModel(
    const EquipmentRuntimeConfig& config,
    EquipmentModel::EquipmentConfigSet& model,
    uint8_t nbZones,
    uint8_t pumpEquipmentIndex
);

}} // namespace AquaLook::Runtime
