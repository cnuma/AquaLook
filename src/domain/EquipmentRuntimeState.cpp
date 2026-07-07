#include "domain/EquipmentRuntimeState.h"

namespace AquaLook { namespace Domain {

bool sameStateValue(const EquipmentStateValue& lhs, const EquipmentStateValue& rhs) {
    return lhs.kind == rhs.kind &&
           lhs.validity == rhs.validity &&
           lhs.value == rhs.value;
}

bool isConverged(const EquipmentRuntimeState& state) {
    if (state.applied.validity != StateValidity::VALID) return false;
    if (state.observed.validity == StateValidity::NOT_SUPPORTED) {
        return sameStateValue(state.authorized, state.applied);
    }
    if (state.observed.validity != StateValidity::VALID) return false;
    return sameStateValue(state.applied, state.observed);
}

bool hasBlockingFault(const EquipmentFault& fault) {
    return isFaultActive(fault) && (fault.flags & FAULT_FLAG_BLOCKING) != 0U;
}

bool isFaultActive(const EquipmentFault& fault) {
    return (fault.flags & FAULT_FLAG_ACTIVE) != 0U;
}

static void incrementRevision(EquipmentRuntimeState& runtime) {
    runtime.revision = static_cast<uint16_t>(runtime.revision + 1U);
    if (runtime.revision == 0U) runtime.revision = 1U;
}

void recordRequestedState(
    EquipmentRuntimeState& runtime,
    const EquipmentStateValue& value,
    uint32_t nowMs
) {
    runtime.requested = value;
    runtime.requestedAtMs = nowMs;
    runtime.flags |= RUNTIME_FLAG_COMMAND_PENDING;
    incrementRevision(runtime);
}

void recordAuthorizedState(
    EquipmentRuntimeState& runtime,
    const EquipmentStateValue& value,
    bool authorized,
    uint32_t nowMs
) {
    runtime.authorized = value;
    runtime.authorizedAtMs = nowMs;
    if (authorized) {
        runtime.flags |= RUNTIME_FLAG_AUTHORIZED;
    } else {
        runtime.flags &= static_cast<uint8_t>(~RUNTIME_FLAG_AUTHORIZED);
        runtime.flags &= static_cast<uint8_t>(~RUNTIME_FLAG_COMMAND_PENDING);
    }
    incrementRevision(runtime);
}

void recordAppliedState(
    EquipmentRuntimeState& runtime,
    const EquipmentStateValue& value,
    uint32_t nowMs
) {
    runtime.applied = value;
    runtime.appliedAtMs = nowMs;
    runtime.flags &= static_cast<uint8_t>(~RUNTIME_FLAG_COMMAND_PENDING);
    incrementRevision(runtime);
}

void recordObservedState(
    EquipmentRuntimeState& runtime,
    const EquipmentStateValue& value,
    uint32_t nowMs
) {
    runtime.observed = value;
    runtime.observedAtMs = nowMs;
    incrementRevision(runtime);
}

void activateFault(EquipmentFault& fault, uint32_t nowMs) {
    if (!isFaultActive(fault)) {
        fault.firstSeenAtMs = nowMs;
        fault.occurrenceCount = 1U;
    } else if (fault.occurrenceCount < 0xFFU) {
        fault.occurrenceCount++;
    }
    fault.lastSeenAtMs = nowMs;
    fault.flags |= FAULT_FLAG_ACTIVE;
    fault.flags &= static_cast<uint8_t>(~FAULT_FLAG_ACKNOWLEDGED);
}

void clearFault(EquipmentFault& fault, uint32_t nowMs) {
    fault.lastSeenAtMs = nowMs;
    fault.flags &= static_cast<uint8_t>(~FAULT_FLAG_ACTIVE);
}

}} // namespace AquaLook::Domain
