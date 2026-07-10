#include "domain/Xl9535BinaryActuatorDriver.h"

#if AQUALOOK_V4_ENABLE_I2C

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

Xl9535BinaryActuatorContext* asContext(void* context) {
    return static_cast<Xl9535BinaryActuatorContext*>(context);
}

const Xl9535BinaryActuatorContext* asContext(const void* context) {
    return static_cast<const Xl9535BinaryActuatorContext*>(context);
}

bool contextIsUsable(const Xl9535BinaryActuatorContext& context) {
    return context.i2c && hasCompleteXl9535I2cOps(*context.i2c);
}

bool isValidChannel(uint16_t channel) {
    return channel < 16U;
}

uint16_t channelMask(uint16_t channel) {
    return static_cast<uint16_t>(1U << channel);
}

bool isInverted(const PortDefinition& port) {
    return (port.flags & PORT_FLAG_INVERTED) != 0U;
}

bool levelHighForState(const PortDefinition& port, BinaryActuatorState state) {
    const bool active = state == BinaryActuatorState::ACTIVE;
    return active != isInverted(port);
}

BinaryActuatorState stateForLevel(const PortDefinition& port, bool high) {
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

bool syncOutputLatchFromSharedState(Xl9535BinaryActuatorContext& context) {
    if (!context.sharedOutputState) {
        return true;
    }

    uint16_t sharedValue = 0U;
    if (context.sharedOutputState->read(context.address, sharedValue)) {
        context.outputLatch = sharedValue;
        return true;
    }

    return context.sharedOutputState->seed(context.address, context.outputLatch);
}

bool writeOutputLatch(Xl9535BinaryActuatorContext& context) {
    return context.i2c->writeRegister16(
        context.platformContext,
        context.address,
        Xl9535Registers::OUTPUT_PORT,
        context.outputLatch
    );
}

bool writeConfiguration(Xl9535BinaryActuatorContext& context) {
    return context.i2c->writeRegister16(
        context.platformContext,
        context.address,
        Xl9535Registers::CONFIGURATION,
        context.directionMask
    );
}

bool setLatchBit(
    Xl9535BinaryActuatorContext& context,
    const PortDefinition& port,
    BinaryActuatorState state
) {
    const bool high = levelHighForState(port, state);

    if (context.sharedOutputState) {
        uint16_t sharedValue = 0U;
        if (!context.sharedOutputState->updateChannel(
                context.address,
                static_cast<uint8_t>(port.channel),
                high,
                sharedValue)) {
            return false;
        }
        context.outputLatch = sharedValue;
        return true;
    }

    const uint16_t mask = channelMask(port.channel);
    if (high) {
        context.outputLatch = static_cast<uint16_t>(context.outputLatch | mask);
    } else {
        context.outputLatch = static_cast<uint16_t>(context.outputLatch & ~mask);
    }
    return true;
}

BinaryActuatorDriverResult writeLogicalState(
    Xl9535BinaryActuatorContext& context,
    const PortDefinition& port,
    BinaryActuatorState requested
) {
    if (!setLatchBit(context, port, requested)) {
        context.health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::INTERNAL_ERROR);
    }
    if (!writeOutputLatch(context)) {
        context.health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::COMMUNICATION_ERROR);
    }

    context.lastObserved = requested;
    context.health = BinaryActuatorHealth::HEALTHY;
    return makeApplied(requested);
}

