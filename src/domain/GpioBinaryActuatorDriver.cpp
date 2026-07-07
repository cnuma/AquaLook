#include "domain/GpioBinaryActuatorDriver.h"

#if AQUALOOK_V4_ENABLE_GPIO

#include "domain/HardwareCatalog.h"

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

GpioBinaryActuatorContext* asContext(void* context) {
    return static_cast<GpioBinaryActuatorContext*>(context);
}

const GpioBinaryActuatorContext* asContext(const void* context) {
    return static_cast<const GpioBinaryActuatorContext*>(context);
}

bool isInverted(const PortDefinition& port) {
    return (port.flags & PORT_FLAG_INVERTED) != 0U;
}

GpioLevel levelForState(const PortDefinition& port, BinaryActuatorState state) {
    const bool active = state == BinaryActuatorState::ACTIVE;
    const bool high = active != isInverted(port);
    return high ? GpioLevel::LEVEL_HIGH : GpioLevel::LEVEL_LOW;
}

BinaryActuatorState stateForLevel(const PortDefinition& port, GpioLevel level) {
    const bool high = level == GpioLevel::LEVEL_HIGH;
    const bool active = high != isInverted(port);
    return active ? BinaryActuatorState::ACTIVE : BinaryActuatorState::INACTIVE;
}

BinaryActuatorState safeStateForPort(const PortDefinition& port) {
    if (port.safeState == PortSafeState::INACTIVE) {
        return BinaryActuatorState::INACTIVE;
    }
    if (port.safeState == PortSafeState::ACTIVE) {
        return BinaryActuatorState::ACTIVE;
    }
    return BinaryActuatorState::UNKNOWN;
}

bool contextIsUsable(const GpioBinaryActuatorContext& context) {
    return context.platform &&
           hasCompleteGpioPlatformOps(*context.platform);
}

BinaryActuatorDriverResult writeLogicalState(
    GpioBinaryActuatorContext& context,
    const PortDefinition& port,
    BinaryActuatorState requested
) {
    if (!context.platform->writeLevel(
            context.platformContext,
            port.channel,
            levelForState(port, requested))) {
        context.health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::COMMUNICATION_ERROR);
    }

    context.lastObserved = requested;
    context.health = BinaryActuatorHealth::HEALTHY;
    return makeApplied(requested);
}

BinaryActuatorDriverResult configureGpio(
    void* rawContext,
    const ControllerDefinition& controller,
    const PortDefinition& port
) {
    GpioBinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (controller.typeId != ControllerTypeIds::LOCAL_GPIO ||
        port.controllerId != controller.id || !isBinaryOutputPort(port)) {
        return makeFailed(BinaryActuatorDriverError::UNSUPPORTED_PORT);
    }
    if (!context->platform->isPinValid(context->platformContext, port.channel)) {
        context->health = BinaryActuatorHealth::UNAVAILABLE;
        return makeFailed(BinaryActuatorDriverError::UNSUPPORTED_PORT);
    }
    if (!context->platform->setMode(
            context->platformContext, port.channel, GpioPinMode::MODE_OUTPUT)) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::COMMUNICATION_ERROR);
    }

    context->configured = 1U;
    context->configuredPin = port.channel;

    const BinaryActuatorState safeState = safeStateForPort(port);
    if (safeState == BinaryActuatorState::UNKNOWN) {
        context->health = BinaryActuatorHealth::DEGRADED;
        return makeFailed(BinaryActuatorDriverError::SAFE_STATE_UNSUPPORTED);
    }

    return writeLogicalState(*context, port, safeState);
}

BinaryActuatorDriverResult writeGpio(
    void* rawContext,
    const PortDefinition& port,
    BinaryActuatorState requested
) {
    GpioBinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (context->configured == 0U || context->configuredPin != port.channel) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }
    return writeLogicalState(*context, port, requested);
}

BinaryActuatorDriverResult readGpio(
    void* rawContext,
    const PortDefinition& port
) {
    GpioBinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (context->configured == 0U || context->configuredPin != port.channel) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    GpioLevel level = GpioLevel::LEVEL_LOW;
    if (!context->platform->readLevel(
            context->platformContext, port.channel, level)) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::READBACK_ERROR);
    }

    context->lastObserved = stateForLevel(port, level);
    context->health = BinaryActuatorHealth::HEALTHY;
    return makeApplied(context->lastObserved);
}

BinaryActuatorDriverResult applySafeStateGpio(
    void* rawContext,
    const PortDefinition& port
) {
    GpioBinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (context->configured == 0U || context->configuredPin != port.channel) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    const BinaryActuatorState safeState = safeStateForPort(port);
    if (safeState == BinaryActuatorState::UNKNOWN) {
        return makeFailed(BinaryActuatorDriverError::SAFE_STATE_UNSUPPORTED);
    }
    return writeLogicalState(*context, port, safeState);
}

BinaryActuatorHealth gpioHealth(
    const void* rawContext,
    const PortDefinition& port
) {
    const GpioBinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return BinaryActuatorHealth::FAULTED;
    }
    if (!context->platform->isPinValid(context->platformContext, port.channel)) {
        return BinaryActuatorHealth::UNAVAILABLE;
    }
    return context->health;
}

const BinaryActuatorDriverOps OPERATIONS = {
    configureGpio,
    writeGpio,
    readGpio,
    applySafeStateGpio,
    gpioHealth
};

} // namespace

bool hasCompleteGpioPlatformOps(const GpioPlatformOps& operations) {
    return operations.setMode != nullptr &&
           operations.writeLevel != nullptr &&
           operations.readLevel != nullptr &&
           operations.isPinValid != nullptr;
}

const BinaryActuatorDriverOps& gpioBinaryActuatorDriverOps() {
    return OPERATIONS;
}

BinaryActuatorDriverBinding makeGpioBinaryActuatorDriverBinding(
    GpioBinaryActuatorContext& context
) {
    BinaryActuatorDriverBinding binding;
    binding.controllerTypeId = ControllerTypeIds::LOCAL_GPIO;
    binding.operations = &OPERATIONS;
    binding.context = &context;
    return binding;
}

}} // namespace AquaLook::Domain

#endif
