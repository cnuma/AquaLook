#include "EquipmentOutputRuntimeAdapter.h"

#include "RelaisManager.h"
#include "config.h"

namespace AquaLook { namespace Runtime {

void EquipmentOutputRuntimeAdapter::bind(RelaisManager* relayManager) {
    _relayManager = relayManager;
}

bool EquipmentOutputRuntimeAdapter::isBound() const {
    return _relayManager != nullptr;
}

Domain::ExecutionId EquipmentOutputRuntimeAdapter::nextExecutionId() {
    const uint16_t current = _nextExecutionValue;
    _nextExecutionValue = static_cast<uint16_t>(_nextExecutionValue + 1U);
    if (_nextExecutionValue == 0U || _nextExecutionValue == 0xFFFFU) {
        _nextExecutionValue = 1U;
    }
    return Domain::ExecutionId(current);
}

Domain::OperationResult EquipmentOutputRuntimeAdapter::rejected(
    Domain::EquipmentId equipmentId,
    Domain::OperationError error,
    uint32_t nowMs
) {
    Domain::OperationResult result;
    result.equipmentId = equipmentId;
    result.status = Domain::OperationStatus::REJECTED;
    result.stage = Domain::OperationStage::REQUEST;
    result.error = error;
    result.completedAtMs = nowMs;
    return result;
}

Domain::OperationResult EquipmentOutputRuntimeAdapter::command(
    const Domain::EquipmentOutputCommand& requested,
    uint32_t nowMs
) {
    if (requested.kind != Domain::EquipmentOutputKind::BINARY) {
        return rejected(
            Domain::EquipmentId(),
            Domain::OperationError::CAPABILITY_NOT_SUPPORTED,
            nowMs
        );
    }

    switch (requested.output.role) {
        case Domain::EquipmentOutputRole::ZONE_VALVE:
            return setZoneValve(requested.output.targetIndex, requested.active, nowMs);

        default:
            return rejected(
                Domain::EquipmentId(),
                Domain::OperationError::CAPABILITY_NOT_SUPPORTED,
                nowMs
            );
    }
}

Domain::OperationResult EquipmentOutputRuntimeAdapter::setZoneValve(
    uint8_t zoneIndex,
    bool active,
    uint32_t nowMs
) {
    const Domain::EquipmentId equipmentId =
        Domain::equipmentIdForZoneValve(zoneIndex);

    if (zoneIndex >= MAX_ZONES) {
        return rejected(equipmentId, Domain::OperationError::INVALID_TARGET, nowMs);
    }

    if (!_relayManager) {
        return rejected(
            equipmentId,
            Domain::OperationError::DEPENDENCY_UNAVAILABLE,
            nowMs
        );
    }

    _relayManager->setRelay(zoneIndex, active);

    Domain::OperationResult result;
    result.executionId = nextExecutionId();
    result.equipmentId = equipmentId;
    result.status = Domain::OperationStatus::APPLIED;
    result.stage = Domain::OperationStage::APPLICATION;
    result.error = Domain::OperationError::NONE;
    result.completedAtMs = nowMs;
    return result;
}

Domain::EquipmentStateValue EquipmentOutputRuntimeAdapter::getZoneValveState(
    uint8_t zoneIndex
) const {
    if (zoneIndex >= MAX_ZONES) {
        return Domain::EquipmentStateValue(
            0,
            Domain::StateValueKind::BINARY,
            Domain::StateValidity::INVALID
        );
    }

    if (!_relayManager) {
        return Domain::EquipmentStateValue(
            0,
            Domain::StateValueKind::BINARY,
            Domain::StateValidity::UNKNOWN
        );
    }

    return Domain::EquipmentStateValue(
        _relayManager->getState(zoneIndex) ? 1 : 0,
        Domain::StateValueKind::BINARY,
        Domain::StateValidity::VALID
    );
}

}} // namespace AquaLook::Runtime