BinaryActuatorDriverResult configureXl9535(
    void* rawContext,
    const ControllerDefinition& controller,
    const PortDefinition& port
) {
    Xl9535BinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (controller.typeId != ControllerTypeIds::XL9535 ||
        port.controllerId != controller.id || !isBinaryOutputPort(port) ||
        !isValidChannel(port.channel)) {
        return makeFailed(BinaryActuatorDriverError::UNSUPPORTED_PORT);
    }

    const uint8_t address = static_cast<uint8_t>(controller.address.primary & 0xFFU);
    if (!context->i2c->probe(context->platformContext, address)) {
        context->health = BinaryActuatorHealth::UNAVAILABLE;
        return makeFailed(BinaryActuatorDriverError::UNAVAILABLE);
    }

    context->address = address;
    if (!syncOutputLatchFromSharedState(*context)) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::INTERNAL_ERROR);
    }

    const BinaryActuatorState safeState = safeStateForPort(port);
    if (safeState == BinaryActuatorState::UNKNOWN) {
        context->health = BinaryActuatorHealth::DEGRADED;
        return makeFailed(BinaryActuatorDriverError::SAFE_STATE_UNSUPPORTED);
    }

    if (!setLatchBit(*context, port, safeState)) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::INTERNAL_ERROR);
    }
    if (!writeOutputLatch(*context)) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::COMMUNICATION_ERROR);
    }

    context->directionMask = static_cast<uint16_t>(
        context->directionMask & ~channelMask(port.channel)
    );
    if (!writeConfiguration(*context)) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::COMMUNICATION_ERROR);
    }

    context->configured = 1U;
    context->lastObserved = safeState;
    context->health = BinaryActuatorHealth::HEALTHY;
    return makeApplied(safeState);
}

BinaryActuatorDriverResult writeXl9535(
    void* rawContext,
    const PortDefinition& port,
    BinaryActuatorState requested
) {
    Xl9535BinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (context->configured == 0U || !isValidChannel(port.channel)) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }
    return writeLogicalState(*context, port, requested);
}

BinaryActuatorDriverResult readXl9535(
    void* rawContext,
    const PortDefinition& port
) {
    Xl9535BinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (context->configured == 0U || !isValidChannel(port.channel)) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    uint16_t value = 0U;
    if (!context->i2c->readRegister16(
            context->platformContext,
            context->address,
            Xl9535Registers::INPUT_PORT,
            value)) {
        context->health = BinaryActuatorHealth::FAULTED;
        return makeFailed(BinaryActuatorDriverError::READBACK_ERROR);
    }

    const bool high = (value & channelMask(port.channel)) != 0U;
    context->lastObserved = stateForLevel(port, high);
    context->health = BinaryActuatorHealth::HEALTHY;
    return makeApplied(context->lastObserved);
}

BinaryActuatorDriverResult applySafeStateXl9535(
    void* rawContext,
    const PortDefinition& port
) {
    Xl9535BinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (context->configured == 0U || !isValidChannel(port.channel)) {
        return makeFailed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    const BinaryActuatorState safeState = safeStateForPort(port);
    if (safeState == BinaryActuatorState::UNKNOWN) {
        return makeFailed(BinaryActuatorDriverError::SAFE_STATE_UNSUPPORTED);
    }
    return writeLogicalState(*context, port, safeState);
}

BinaryActuatorHealth xl9535Health(
    const void* rawContext,
    const PortDefinition&
) {
    const Xl9535BinaryActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return BinaryActuatorHealth::FAULTED;
    }
    if (context->configured == 0U) {
        return BinaryActuatorHealth::UNKNOWN;
    }
    if (!context->i2c->probe(context->platformContext, context->address)) {
        return BinaryActuatorHealth::UNAVAILABLE;
    }
    return context->health;
}

const BinaryActuatorDriverOps OPERATIONS = {
    configureXl9535,
    writeXl9535,
    readXl9535,
    applySafeStateXl9535,
    xl9535Health
};

} // namespace

bool hasCompleteXl9535I2cOps(const Xl9535I2cOps& operations) {
    return operations.probe != nullptr &&
           operations.writeRegister16 != nullptr &&
           operations.readRegister16 != nullptr;
}

const BinaryActuatorDriverOps& xl9535BinaryActuatorDriverOps() {
    return OPERATIONS;
}

BinaryActuatorDriverBinding makeXl9535BinaryActuatorDriverBinding(
    Xl9535BinaryActuatorContext& context
) {
    BinaryActuatorDriverBinding binding;
    binding.controllerTypeId = ControllerTypeIds::XL9535;
    binding.operations = &OPERATIONS;
    binding.context = &context;
    return binding;
}

}} // namespace AquaLook::Domain

#endif
