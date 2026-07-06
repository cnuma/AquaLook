#include "EquipmentManager.h"

EquipmentManager::ZoneResolution::ZoneResolution()
    : result(ACTION_NOT_INITIALIZED),
      equipmentIndex(EquipmentModel::INVALID_INDEX),
      relayAssignmentIndex(EquipmentModel::INVALID_INDEX),
      relay() {}

bool EquipmentManager::ZoneResolution::valid() const {
    return result == ACTION_OK && relay.valid;
}

void EquipmentManager::begin(
    const EquipmentModel::EquipmentConfigSet* equipmentModel,
    const RelayTopology::RelayTopologyConfig* relayTopology,
    uint8_t nbZones
) {
    _equipmentModel = equipmentModel;
    _relayTopology = relayTopology;
    _nbZones = constrain(nbZones, (uint8_t)0, (uint8_t)MAX_ZONES);
}

bool EquipmentManager::isInitialized() const {
    return _equipmentModel != nullptr && _relayTopology != nullptr;
}

EquipmentManager::ActionResult EquipmentManager::validateZoneRequest(
    uint8_t zone,
    ZoneResolution& resolution
) const {
    if (!isInitialized()) return ACTION_NOT_INITIALIZED;
    if (zone >= _nbZones || zone >= MAX_ZONES) return ACTION_INVALID_ZONE;

    const int16_t equipmentIndex = EquipmentModel::findByTypeAndTarget(
        *_equipmentModel,
        EquipmentModel::EQUIP_ZONE_VALVE,
        zone
    );
    if (equipmentIndex < 0) return ACTION_EQUIPMENT_NOT_FOUND;

    resolution.equipmentIndex = static_cast<uint8_t>(equipmentIndex);
    if (!EquipmentModel::validateEquipment(*_equipmentModel, resolution.equipmentIndex)) {
        return ACTION_INVALID_EQUIPMENT;
    }

    const EquipmentModel::EquipmentConfig& equipment =
        _equipmentModel->equipments[resolution.equipmentIndex];
    resolution.relayAssignmentIndex = equipment.relayAssignmentIndex;
    resolution.relay = RelayTopology::resolveAssignment(
        *_relayTopology,
        resolution.relayAssignmentIndex
    );

    if (!resolution.relay.valid) return ACTION_RELAY_MAPPING_NOT_FOUND;

    const uint8_t expectedRole = EquipmentModel::expectedRelayRole(equipment.type);
    if (resolution.relay.role != expectedRole ||
        resolution.relay.targetIndex != equipment.targetIndex) {
        return ACTION_RELAY_ROLE_MISMATCH;
    }

    return ACTION_OK;
}

EquipmentManager::ZoneResolution EquipmentManager::resolveZone(uint8_t zone) const {
    ZoneResolution resolution;
    resolution.result = validateZoneRequest(zone, resolution);
    return resolution;
}

EquipmentManager::ActionResult EquipmentManager::startZone(uint8_t zone) {
    ZoneResolution resolution;
    const ActionResult validation = validateZoneRequest(zone, resolution);
    if (validation != ACTION_OK) return validation;

    return ACTION_EXECUTOR_NOT_CONNECTED;
}

EquipmentManager::ActionResult EquipmentManager::stopZone(uint8_t zone) {
    ZoneResolution resolution;
    const ActionResult validation = validateZoneRequest(zone, resolution);
    if (validation != ACTION_OK) return validation;

    return ACTION_EXECUTOR_NOT_CONNECTED;
}
