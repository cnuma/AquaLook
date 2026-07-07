#include "domain/SimulatedBinaryActuatorDriver.h"

namespace AquaLook { namespace Domain {

namespace {

BinaryActuatorDriverResult makeApplied(BinaryActuatorState state) {
    BinaryActuatorDriverResult result;
    result.status = BinaryActuatorCommandStatus::APPLIED;
    result.error = BinaryActuatorDriverError::NONE;
    result.state = state;
    return result;
}

BinaryActuatorDriverResult makeFailed(BinaryActuatorDriverError error) {
    BinaryActuatorDriverResult result;
    result.status = BinaryActuatorCommandStatus::FAILED;
    result.error = error;
    result.state = BinaryActuatorState::UNKNOWN;
    return result;
}

SimulatedBinaryActuatorContext* asContext(void* context) {
    return static_cast<SimulatedBinaryActuatorContext*>(context);
}

const SimulatedBinaryActuatorContext* asContext(const void* context) {
    return static_cast<const SimulatedBinaryActuatorContext*>(context);
}

BinaryActuatorDriverResult configureSimulated(
    void* rawContext,
    const ControllerDefinition&,
    const PortDefinition& port
) {
    SimulatedBinaryActuatorContext* context = asContext(rawContext);
    if (!context) return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);

    ++context->configureCount;
    if ((context->faultMask & SIMULATED_FAULT_UNAVAILABLE) != 0U) {
        context->health = BinaryActuatorHealth::UNAVAILABLE;
        return makeFailed(BinaryActuatorDriverError::UNAVAILABLE);
    }
    if ((context->faultMask & SIMULATED_FAULT_CONFIGURE) != 0U) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::INTERNAL_ERROR);
    }
    if (!isBinaryOutputPort(port)) {
        return makeFailed(BinaryActuatorDriverError::UNSUPPORTED_PORT);
    }

    context->configured = 1U;
    context->health = BinaryActuatorHealth::HEALTHY;
    return makeApplied(context->state);
}

BinaryActuatorDriverResult writeSimulated(
    void* rawContext,
    const PortDefinition&,
    BinaryActuatorState requested
) {
    SimulatedBinaryActuatorContext* context = asContext(rawContext);
    if (!context) return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    if (context->configured == 0U) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    ++context->writeCount;
    if ((context->faultMask & SIMULATED_FAULT_UNAVAILABLE) != 0U) {
        context->health = BinaryActuatorHealth::UNAVAILABLE;
        return makeFailed(BinaryActuatorDriverError::UNAVAILABLE);
    }
    if ((context->faultMask & SIMULATED_FAULT_WRITE) != 0U) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::COMMUNICATION_ERROR);
    }

    context->state = requested;
    context->health = BinaryActuatorHealth::HEALTHY;
    return makeApplied(context->state);
}

BinaryActuatorDriverResult readSimulated(
    void* rawContext,
    const PortDefinition&
) {
    SimulatedBinaryActuatorContext* context = asContext(rawContext);
    if (!context) return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    if (context->configured == 0U) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    ++context->readCount;
    if ((context->faultMask & SIMULATED_FAULT_UNAVAILABLE) != 0U) {
        context->health = BinaryActuatorHealth::UNAVAILABLE;
        return makeFailed(BinaryActuatorDriverError::UNAVAILABLE);
    }
    if ((context->faultMask & SIMULATED_FAULT_READ) != 0U) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::READBACK_ERROR);
    }

    BinaryActuatorDriverResult result = makeApplied(context->state);
    if ((context->faultMask & SIMULATED_FAULT_READBACK_MISMATCH) != 0U) {
        result.state = context->state == BinaryActuatorState::ACTIVE
            ? BinaryActuatorState::INACTIVE
            : BinaryActuatorState::ACTIVE;
        result.detail = 1U;
        context->health = BinaryActuatorHealth::DEGRADED;
    }
    return result;
}

BinaryActuatorDriverResult applySafeStateSimulated(
    void* rawContext,
    const PortDefinition& port
) {
    SimulatedBinaryActuatorContext* context = asContext(rawContext);
    if (!context) return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    if (context->configured == 0U) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    ++context->safeStateCount;
    if ((context->faultMask & SIMULATED_FAULT_UNAVAILABLE) != 0U) {
        context->health = BinaryActuatorHealth::UNAVAILABLE;
        return makeFailed(BinaryActuatorDriverError::UNAVAILABLE);
    }
    if ((context->faultMask & SIMULATED_FAULT_SAFE_STATE) != 0U) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::COMMUNICATION_ERROR);
    }

    if (port.safeState == PortSafeState::INACTIVE) {
        context->state = BinaryActuatorState::INACTIVE;
    } else if (port.safeState == PortSafeState::ACTIVE) {
        context->state = BinaryActuatorState::ACTIVE;
    } else {
        return makeFailed(BinaryActuatorDriverError::SAFE_STATE_UNSUPPORTED);
    }

    context->health = BinaryActuatorHealth::HEALTHY;
    return makeApplied(context->state);
}

BinaryActuatorHealth simulatedHealth(
    const void* rawContext,
    const PortDefinition&
) {
    const SimulatedBinaryActuatorContext* context = asContext(rawContext);
    if (!context) return BinaryActuatorHealth::FAULTED;
    if ((context->faultMask & SIMULATED_FAULT_UNAVAILABLE) != 0U) {
        return BinaryActuatorHealth::UNAVAILABLE;
    }
    return context->health;
}

const BinaryActuatorDriverOps OPERATIONS = {
    configureSimulated,
    writeSimulated,
    readSimulated,
    applySafeStateSimulated,
    simulatedHealth
};

} // namespace

const BinaryActuatorDriverOps& simulatedBinaryActuatorDriverOps() {
    return OPERATIONS;
}

void resetSimulatedBinaryActuatorContext(
    SimulatedBinaryActuatorContext& context,
    BinaryActuatorState initialState
) {
    context = SimulatedBinaryActuatorContext();
    context.state = initialState;
}

void setSimulatedBinaryActuatorFaults(
    SimulatedBinaryActuatorContext& context,
    uint8_t faultMask
) {
    context.faultMask = faultMask;
    if (faultMask == SIMULATED_FAULT_NONE) {
        context.health = BinaryActuatorHealth::HEALTHY;
    }
}

}} // namespace AquaLook::Domain
