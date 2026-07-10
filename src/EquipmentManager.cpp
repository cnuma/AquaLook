#include "EquipmentManager.h"
#include "EquipmentOutputRuntimeAdapter.h"
#include "RelaisManager.h"

EquipmentManager::EquipmentResolution::EquipmentResolution()
    : result(ACTION_NOT_INITIALIZED),
      equipmentIndex(EquipmentModel::INVALID_INDEX),
      relayAssignmentIndex(EquipmentModel::INVALID_INDEX),
      relay() {}

bool EquipmentManager::EquipmentResolution::valid() const {
    return result == ACTION_OK && relay.valid;
}

EquipmentManager::ZoneDependencyResolution::ZoneDependencyResolution()
    : result(ACTION_NOT_INITIALIZED),
      linkIndex(EquipmentModel::INVALID_INDEX),
      valve(),
      requiresPump(false),
      pump() {}

bool EquipmentManager::ZoneDependencyResolution::valid() const {
    if (result != ACTION_OK || !valve.valid()) return false;
    return !requiresPump || pump.valid();
}

void EquipmentManager::begin(
    const EquipmentModel::EquipmentConfigSet* equipmentModel,
    const RelayTopology::RelayTopologyConfig* relayTopology,
    uint8_t nbZones,
    RelaisManager* relayExecutor
) {
    _equipmentModel = equipmentModel;
    _relayTopology = relayTopology;
    _relayExecutor = relayExecutor;
    _nbZones = constrain(nbZones, (uint8_t)0, (uint8_t)MAX_ZONES);
}

void EquipmentManager::setRelayExecutor(RelaisManager* relayExecutor) {
    _relayExecutor = relayExecutor;
}

void EquipmentManager::setOutputAdapter(
    AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputAdapter
) {
    _outputAdapter = outputAdapter;
}

bool EquipmentManager::isInitialized() const {
    return _equipmentModel != nullptr && _relayTopology != nullptr;
}

bool EquipmentManager::hasRelayExecutor() const {
    return _relayExecutor != nullptr;
}

bool EquipmentManager::hasOutputAdapter() const {
    return _outputAdapter != nullptr && _outputAdapter->isBound();
}

bool EquipmentManager::hasExecutor() const {
    return hasOutputAdapter() || hasRelayExecutor();
}

