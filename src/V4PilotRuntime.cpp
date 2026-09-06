#include "V4PilotRuntime.h"

#include "config.h"

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
    _boardCount = 0U;
    _portCount = 0U;
    _driverRegistry.clear();

    // Les tableaux sont indexes par l'index de TOPOLOGIE, pas compactes : le
    // backend retrouve une carte par acces direct (findBoardByTopologyIndex).
    // Les cartes non prises en charge laissent donc un trou, exclu ensuite du
    // masque de zones migrees -- elles restent servies par le moteur
    // historique au lieu de faire echouer tout le runtime.
    for (size_t i = 0U; i < BOARD_COUNT; ++i) {
        _controllers[i] = Domain::ControllerDefinition();
        _boards[i] = Domain::BoardDefinition();
    }

    uint32_t managedBoards = 0U;
    size_t nextPort = 0U;

    for (uint8_t b = 0U; b < RelayTopology::MAX_RELAY_BOARDS; ++b) {
        const RelayTopology::RelayBoardConfig& src = topology.boards[b];
        if (!src.enabled || !RelayTopology::validateBoard(src)) continue;
        // Seul le XL9535 dispose d'un pilote V4 a ce jour. Une carte d'un autre
        // type (MCP23017) reste pilotee par le moteur historique.
        if (src.controller != RelayTopology::CONTROLLER_XL9535) continue;
        if (nextPort + src.channelCount > PORT_COUNT) break;

        Domain::ControllerDefinition& controller = _controllers[b];
        controller.id = Domain::ControllerId(static_cast<uint16_t>(b + 1U));
        controller.typeId = Domain::ControllerTypeIds::XL9535;
        controller.busId = Domain::BusId(1U);
        controller.address = Domain::ControllerAddress(src.i2cAddress);
        controller.capabilities = Domain::CONTROLLER_CAP_DIGITAL_OUTPUT |
                                  Domain::CONTROLLER_CAP_RELAY_OUTPUT;
        controller.channelCount = 16U;
        controller.status = Domain::ControllerStatus::AVAILABLE;
        controller.flags = Domain::CONTROLLER_FLAG_ENABLED |
                           Domain::CONTROLLER_FLAG_ADDRESS_REQUIRED |
                           Domain::CONTROLLER_FLAG_EXCLUSIVE_ENDPOINT;

        Domain::BoardDefinition& board = _boards[b];
        board.id = Domain::BoardId(static_cast<uint16_t>(b + 1U));
        board.typeId = Domain::BoardTypeIds::RELAY_8_XL9535;
        board.controllerId = controller.id;
        board.modelVersion = 1U;
        board.firstPortIndex = static_cast<uint16_t>(nextPort);
        board.portCount = src.channelCount;
        board.status = Domain::BoardStatus::AVAILABLE;
        board.flags = Domain::BOARD_FLAG_ENABLED | Domain::BOARD_FLAG_EXTERNAL;

        for (uint8_t c = 0U; c < src.channelCount; ++c) {
            Domain::PortDefinition& port = _ports[nextPort + c];
            port = Domain::PortDefinition();
            port.controllerId = controller.id;
            port.boardId = board.id;
            port.id = Domain::PortId(static_cast<uint16_t>(nextPort + c + 1U));
            port.channel = static_cast<uint16_t>(c);
            port.capabilities = Domain::PORT_CAP_DIGITAL_OUTPUT |
                                Domain::PORT_CAP_RELAY_OUTPUT;
            port.type = Domain::PortType::RELAY;
            // Arduino definit OUTPUT en macro : passer par la valeur d'enum.
            port.direction = static_cast<Domain::PortDirection>(2U);
            port.safeState = Domain::PortSafeState::INACTIVE;
            port.flags = Domain::PORT_FLAG_ENABLED;
            if (src.logic == RelayTopology::LOGIC_INVERTED) {
                port.flags = static_cast<uint8_t>(port.flags | Domain::PORT_FLAG_INVERTED);
            }
        }

        // Chaque carte a SON contexte, donc sa propre adresse : c'est ce qui
        // permet deux cartes du meme type sans qu'elles se marchent dessus.
        Domain::Xl9535BinaryActuatorContext& ctx = _xl9535Contexts[b];
        ctx = Domain::Xl9535BinaryActuatorContext();
        ctx.i2c = &Drivers::arduinoI2cPlatformOps();
        ctx.platformContext = &RELAY_WIRE_BUS;
        ctx.sharedOutputState = &sharedOutputState;
        const uint16_t outputs = (src.channelCount >= 16U)
            ? 0xFFFFU
            : static_cast<uint16_t>((1UL << src.channelCount) - 1UL);
        ctx.directionMask = static_cast<uint16_t>(~outputs);

        Domain::BinaryActuatorDriverBinding binding =
            Domain::makeXl9535BinaryActuatorDriverBinding(ctx);
        binding.controllerId = controller.id;
        if (!_driverRegistry.registerDriver(binding).ok()) break;

        nextPort += src.channelCount;
        managedBoards |= (1UL << b);
        _boardCount = static_cast<size_t>(b) + 1U;
    }

    if (managedBoards == 0U) return false;
    _portCount = nextPort;

    _backend.bind(
        &topology,
        _controllers,
        _boardCount,
        _boards,
        _boardCount,
        _ports,
        _portCount,
        &_driverRegistry
    );

    // Une zone n'est migree que si SA carte est effectivement pilotee par V4.
    // Sans ce filtrage, une zone servie par une carte laissee au moteur
    // historique remonterait un echec de pilotage au lieu d'un repli normal.
    uint32_t migrated = 0U;
    for (uint8_t z = 0U; z < MAX_ZONES && z < 32U; ++z) {
        const RelayTopology::MappingResolution m =
            RelayTopology::resolveZoneValve(topology, z, MAX_ZONES);
        if (m.valid && (managedBoards & (1UL << m.boardIndex)) != 0U) {
            migrated |= (1UL << z);
        }
    }
    _backend.setMigratedZoneMask(migrated);

    _ready = _backend.isReady() && _backend.hasAnyMigratedZone();
    return _ready;
}

bool V4PilotRuntime::isReady() const {
    return _ready;
}

V4RelayPhysicalBackend& V4PilotRuntime::backend() {
    return _backend;
}

}} // namespace AquaLook::Runtime
