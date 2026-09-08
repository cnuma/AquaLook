#include "domain/I2cExpanderBinaryActuatorDriver.h"

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

I2cExpanderActuatorContext* asContext(void* context) {
    return static_cast<I2cExpanderActuatorContext*>(context);
}

const I2cExpanderActuatorContext* asContext(const void* context) {
    return static_cast<const I2cExpanderActuatorContext*>(context);
}

bool contextIsUsable(const I2cExpanderActuatorContext& context) {
    return context.i2c && hasCompleteI2cExpanderOps(*context.i2c);
}

// Plan de registres du XL9535, servant de defaut : un contexte construit
// avant l'arrivee du MCP23017 ne porte pas de plan et doit continuer a se
// comporter exactement comme avant.
constexpr I2cExpanderRegisterMap XL9535_MAP = {
    Xl9535Registers::INPUT_PORT,
    Xl9535Registers::OUTPUT_PORT,
    Xl9535Registers::CONFIGURATION
};

constexpr I2cExpanderRegisterMap MCP23017_MAP = {
    Mcp23017Registers::GPIO,
    Mcp23017Registers::OLAT,
    Mcp23017Registers::IODIR
};

const I2cExpanderRegisterMap& registersFor(const I2cExpanderActuatorContext& context) {
    return context.registers ? *context.registers : XL9535_MAP;
}

ControllerTypeId expectedTypeFor(const I2cExpanderActuatorContext& context) {
    return context.expectedControllerType.isValid()
        ? context.expectedControllerType
        : ControllerTypeIds::XL9535;
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

bool syncOutputLatchFromSharedState(I2cExpanderActuatorContext& context) {
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

bool writeOutputLatch(I2cExpanderActuatorContext& context) {
    return context.i2c->writeRegister16(
        context.platformContext,
        context.address,
        registersFor(context).output,
        context.outputLatch
    );
}

bool writeConfiguration(I2cExpanderActuatorContext& context) {
    return context.i2c->writeRegister16(
        context.platformContext,
        context.address,
        registersFor(context).direction,
        context.directionMask
    );
}

bool setLatchBit(
    I2cExpanderActuatorContext& context,
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
    I2cExpanderActuatorContext& context,
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
    I2cExpanderActuatorContext* context = asContext(rawContext);
    if (!context || !contextIsUsable(*context)) {
        return makeFailed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (controller.typeId != expectedTypeFor(*context) ||
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
    I2cExpanderActuatorContext* context = asContext(rawContext);
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
    I2cExpanderActuatorContext* context = asContext(rawContext);
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
            registersFor(*context).input,
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
    I2cExpanderActuatorContext* context = asContext(rawContext);
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
    const I2cExpanderActuatorContext* context = asContext(rawContext);
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

bool hasCompleteI2cExpanderOps(const I2cExpanderOps& operations) {
    return operations.probe != nullptr &&
           operations.writeRegister16 != nullptr &&
           operations.readRegister16 != nullptr;
}

const BinaryActuatorDriverOps& i2cExpanderBinaryActuatorDriverOps() {
    return OPERATIONS;
}

BinaryActuatorDriverBinding makeXl9535BinaryActuatorDriverBinding(
    I2cExpanderActuatorContext& context
) {
    context.registers = &XL9535_MAP;
    context.expectedControllerType = ControllerTypeIds::XL9535;
    BinaryActuatorDriverBinding binding;
    binding.controllerTypeId = ControllerTypeIds::XL9535;
    binding.operations = &OPERATIONS;
    binding.context = &context;
    return binding;
}

BinaryActuatorDriverBinding makeMcp23017BinaryActuatorDriverBinding(
    I2cExpanderActuatorContext& context
) {
    context.registers = &MCP23017_MAP;
    context.expectedControllerType = ControllerTypeIds::MCP23017;
    BinaryActuatorDriverBinding binding;
    binding.controllerTypeId = ControllerTypeIds::MCP23017;
    binding.operations = &OPERATIONS;
    binding.context = &context;
    return binding;
}

}} // namespace AquaLook::Domain

#endif
