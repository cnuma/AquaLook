#include "V4PilotRuntime.h"

#include <Wire.h>

#include "domain/HardwareCatalog.h"
#include "drivers/ArduinoI2cPlatform.h"

namespace AquaLook { namespace Runtime {

V4PilotRuntime::V4PilotRuntime()
    : _driverRegistry(_driverStorage, DRIVER_CAPACITY), _ready(false) {}

bool V4PilotRuntime::begin(
    const RelayTopology::RelayTopologyConfig& topology,
    Domain::Xl9535SharedOutputState& sharedOutputState
) {
    _ready = false;
    _driverRegistry.clear();
    _xl9535Context = Domain::Xl9535BinaryActuatorContext();

    const RelayTopology::RelayBoardConfig& sourceBoard = topology.boards[0];
    if (!RelayTopology::validateBoard(sourceBoard) ||
        sourceBoard.controller != RelayTopology::CONTROLLER_XL9535 ||
        sourceBoard.channelCount == 0U || sourceBoard.channelCount > PORT_COUNT) {
        return false;
    }

    Domain::ControllerDefinition& controller = _controllers[0];
    controller = Domain::ControllerDefinition();
    controller.id = Domain::ControllerId(1U);
    controller.typeId = Domain::ControllerTypeIds::XL9535;
    controller.busId = Domain::BusId(1U);
    controller.address = Domain::ControllerAddress(sourceBoard.i2cAddress);
    controller.capabilities = Domain::CONTROLLER_CAP_DIGITAL_OUTPUT |
                              Domain::CONTROLLER_CAP_RELAY_OUTPUT;
    controller.channelCount = 16U;
    controller.status = Domain::ControllerStatus::AVAILABLE;
    controller.flags = Domain::CONTROLLER_FLAG_ENABLED |
                       Domain::CONTROLLER_FLAG_ADDRESS_REQUIRED |
                       Domain::CONTROLLER_FLAG_EXCLUSIVE_ENDPOINT;

    Domain::BoardDefinition& board = _boards[0];
    board = Domain::BoardDefinition();
    board.id = Domain::BoardId(1U);
    board.typeId = Domain::BoardTypeIds::RELAY_8_XL9535;
    board.controllerId = controller.id;
    board.modelVersion = 1U;
    board.firstPortIndex = 0U;
    board.portCount = sourceBoard.channelCount;
    board.status = Domain::BoardStatus::AVAILABLE;
    board.flags = Domain::BOARD_FLAG_ENABLED | Domain::BOARD_FLAG_EXTERNAL;

    for (size_t index = 0U; index < PORT_COUNT; ++index) {
        Domain::PortDefinition& port = _ports[index];
        port = Domain::PortDefinition();
        port.controllerId = controller.id;
        port.boardId = board.id;
        port.id = Domain::PortId(static_cast<uint16_t>(index + 1U));
        port.channel = static_cast<uint16_t>(index);
        port.capabilities = Domain::PORT_CAP_DIGITAL_OUTPUT |
                            Domain::PORT_CAP_RELAY_OUTPUT;
        port.type = Domain::PortType::RELAY;
        port.direction = Domain::PortDirection::OUTPUT;
        port.safeState = Domain::PortSafeState::INACTIVE;
        port.flags = Domain::PORT_FLAG_ENABLED;
        if (sourceBoard.logic == RelayTopology::LOGIC_INVERTED) {
            port.flags = static_cast<uint8_t>(port.flags | Domain::PORT_FLAG_INVERTED);
        }
    }

    _xl9535Context.i2c = &Drivers::arduinoI2cPlatformOps();
    _xl9535Context.platformContext = &Wire;
    _xl9535Context.sharedOutputState = &sharedOutputState;

    const Domain::DriverRegistryResult registered = _driverRegistry.registerDriver(
        Domain::makeXl9535BinaryActuatorDriverBinding(_xl9535Context)
    );
    if (!registered.ok()) {
        return false;
    }

    _backend.bind(
        &topology,
        _controllers,
        CONTROLLER_COUNT,
        _boards,
        BOARD_COUNT,
        _ports,
        board.portCount,
        &_driverRegistry
    );
    _backend.setMigratedZoneMask(1UL << 0U);
    _ready = _backend.isReady() && _backend.isZoneMigrated(0U);
    return _ready;
}

bool V4PilotRuntime::isReady() const {
    return _ready;
}

V4RelayPhysicalBackend& V4PilotRuntime::backend() {
    return _backend;
}

}} // namespace AquaLook::Runtime
