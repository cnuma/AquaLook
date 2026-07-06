#pragma once

#include <Arduino.h>
#include "RelayTopology.h"

// Modèle métier des équipements AquaLook.
// Cette couche décrit les actionneurs logiques sans piloter le matériel,
// sans dépendre de ConfigManager et sans modifier la persistance NVS.
namespace EquipmentModel {

static constexpr uint8_t MAX_EQUIPMENTS = RelayTopology::MAX_RELAY_ASSIGNMENTS;
static constexpr uint8_t INVALID_INDEX = 0xFF;
static constexpr size_t EQUIPMENT_NAME_LENGTH = 24;

enum EquipmentType : uint8_t {
    EQUIP_UNUSED = 0,
    EQUIP_ZONE_VALVE = 1,
    EQUIP_PUMP = 2,
    EQUIP_AUX_CONTACT = 3,
    EQUIP_GREENHOUSE_VENT = 4,
    EQUIP_LIGHTING = 5,
    EQUIP_MISTER = 6,
    EQUIP_FAN = 7
};

struct EquipmentConfig {
    bool enabled;
    uint8_t type;
    uint8_t targetIndex;
    char name[EQUIPMENT_NAME_LENGTH];
    uint8_t relayAssignmentIndex;
    uint16_t startupDelayMs;
    uint16_t shutdownDelayMs;
    uint16_t minOnSec;
    uint16_t minOffSec;

    EquipmentConfig();
};

struct EquipmentConfigSet {
    EquipmentConfig equipments[MAX_EQUIPMENTS];
};

const char* typeName(uint8_t type);
bool isSupportedType(uint8_t type);
bool typeUsesRelay(uint8_t type);
uint8_t expectedRelayRole(uint8_t type);

void clear(EquipmentConfigSet& model);
bool validateEquipment(const EquipmentConfigSet& model, uint8_t equipmentIndex);
int16_t findByTypeAndTarget(
    const EquipmentConfigSet& model,
    uint8_t type,
    uint8_t targetIndex
);

} // namespace EquipmentModel
