#pragma once

#include <Arduino.h>
#include "EquipmentModel.h"

namespace AquaLook { namespace Runtime {

enum class EquipmentExecutionMode : uint8_t {
    MODE_DISABLED = 0,
    MODE_SHADOW = 1,
    MODE_PHYSICAL = 2
};

struct ManagedEquipmentConfig {
    bool enabled;
    EquipmentExecutionMode mode;
    uint8_t type;
    uint8_t targetIndex;
    uint8_t relayAssignmentIndex;
    uint16_t startupDelayMs;
    uint16_t shutdownDelayMs;
    uint16_t minOnSec;
    uint16_t minOffSec;
    char name[EquipmentModel::EQUIPMENT_NAME_LENGTH];

    ManagedEquipmentConfig();
};

struct ZoneEquipmentDependencies {
    bool enabled;
    uint8_t zoneIndex;
    uint8_t valveEquipmentIndex;
    uint8_t supportEquipmentIndex;

    ZoneEquipmentDependencies();
};

struct EquipmentAutomationProfile {
    uint16_t schemaVersion;
    ManagedEquipmentConfig equipments[EquipmentModel::MAX_EQUIPMENTS];
    ZoneEquipmentDependencies zoneDependencies[MAX_ZONES];

    EquipmentAutomationProfile();
};

const char* equipmentExecutionModeName(EquipmentExecutionMode mode);
void clearEquipmentAutomationProfile(EquipmentAutomationProfile& profile);
bool validateManagedEquipment(
    const EquipmentAutomationProfile& profile,
    uint8_t equipmentIndex
);
bool validateZoneDependencies(
    const EquipmentAutomationProfile& profile,
    uint8_t dependencyIndex,
    uint8_t nbZones
);
int16_t findManagedEquipment(
    const EquipmentAutomationProfile& profile,
    uint8_t type,
    uint8_t targetIndex
);
bool buildEquipmentModelFromProfile(
    const EquipmentAutomationProfile& profile,
    uint8_t nbZones,
    EquipmentModel::EquipmentConfigSet& model
);

}} // namespace AquaLook::Runtime
