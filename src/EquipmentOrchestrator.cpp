#include "EquipmentOrchestrator.h"

namespace AquaLook { namespace Application {

void EquipmentOrchestrator::begin(
    const EquipmentManager* equipmentManager,
    uint8_t nbZones
) {
    _equipmentManager = equipmentManager;
    _nbZones = nbZones;
}

bool EquipmentOrchestrator::isInitialized() const {
    return _equipmentManager != nullptr &&
        _equipmentManager->isInitialized() &&
        _nbZones > 0U;
}

uint8_t EquipmentOrchestrator::zoneCount() const {
    return _nbZones;
}

EquipmentOrchestrator::Preview EquipmentOrchestrator::previewStartZone(
    uint8_t zone
) const {
    return previewZone(INTENT_START_ZONE, zone);
}

EquipmentOrchestrator::Preview EquipmentOrchestrator::previewStopZone(
    uint8_t zone
) const {
    return previewZone(INTENT_STOP_ZONE, zone);
}

EquipmentOrchestrator::Preview EquipmentOrchestrator::previewZone(
    Intent intent,
    uint8_t zone
) const {
    Preview preview;
    preview.intent = intent;
    preview.zone = zone;

    if (!isInitialized()) {
        preview.status = PREVIEW_NOT_INITIALIZED;
        return preview;
    }

    if (zone >= _nbZones) {
        preview.status = PREVIEW_INVALID_ZONE;
        preview.planResult = EquipmentManager::ACTION_INVALID_ZONE;
        return preview;
    }

    const EquipmentManager::ZoneExecutionPlan plan =
        intent == INTENT_START_ZONE
            ? _equipmentManager->buildZoneStartPlan(zone)
            : _equipmentManager->buildZoneStopPlan(zone);

    preview.planResult = plan.result;
    preview.requiresPump = plan.requiresPump;
    preview.stepCount = plan.stepCount;
    preview.status = plan.valid()
        ? PREVIEW_READY
        : PREVIEW_PLAN_REJECTED;
    return preview;
}

}} // namespace AquaLook::Application
