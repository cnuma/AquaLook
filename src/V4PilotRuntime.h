#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/BinaryActuatorDriverRegistry.h"
#include "domain/BoardPortModel.h"
#include "domain/HardwareInventoryModel.h"
#include "domain/I2cExpanderBinaryActuatorDriver.h"
#include "domain/I2cExpanderSharedOutputState.h"
#include "RelayTopology.h"
#include "V4RelayPhysicalBackend.h"

namespace AquaLook { namespace Runtime {

class V4PilotRuntime {
public:
    V4PilotRuntime();

    bool begin(
        const RelayTopology::RelayTopologyConfig& topology,
        Domain::I2cExpanderSharedOutputState& sharedOutputState
    );

    bool isReady() const;
    V4RelayPhysicalBackend& backend();

    // Lit une ENTREE tout ou rien par son identifiant stable.
    //
    // Passe par le meme pilote et le meme bus que les sorties : une entree
    // n'est pas un peripherique a part, c'est une voie dans l'autre sens.
    // Retourne false si l'identifiant ne designe rien, si la carte ne repond
    // pas, ou si l'option demandee n'existe pas sur ce composant.
    bool readInputById(uint16_t inputId, bool& active) const;

private:
    // Une entree par carte declarable dans la topologie : le runtime ne doit
    // plus etre le facteur limitant. Chaque carte a SON contexte, donc sa
    // propre adresse -- prerequis d'un montage a plusieurs cartes, et du jour
    // ou une carte vivra au bout d'un RS485, d'un Ethernet ou d'un LoRa.
    static constexpr size_t CONTROLLER_COUNT = RelayTopology::MAX_RELAY_BOARDS;
    static constexpr size_t BOARD_COUNT = RelayTopology::MAX_RELAY_BOARDS;
    static constexpr size_t PORT_COUNT =
        RelayTopology::MAX_RELAY_BOARDS * RelayTopology::MAX_CHANNELS_PER_BOARD;
    static constexpr size_t DRIVER_CAPACITY = RelayTopology::MAX_RELAY_BOARDS;

    Domain::ControllerDefinition _controllers[CONTROLLER_COUNT];
    Domain::BoardDefinition _boards[BOARD_COUNT];
    Domain::PortDefinition _ports[PORT_COUNT];
    Domain::BinaryActuatorDriverBinding _driverStorage[DRIVER_CAPACITY];
    Domain::BinaryActuatorDriverRegistry _driverRegistry;
    Domain::I2cExpanderActuatorContext _expanderContexts[BOARD_COUNT];
    // Conserve pour resoudre une entree apres coup : le cablage vit ailleurs,
    // on ne le recopie pas.
    const RelayTopology::RelayTopologyConfig* _topology = nullptr;
    size_t _boardCount = 0U;
    size_t _portCount = 0U;
    V4RelayPhysicalBackend _backend;
    bool _ready;
};

}} // namespace AquaLook::Runtime