EquipmentManager::ActionResult EquipmentManager::resolveEquipment(
    uint8_t equipmentIndex,
    EquipmentResolution& resolution
) const {
    if (!isInitialized()) return ACTION_NOT_INITIALIZED;
    if (!EquipmentModel::validateEquipment(*_equipmentModel, equipmentIndex)) {
        return ACTION_INVALID_EQUIPMENT;
    }

    const EquipmentModel::EquipmentConfig& equipment =
        _equipmentModel->equipments[equipmentIndex];

    resolution.equipmentIndex = equipmentIndex;
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

EquipmentManager::ActionResult EquipmentManager::validateZoneRequest(
    uint8_t zone,
    ZoneResolution& resolution
) const {
    if (!isInitialized()) return ACTION_NOT_INITIALIZED;
    if (zone >= _nbZones || zone >= MAX_ZONES) return ACTION_INVALID_ZONE;

    const int16_t linkIndex = EquipmentModel::findZoneLink(
        *_equipmentModel,
        zone,
        _nbZones
    );

    if (linkIndex >= 0) {
        const EquipmentModel::ZoneEquipmentLink& link =
            _equipmentModel->zoneLinks[linkIndex];
        return resolveEquipment(link.valveEquipmentIndex, resolution);
    }

    // Compatibilité transitoire avec les modèles créés avant ZoneEquipmentLink.
    const int16_t equipmentIndex = EquipmentModel::findByTypeAndTarget(
        *_equipmentModel,
        EquipmentModel::EQUIP_ZONE_VALVE,
        zone
    );
    if (equipmentIndex < 0) return ACTION_EQUIPMENT_NOT_FOUND;

    return resolveEquipment(static_cast<uint8_t>(equipmentIndex), resolution);
}

EquipmentManager::ZoneResolution EquipmentManager::resolveZone(uint8_t zone) const {
    ZoneResolution resolution;
    resolution.result = validateZoneRequest(zone, resolution);
    return resolution;
}

EquipmentManager::ZoneDependencyResolution
EquipmentManager::resolveZoneDependencies(uint8_t zone) const {
    ZoneDependencyResolution resolution;

    if (!isInitialized()) {
        resolution.result = ACTION_NOT_INITIALIZED;
        return resolution;
    }
    if (zone >= _nbZones || zone >= MAX_ZONES) {
        resolution.result = ACTION_INVALID_ZONE;
        return resolution;
    }

    const int16_t linkIndex = EquipmentModel::findZoneLink(
        *_equipmentModel,
        zone,
        _nbZones
    );
    if (linkIndex < 0) {
        resolution.result = ACTION_ZONE_LINK_NOT_FOUND;
        return resolution;
    }

    resolution.linkIndex = static_cast<uint8_t>(linkIndex);
    const EquipmentModel::ZoneEquipmentLink& link =
        _equipmentModel->zoneLinks[resolution.linkIndex];

    resolution.valve.result = resolveEquipment(
        link.valveEquipmentIndex,
        resolution.valve
    );
    if (resolution.valve.result != ACTION_OK) {
        resolution.result = resolution.valve.result;
        return resolution;
    }

    resolution.requiresPump = link.requiresPump();
    if (resolution.requiresPump) {
        resolution.pump.result = resolveEquipment(
            link.pumpEquipmentIndex,
            resolution.pump
        );
        if (resolution.pump.result != ACTION_OK) {
            resolution.result = resolution.pump.result;
            return resolution;
        }
    }

    resolution.result = ACTION_OK;
    return resolution;
}

EquipmentManager::ZoneExecutionPlan EquipmentManager::buildZonePlan(
    uint8_t zone,
    bool starting
) const {
    ZoneExecutionPlan plan;
    plan.zone = zone;

    const ZoneDependencyResolution dependencies = resolveZoneDependencies(zone);
    plan.result = dependencies.result;
    if (!dependencies.valid()) return plan;

    plan.requiresPump = dependencies.requiresPump;

    if (!dependencies.requiresPump) {
        plan.steps[0] = PlanStep(
            starting ? PLAN_ACTION_VALVE_ON : PLAN_ACTION_VALVE_OFF,
            dependencies.valve.equipmentIndex
        );
        plan.stepCount = 1U;
        plan.result = ACTION_OK;
        return plan;
    }

    const EquipmentModel::EquipmentConfig& pump =
        _equipmentModel->equipments[dependencies.pump.equipmentIndex];

    if (starting) {
        plan.steps[plan.stepCount++] = PlanStep(
            PLAN_ACTION_VALVE_ON,
            dependencies.valve.equipmentIndex
        );
        if (pump.startupDelayMs > 0U) {
            plan.steps[plan.stepCount++] = PlanStep(
                PLAN_ACTION_WAIT,
                EquipmentModel::INVALID_INDEX,
                pump.startupDelayMs
            );
        }
        plan.steps[plan.stepCount++] = PlanStep(
            PLAN_ACTION_PUMP_ON,
            dependencies.pump.equipmentIndex
        );
    } else {
        plan.steps[plan.stepCount++] = PlanStep(
            PLAN_ACTION_PUMP_OFF,
            dependencies.pump.equipmentIndex
        );
        if (pump.shutdownDelayMs > 0U) {
            plan.steps[plan.stepCount++] = PlanStep(
                PLAN_ACTION_WAIT,
                EquipmentModel::INVALID_INDEX,
                pump.shutdownDelayMs
            );
        }
        plan.steps[plan.stepCount++] = PlanStep(
            PLAN_ACTION_VALVE_OFF,
            dependencies.valve.equipmentIndex
        );
    }

    plan.result = ACTION_OK;
    return plan;
}

EquipmentManager::ZoneExecutionPlan
EquipmentManager::buildZoneStartPlan(uint8_t zone) const {
    return buildZonePlan(zone, true);
}

EquipmentManager::ZoneExecutionPlan
EquipmentManager::buildZoneStopPlan(uint8_t zone) const {
    return buildZonePlan(zone, false);
}

EquipmentManager::ActionResult EquipmentManager::executeZone(uint8_t zone, bool state) {
    ZoneResolution resolution;
    const ActionResult validation = validateZoneRequest(zone, resolution);
    if (validation != ACTION_OK) return validation;
    if (!hasExecutor()) return ACTION_EXECUTOR_NOT_CONNECTED;

    if (hasOutputAdapter()) {
        const AquaLook::Domain::OperationResult result =
            _outputAdapter->setZoneValve(zone, state, millis());
        if (result.succeeded()) return ACTION_OK;
    }

    if (hasRelayExecutor()) {
        return _relayExecutor->setAssignment(resolution.relayAssignmentIndex, state)
            ? ACTION_OK
            : ACTION_EXECUTION_FAILED;
    }

    return ACTION_EXECUTION_FAILED;
}

EquipmentManager::ActionResult EquipmentManager::startZone(uint8_t zone) {
    return executeZone(zone, true);
}

EquipmentManager::ActionResult EquipmentManager::stopZone(uint8_t zone) {
    return executeZone(zone, false);
}
