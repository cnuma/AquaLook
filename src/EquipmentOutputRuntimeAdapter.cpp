#include "EquipmentOutputRuntimeAdapter.h"

#include "RelaisManager.h"
#include "RelayPhysicalBackend.h"
#include "config.h"

namespace AquaLook { namespace Runtime {

void EquipmentOutputRuntimeAdapter::bind(RelaisManager* relayManager) {
    _relayManager = relayManager;
}

void EquipmentOutputRuntimeAdapter::setPhysicalBackend(
    RelayPhysicalBackend* physicalBackend
) {
    _physicalBackend = physicalBackend;
}

bool EquipmentOutputRuntimeAdapter::isBound() const {
    return _relayManager != nullptr || _physicalBackend != nullptr;
}

EquipmentOutputRuntimeAdapter::ExecutionPath
EquipmentOutputRuntimeAdapter::lastExecutionPath() const {
    return _lastExecutionPath;
}

const char* EquipmentOutputRuntimeAdapter::executionPathName(ExecutionPath path) {
    switch (path) {
        case ExecutionPath::PHYSICAL_BACKEND: return "physical_backend";
        case ExecutionPath::RELAY_MANAGER_FALLBACK: return "relay_manager_fallback";
        case ExecutionPath::FAILED: return "failed";
        case ExecutionPath::NONE:
        default:
            return "none";
    }
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
        _lastExecutionPath = ExecutionPath::FAILED;
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
            _lastExecutionPath = ExecutionPath::FAILED;
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
        _lastExecutionPath = ExecutionPath::FAILED;
        return rejected(equipmentId, Domain::OperationError::INVALID_TARGET, nowMs);
    }

    bool applied = false;
    bool appliedByPhysicalBackend = false;

    if (_physicalBackend) {
        applied = _physicalBackend->setZoneValve(zoneIndex, active, nowMs);
        appliedByPhysicalBackend = applied;
    }

    if (appliedByPhysicalBackend && _relayManager) {
        _relayManager->mirrorZoneState(zoneIndex, active, nowMs);
    }

    if (!applied && _relayManager) {
        _relayManager->setRelay(zoneIndex, active);
        applied = true;
        _lastExecutionPath = ExecutionPath::RELAY_MANAGER_FALLBACK;
    } else if (appliedByPhysicalBackend) {
        _lastExecutionPath = ExecutionPath::PHYSICAL_BACKEND;
    }

    if (!applied) {
        _lastExecutionPath = ExecutionPath::FAILED;
        return rejected(
            equipmentId,
            Domain::OperationError::DEPENDENCY_UNAVAILABLE,
            nowMs
        );
    }

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

    bool active = false;

    if (_physicalBackend && _physicalBackend->getZoneValveState(zoneIndex, active)) {
        return Domain::EquipmentStateValue(
            active ? 1 : 0,
            Domain::StateValueKind::BINARY,
            Domain::StateValidity::VALID
        );
    }

    if (_relayManager) {
        return Domain::EquipmentStateValue(
            _relayManager->getState(zoneIndex) ? 1 : 0,
            Domain::StateValueKind::BINARY,
            Domain::StateValidity::VALID
        );
    }

    return Domain::EquipmentStateValue(
        0,
        Domain::StateValueKind::BINARY,
        Domain::StateValidity::UNKNOWN
    );
}

}} // namespace AquaLook::Runtime
