#include "EquipmentModel.h"

namespace EquipmentModel {

EquipmentConfig::EquipmentConfig()
    : enabled(false),
      type(EQUIP_UNUSED),
      targetIndex(0),
      name{},
      relayAssignmentIndex(INVALID_INDEX),
      startupDelayMs(0),
      shutdownDelayMs(0),
      minOnSec(0),
      minOffSec(0) {}

ZoneEquipmentLink::ZoneEquipmentLink()
    : enabled(false),
      zoneIndex(0),
      valveEquipmentIndex(INVALID_INDEX),
      pumpEquipmentIndex(INVALID_INDEX) {}

bool ZoneEquipmentLink::requiresPump() const {
    return pumpEquipmentIndex != INVALID_INDEX;
}

const char* typeName(uint8_t type) {
    switch (type) {
        case EQUIP_UNUSED: return "unused";
        case EQUIP_ZONE_VALVE: return "zone_valve";
        case EQUIP_PUMP: return "pump";
        case EQUIP_AUX_CONTACT: return "aux_contact";
        case EQUIP_GREENHOUSE_VENT: return "greenhouse_vent";
        case EQUIP_LIGHTING: return "lighting";
        case EQUIP_MISTER: return "mister";
        case EQUIP_FAN: return "fan";
        default: return "unknown";
    }
}

bool isSupportedType(uint8_t type) {
    return type <= EQUIP_FAN;
}

bool typeUsesRelay(uint8_t type) {
    return isSupportedType(type) && type != EQUIP_UNUSED;
}

uint8_t expectedRelayRole(uint8_t type) {
    switch (type) {
        case EQUIP_ZONE_VALVE: return RelayTopology::ROLE_ZONE_VALVE;
        case EQUIP_PUMP: return RelayTopology::ROLE_PUMP;
        case EQUIP_AUX_CONTACT:
        case EQUIP_MISTER:
        case EQUIP_FAN:
            return RelayTopology::ROLE_AUX;
        case EQUIP_GREENHOUSE_VENT: return RelayTopology::ROLE_GREENHOUSE_VENT;
        case EQUIP_LIGHTING: return RelayTopology::ROLE_LIGHTING;
        case EQUIP_UNUSED:
        default:
            return RelayTopology::ROLE_UNUSED;
    }
}

void clear(EquipmentConfigSet& model) {
    for (uint8_t i = 0; i < MAX_EQUIPMENTS; ++i) {
        model.equipments[i] = EquipmentConfig{};
    }
    for (uint8_t z = 0; z < MAX_ZONES; ++z) {
        model.zoneLinks[z] = ZoneEquipmentLink{};
    }
}

bool validateEquipment(const EquipmentConfigSet& model, uint8_t equipmentIndex) {
    if (equipmentIndex >= MAX_EQUIPMENTS) return false;

    const EquipmentConfig& equipment = model.equipments[equipmentIndex];
    if (!equipment.enabled) return false;
    if (!isSupportedType(equipment.type) || equipment.type == EQUIP_UNUSED) return false;
    if (equipment.relayAssignmentIndex >= RelayTopology::MAX_RELAY_ASSIGNMENTS) return false;
    if (equipment.name[EQUIPMENT_NAME_LENGTH - 1] != '\0') return false;

    return true;
}

bool validateZoneLink(
    const EquipmentConfigSet& model,
    uint8_t linkIndex,
    uint8_t nbZones
) {
    if (linkIndex >= MAX_ZONES) return false;

    const ZoneEquipmentLink& link = model.zoneLinks[linkIndex];
    if (!link.enabled) return false;
    if (link.zoneIndex >= nbZones || link.zoneIndex >= MAX_ZONES) return false;
    if (!validateEquipment(model, link.valveEquipmentIndex)) return false;

    const EquipmentConfig& valve = model.equipments[link.valveEquipmentIndex];
    if (valve.type != EQUIP_ZONE_VALVE || valve.targetIndex != link.zoneIndex) {
        return false;
    }

    if (!link.requiresPump()) return true;
    if (!validateEquipment(model, link.pumpEquipmentIndex)) return false;

    const EquipmentConfig& pump = model.equipments[link.pumpEquipmentIndex];
    return pump.type == EQUIP_PUMP;
}

int16_t findByTypeAndTarget(
    const EquipmentConfigSet& model,
    uint8_t type,
    uint8_t targetIndex
) {
    if (!isSupportedType(type) || type == EQUIP_UNUSED) return -1;

    for (uint8_t i = 0; i < MAX_EQUIPMENTS; ++i) {
        if (!validateEquipment(model, i)) continue;

        const EquipmentConfig& equipment = model.equipments[i];
        if (equipment.type == type && equipment.targetIndex == targetIndex) {
            return i;
        }
    }

    return -1;
}

int16_t findZoneLink(
    const EquipmentConfigSet& model,
    uint8_t zoneIndex,
    uint8_t nbZones
) {
    if (zoneIndex >= nbZones || zoneIndex >= MAX_ZONES) return -1;

    for (uint8_t i = 0; i < MAX_ZONES; ++i) {
        if (!validateZoneLink(model, i, nbZones)) continue;
        if (model.zoneLinks[i].zoneIndex == zoneIndex) return i;
    }

    return -1;
}

} // namespace EquipmentModel
