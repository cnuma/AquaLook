#include "EquipmentOrchestrator.h"

namespace AquaLook { namespace Application {

void EquipmentOrchestrator::begin(
    EquipmentManager* equipmentManager,
    uint8_t nbZones
) {
    _equipmentManager = equipmentManager;
    _nbZones = nbZones;
    resetStats();
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

EquipmentOrchestrator::ExecutionResult EquipmentOrchestrator::executeStartZone(
    uint8_t zone
) {
    return executeZone(INTENT_START_ZONE, zone);
}

EquipmentOrchestrator::ExecutionResult EquipmentOrchestrator::executeStopZone(
    uint8_t zone
) {
    return executeZone(INTENT_STOP_ZONE, zone);
}

const EquipmentOrchestrator::ObservationStats& EquipmentOrchestrator::stats() const {
    return _stats;
}

void EquipmentOrchestrator::resetStats() {
    _stats = ObservationStats();
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
        recordPreview(preview);
        return preview;
    }

    if (zone >= _nbZones) {
        preview.status = PREVIEW_INVALID_ZONE;
        preview.planResult = EquipmentManager::ACTION_INVALID_ZONE;
        recordPreview(preview);
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
    recordPreview(preview);
    return preview;
}

EquipmentOrchestrator::ExecutionResult EquipmentOrchestrator::executeZone(
    Intent intent,
    uint8_t zone
) {
    ExecutionResult result;
    result.preview = previewZone(intent, zone);
    result.actionResult = result.preview.planResult;

    if (!result.preview.ready() || _equipmentManager == nullptr) {
        return result;
    }

    result.actionResult = intent == INTENT_START_ZONE
        ? _equipmentManager->startZone(zone)
        : _equipmentManager->stopZone(zone);
    result.executed = true;
    return result;
}

void EquipmentOrchestrator::recordPreview(const Preview& preview) const {
    ++_stats.totalRequests;
    if (preview.intent == INTENT_START_ZONE) ++_stats.startRequests;
    if (preview.intent == INTENT_STOP_ZONE) ++_stats.stopRequests;

    switch (preview.status) {
        case PREVIEW_READY:
            ++_stats.readyPlans;
            _stats.plannedSteps += preview.stepCount;
            if (preview.requiresPump) ++_stats.plansWithPump;
            break;
        case PREVIEW_NOT_INITIALIZED:
            ++_stats.rejectedPlans;
            ++_stats.notInitialized;
            break;
        case PREVIEW_INVALID_ZONE:
            ++_stats.rejectedPlans;
            ++_stats.invalidZones;
            break;
        case PREVIEW_PLAN_REJECTED:
            ++_stats.rejectedPlans;
            ++_stats.managerRejections;
            break;
    }
}

}} // namespace AquaLook::Application
