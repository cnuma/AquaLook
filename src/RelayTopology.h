#pragma once
#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════
//  RelayTopology
//
//  Modèle matériel cible pour découpler les usages logiques AquaLook
//  des voies physiques des cartes relais I2C.
//
//  Un relais physique n'est pas obligatoirement une électrovanne :
//  il peut piloter une zone, une pompe, un contact sec, un volet de
//  serre, un éclairage ou tout autre équipement auxiliaire.
// ═══════════════════════════════════════════════════════════════

namespace RelayTopology {

static constexpr uint8_t MAX_RELAY_BOARDS = 8;
static constexpr uint8_t MAX_CHANNELS_PER_BOARD = 8;
static constexpr uint8_t MAX_RELAY_ASSIGNMENTS = MAX_ZONES;

// Valeurs alignées sur le modèle existant ConfigManager :
// 0 = XL9535, 1 = MCP23017.
static constexpr uint8_t CONTROLLER_XL9535 = 0;
static constexpr uint8_t CONTROLLER_MCP23017 = 1;

static constexpr uint8_t LOGIC_INVERTED = 0;
static constexpr uint8_t LOGIC_DIRECT = 1;

// Rôle logique d'une voie relais.
// Les zones d'arrosage gardent leur index de zone dans targetIndex.
// Les autres rôles pourront être exploités plus tard par des managers dédiés.
static constexpr uint8_t ROLE_UNUSED = 0;
static constexpr uint8_t ROLE_ZONE_VALVE = 1;
static constexpr uint8_t ROLE_PUMP = 2;
static constexpr uint8_t ROLE_AUX = 3;
static constexpr uint8_t ROLE_GREENHOUSE_VENT = 4;
static constexpr uint8_t ROLE_LIGHTING = 5;

struct RelayBoardConfig {
    bool    enabled;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t channelCount;
    uint8_t logic;

    RelayBoardConfig()
        : enabled(false),
          controller(CONTROLLER_XL9535),
          i2cAddress(XL9535_ADDR),
          channelCount(0),
          logic(LOGIC_DIRECT) {}
};

struct RelayAssignment {
    bool    enabled;
    uint8_t role;
    uint8_t targetIndex;
    uint8_t boardIndex;
    uint8_t channelIndex;

    RelayAssignment()
        : enabled(false), role(ROLE_UNUSED), targetIndex(0),
          boardIndex(0), channelIndex(0) {}
};

// Alias de compatibilité conceptuelle : une zone d'arrosage est maintenant
// un cas particulier de RelayAssignment avec role=ROLE_ZONE_VALVE.
using ZoneRelayMapping = RelayAssignment;

struct RelayTopologyConfig {
    RelayBoardConfig boards[MAX_RELAY_BOARDS];
    RelayAssignment assignments[MAX_RELAY_ASSIGNMENTS];
};

struct MappingResolution {
    bool valid;
    uint8_t role;
    uint8_t targetIndex;
    uint8_t boardIndex;
    uint8_t channelIndex;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t logic;

    MappingResolution()
        : valid(false), role(ROLE_UNUSED), targetIndex(0),
          boardIndex(0), channelIndex(0), controller(CONTROLLER_XL9535),
          i2cAddress(XL9535_ADDR), logic(LOGIC_DIRECT) {}
};

const char* controllerName(uint8_t controller);
const char* roleName(uint8_t role);
bool isSupportedController(uint8_t controller);
bool isSupportedChannelCount(uint8_t channelCount);
bool isSupportedRole(uint8_t role);
uint8_t normalizeChannelCount(uint8_t channelCount);
uint8_t defaultAddressForController(uint8_t controller);

void clear(RelayTopologyConfig& topology);
void buildLegacyCompatibleTopology(
    RelayTopologyConfig& topology,
    uint8_t nbZones,
    uint8_t nbRelaisPhysical,
    uint8_t controller,
    uint8_t logic
);

bool validateBoard(const RelayBoardConfig& board);
bool validateAssignment(
    const RelayTopologyConfig& topology,
    uint8_t assignmentIndex
);

MappingResolution resolveAssignment(
    const RelayTopologyConfig& topology,
    uint8_t assignmentIndex
);

MappingResolution resolveZoneValve(
    const RelayTopologyConfig& topology,
    uint8_t zone,
    uint8_t nbZones
);

uint8_t totalEnabledChannels(const RelayTopologyConfig& topology);
bool hasDuplicateAssignments(const RelayTopologyConfig& topology);

// Compatibilité avec les appels du run précédent.
bool validateMapping(
    const RelayTopologyConfig& topology,
    uint8_t zone,
    uint8_t nbZones
);
MappingResolution resolveMapping(
    const RelayTopologyConfig& topology,
    uint8_t zone,
    uint8_t nbZones
);
bool hasDuplicateMappings(const RelayTopologyConfig& topology, uint8_t nbZones);

} // namespace RelayTopology
