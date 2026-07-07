#include "domain/BinaryActuatorDriver.h"

namespace AquaLook { namespace Domain {

namespace {

BinaryActuatorDriverResult failed(BinaryActuatorDriverError error) {
    BinaryActuatorDriverResult result;
    result.status = BinaryActuatorCommandStatus::FAILED;
    result.error = error;
    return result;
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

} // namespace

bool isBinaryOutputPort(const PortDefinition& port) {
    const bool outputDirection =
        port.direction == PortDirection::OUTPUT ||
        port.direction == PortDirection::BIDIRECTIONAL;
    const bool binaryCapability =
        (port.capabilities &
         (PORT_CAP_DIGITAL_OUTPUT | PORT_CAP_RELAY_OUTPUT)) != 0U;
    return outputDirection && binaryCapability;
}

bool hasCompleteBinaryActuatorOps(const BinaryActuatorDriverOps& operations) {
    return operations.configure != nullptr &&
           operations.write != nullptr &&
           operations.read != nullptr &&
           operations.applySafeState != nullptr &&
           operations.health != nullptr;
}

BinaryActuatorDriverResult configureBinaryActuator(
    const BinaryActuatorDriverBinding& driver,
    const ControllerDefinition& controller,
    const PortDefinition& port,
    BinaryActuatorSession& session
) {
    if (!driver.controllerTypeId.isValid() ||
        driver.controllerTypeId != controller.typeId ||
        !driver.operations ||
        !hasCompleteBinaryActuatorOps(*driver.operations)) {
        return failed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (!isBinaryOutputPort(port) || port.controllerId != controller.id) {
        return failed(BinaryActuatorDriverError::UNSUPPORTED_PORT);
    }

    BinaryActuatorDriverResult result =
        driver.operations->configure(driver.context, controller, port);
    if (result.succeeded()) {
        session.portId = port.id;
        session.configured = 1U;
        session.lastApplied = result.state;
        ++session.revision;
    }
    return result;
}

BinaryActuatorDriverResult commandBinaryActuator(
    const BinaryActuatorDriverBinding& driver,
    const PortDefinition& port,
    BinaryActuatorState requested,
    BinaryActuatorSession& session
) {
    if (requested != BinaryActuatorState::INACTIVE &&
        requested != BinaryActuatorState::ACTIVE) {
        return failed(BinaryActuatorDriverError::INVALID_ARGUMENT);
    }
    if (!driver.operations || !driver.operations->write ||
        session.configured == 0U || session.portId != port.id) {
        return failed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }
    if (!isBinaryOutputPort(port)) {
        return failed(BinaryActuatorDriverError::UNSUPPORTED_PORT);
    }
    if (session.lastApplied == requested) {
        BinaryActuatorDriverResult result;
        result.status = BinaryActuatorCommandStatus::ALREADY_APPLIED;
        result.state = requested;
        return result;
    }

    BinaryActuatorDriverResult result =
        driver.operations->write(driver.context, port, requested);
    if (result.succeeded()) {
        session.lastApplied = requested;
        ++session.revision;
        result.state = requested;
    }
    return result;
}

BinaryActuatorDriverResult readBinaryActuator(
    const BinaryActuatorDriverBinding& driver,
    const PortDefinition& port,
    BinaryActuatorSession& session
) {
    if (!driver.operations || !driver.operations->read ||
        session.configured == 0U || session.portId != port.id) {
        return failed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    BinaryActuatorDriverResult result =
        driver.operations->read(driver.context, port);
    if (result.succeeded() && result.state != BinaryActuatorState::UNKNOWN) {
        session.lastApplied = result.state;
    }
    return result;
}

BinaryActuatorDriverResult applyBinaryActuatorSafeState(
    const BinaryActuatorDriverBinding& driver,
    const PortDefinition& port,
    BinaryActuatorSession& session
) {
    if (!driver.operations || !driver.operations->applySafeState ||
        session.configured == 0U || session.portId != port.id) {
        return failed(BinaryActuatorDriverError::NOT_CONFIGURED);
    }

    const BinaryActuatorState expected = safeStateForPort(port);
    if (expected == BinaryActuatorState::UNKNOWN) {
        return failed(BinaryActuatorDriverError::SAFE_STATE_UNSUPPORTED);
    }
    if (session.lastApplied == expected) {
        BinaryActuatorDriverResult result;
        result.status = BinaryActuatorCommandStatus::ALREADY_APPLIED;
        result.state = expected;
        return result;
    }

    BinaryActuatorDriverResult result =
        driver.operations->applySafeState(driver.context, port);
    if (result.succeeded()) {
        session.lastApplied = expected;
        ++session.revision;
        result.state = expected;
    }
    return result;
}

OperationError toOperationError(BinaryActuatorDriverError error) {
    switch (error) {
        case BinaryActuatorDriverError::NONE:
            return OperationError::NONE;
        case BinaryActuatorDriverError::INVALID_ARGUMENT:
            return OperationError::INVALID_STATE;
        case BinaryActuatorDriverError::NOT_CONFIGURED:
        case BinaryActuatorDriverError::UNSUPPORTED_PORT:
            return OperationError::CAPABILITY_NOT_SUPPORTED;
        case BinaryActuatorDriverError::UNAVAILABLE:
            return OperationError::ACTUATOR_UNAVAILABLE;
        case BinaryActuatorDriverError::COMMUNICATION_ERROR:
        case BinaryActuatorDriverError::READBACK_ERROR:
            return OperationError::COMMUNICATION_ERROR;
        case BinaryActuatorDriverError::SAFE_STATE_UNSUPPORTED:
            return OperationError::CAPABILITY_NOT_SUPPORTED;
        case BinaryActuatorDriverError::INTERNAL_ERROR:
        default:
            return OperationError::INTERNAL_ERROR;
    }
}

}} // namespace AquaLook::Domain
