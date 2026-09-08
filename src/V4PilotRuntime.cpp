#include "V4PilotRuntime.h"

#include "config.h"

#include <Wire.h>

#include "EventLog.h"
#include "domain/HardwareCatalog.h"
#include "drivers/ArduinoI2cPlatform.h"

namespace AquaLook { namespace Runtime {

V4PilotRuntime::V4PilotRuntime()
    : _driverRegistry(_driverStorage, DRIVER_CAPACITY), _ready(false) {}

bool V4PilotRuntime::begin(
    const RelayTopology::RelayTopologyConfig& topology,
    Domain::I2cExpanderSharedOutputState& sharedOutputState
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
        if (nextPort + src.channelCount > PORT_COUNT) break;

        // XL9535 et MCP23017 partagent le meme pilote, a un plan de registres
        // pres (voir I2cExpanderRegisterMap). Le repli sur le moteur
        // historique pour les cartes non-XL9535 n'a donc plus lieu d'etre :
        // TOUTES les cartes declarees sont pilotees par V4, ce qui est la
        // condition pour que la modularite serve a quelque chose -- un type
        // de carte de plus ne doit demander qu'une donnee, pas un chemin
        // d'execution parallele.
        const bool isMcp = (src.controller == RelayTopology::CONTROLLER_MCP23017);

        Domain::ControllerDefinition& controller = _controllers[b];
        controller.id = Domain::ControllerId(static_cast<uint16_t>(b + 1U));
        controller.typeId = isMcp ? Domain::ControllerTypeIds::MCP23017
                                  : Domain::ControllerTypeIds::XL9535;
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
        board.typeId = isMcp ? Domain::BoardTypeIds::IO_16_MCP23017
                             : Domain::BoardTypeIds::RELAY_8_XL9535;
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
        Domain::I2cExpanderActuatorContext& ctx = _expanderContexts[b];
        ctx = Domain::I2cExpanderActuatorContext();
        ctx.i2c = &Drivers::arduinoI2cPlatformOps();
        ctx.platformContext = &RELAY_WIRE_BUS;
        ctx.sharedOutputState = &sharedOutputState;
        const uint16_t outputs = (src.channelCount >= 16U)
            ? 0xFFFFU
            : static_cast<uint16_t>((1UL << src.channelCount) - 1UL);
        ctx.directionMask = static_cast<uint16_t>(~outputs);

        Domain::BinaryActuatorDriverBinding binding =
            isMcp ? Domain::makeMcp23017BinaryActuatorDriverBinding(ctx)
                  : Domain::makeXl9535BinaryActuatorDriverBinding(ctx);
        binding.controllerId = controller.id;
        // Ne PAS interrompre la boucle : une carte qui echoue ne doit pas
        // priver de V4 celles qui la suivent. Et surtout ne pas echouer en
        // silence -- c'est ce silence qui a laisse croire pendant des
        // semaines que toutes les cartes etaient pilotees par V4.
        const Domain::DriverRegistryResult reg = _driverRegistry.registerDriver(binding);
        if (!reg.ok()) {
            EventLog::log(LOG_ERROR,
                          "Relais V4: carte %u refusee par le registre (erreur %u)",
                          (unsigned)b, (unsigned)reg.error);
            continue;
        }

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
    // Plus de migration PARTIELLE : toute zone raccordee est pilotee par V4.
    //
    // Le filtrage sur managedBoards existait pour laisser au moteur historique
    // les cartes que V4 ne savait pas piloter. Ce repli a disparu -- il n'y a
    // plus de second moteur derriere. Une zone dont la carte a ete refusee par
    // le registre ne doit donc pas etre discretement rendue a personne : elle
    // echoue, bruyamment, a la premiere commande. C'est la seule facon qu'un
    // defaut de pilotage se voie.
    uint32_t migrated = 0U;
    for (uint8_t z = 0U; z < MAX_ZONES && z < 32U; ++z) {
        const RelayTopology::MappingResolution m =
            RelayTopology::resolveZoneValve(topology, z, MAX_ZONES);
        if (m.valid) {
            migrated |= (1UL << z);
        }
    }
    _backend.setMigratedZoneMask(migrated);

    // Dire ce qui est REELLEMENT pilote, carte par carte et zone par zone.
    // Le message de main.cpp affirmait "toutes les zones pilotees par V4"
    // sans jamais le verifier : il etait faux depuis l'ajout d'une deuxieme
    // carte, et rien ne le contredisait a l'ecran ni dans le journal.
    uint8_t managedCount = 0U;
    for (uint8_t b = 0U; b < RelayTopology::MAX_RELAY_BOARDS; ++b) {
        if ((managedBoards & (1UL << b)) != 0U) managedCount++;
    }
    uint8_t migratedCount = 0U;
    for (uint8_t z = 0U; z < 32U; ++z) {
        if ((migrated & (1UL << z)) != 0U) migratedCount++;
    }
    EventLog::log(LOG_INFO,
                  "Relais V4: %u carte(s) pilotee(s), %u zone(s) raccordee(s)",
                  (unsigned)managedCount, (unsigned)migratedCount);
    // Une zone raccordee a une carte que le registre a refusee echouera a la
    // premiere commande. Le dire MAINTENANT, au demarrage, plutot que de
    // laisser l'utilisateur le decouvrir devant une vanne qui ne s'ouvre pas.
    for (uint8_t z = 0U; z < MAX_ZONES && z < 32U; ++z) {
        if ((migrated & (1UL << z)) == 0U) continue;
        const RelayTopology::MappingResolution m =
            RelayTopology::resolveZoneValve(topology, z, MAX_ZONES);
        if ((managedBoards & (1UL << m.boardIndex)) == 0U) {
            EventLog::log(LOG_ERROR,
                          "Relais V4: zone %u sur carte %u sans pilote",
                          (unsigned)(z + 1U), (unsigned)m.boardIndex);
        }
    }

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
