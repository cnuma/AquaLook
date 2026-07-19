#include "EquipmentOutputRuntimeAdapter.h"

#include "EventLog.h"
#include "NotificationManager.h"
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

const EquipmentOutputRuntimeAdapter::ExecutionCounters&
EquipmentOutputRuntimeAdapter::executionCounters() const {
    return _executionCounters;
}

void EquipmentOutputRuntimeAdapter::recordExecutionPath(ExecutionPath path) {
    _lastExecutionPath = path;
    switch (path) {
        case ExecutionPath::PHYSICAL_BACKEND:
            ++_executionCounters.physicalBackend;
            break;
        case ExecutionPath::RELAY_MANAGER_FALLBACK:
            ++_executionCounters.relayManagerFallback;
            break;
        case ExecutionPath::FAILED:
            ++_executionCounters.failed;
            break;
        case ExecutionPath::NONE:
        default:
            break;
    }
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
        recordExecutionPath(ExecutionPath::FAILED);
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
            recordExecutionPath(ExecutionPath::FAILED);
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
        recordExecutionPath(ExecutionPath::FAILED);
        EventLog::log(
            LOG_ERROR,
            "Equipment: zone %u %s path=failed error=invalid_target",
            zoneIndex + 1U,
            active ? "ON" : "OFF"
        );
        return rejected(equipmentId, Domain::OperationError::INVALID_TARGET, nowMs);
    }

    bool applied = false;
    bool appliedByPhysicalBackend = false;
    const Domain::EquipmentStateValue previousState = getZoneValveState(zoneIndex);
    const bool previousActive =
        previousState.validity == Domain::StateValidity::VALID &&
        previousState.kind == Domain::StateValueKind::BINARY &&
        previousState.value != 0;

    if (_physicalBackend) {
        applied = _physicalBackend->setZoneValve(zoneIndex, active, nowMs);
        appliedByPhysicalBackend = applied;
    }

    if (appliedByPhysicalBackend && _relayManager) {
        _relayManager->mirrorZoneState(zoneIndex, active, nowMs);
    }

    if (!applied && _relayManager) {
        applied = _relayManager->setRelay(zoneIndex, active);
        if (applied) {
            recordExecutionPath(ExecutionPath::RELAY_MANAGER_FALLBACK);
        }
    } else if (appliedByPhysicalBackend) {
        recordExecutionPath(ExecutionPath::PHYSICAL_BACKEND);
    }

    if (!applied) {
        recordExecutionPath(ExecutionPath::FAILED);
        EventLog::log(
            LOG_ERROR,
            "Equipment: zone %u %s path=failed error=dependency_unavailable",
            zoneIndex + 1U,
            active ? "ON" : "OFF"
        );
        return rejected(
            equipmentId,
            Domain::OperationError::DEPENDENCY_UNAVAILABLE,
            nowMs
        );
    }

    if (previousActive != active) {
        NotificationManager::enqueueZoneEvent(zoneIndex, active);
    }

    Domain::OperationResult result;
    result.executionId = nextExecutionId();
    result.equipmentId = equipmentId;
    result.status = Domain::OperationStatus::APPLIED;
    result.stage = Domain::OperationStage::APPLICATION;
    result.error = Domain::OperationError::NONE;
    result.completedAtMs = nowMs;

    EventLog::log(
        LOG_INFO,
        "Equipment: zone %u %s path=%s exec=%u totals=%lu/%lu/%lu",
        zoneIndex + 1U,
        active ? "ON" : "OFF",
        executionPathName(_lastExecutionPath),
        static_cast<unsigned>(result.executionId.value),
        static_cast<unsigned long>(_executionCounters.physicalBackend),
        static_cast<unsigned long>(_executionCounters.relayManagerFallback),
        static_cast<unsigned long>(_executionCounters.failed)
    );

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
