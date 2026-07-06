#pragma once
#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════
//  RelayTopology
//
//  Modèle matériel cible pour découpler les zones logiques AquaLook
//  des voies physiques des cartes relais I2C.
//
//  Ce module est volontairement isolé de ConfigManager pour le run 2 :
//  l'intégration NVS sera faite dans un run dédié avec migration de schéma.
// ═══════════════════════════════════════════════════════════════

namespace RelayTopology {

static constexpr uint8_t MAX_RELAY_BOARDS = 8;
static constexpr uint8_t MAX_CHANNELS_PER_BOARD = 8;

// Valeurs alignées sur le modèle existant ConfigManager :
// 0 = XL9535, 1 = MCP23017.
static constexpr uint8_t CONTROLLER_XL9535 = 0;
static constexpr uint8_t CONTROLLER_MCP23017 = 1;

static constexpr uint8_t LOGIC_INVERTED = 0;
static constexpr uint8_t LOGIC_DIRECT = 1;

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

struct ZoneRelayMapping {
    bool    enabled;
    uint8_t boardIndex;
    uint8_t channelIndex;

    ZoneRelayMapping()
        : enabled(false), boardIndex(0), channelIndex(0) {}
};

struct RelayTopologyConfig {
    RelayBoardConfig boards[MAX_RELAY_BOARDS];
    ZoneRelayMapping mappings[MAX_ZONES];
};

struct MappingResolution {
    bool valid;
    uint8_t boardIndex;
    uint8_t channelIndex;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t logic;

    MappingResolution()
        : valid(false), boardIndex(0), channelIndex(0),
          controller(CONTROLLER_XL9535), i2cAddress(XL9535_ADDR),
          logic(LOGIC_DIRECT) {}
};

const char* controllerName(uint8_t controller);
bool isSupportedController(uint8_t controller);
bool isSupportedChannelCount(uint8_t channelCount);
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

uint8_t totalEnabledChannels(const RelayTopologyConfig& topology);
bool hasDuplicateMappings(const RelayTopologyConfig& topology, uint8_t nbZones);

} // namespace RelayTopology
