#include "RelayTopology.h"

namespace RelayTopology {

const char* controllerName(uint8_t controller) {
    switch (controller) {
        case CONTROLLER_XL9535: return "XL9535";
        case CONTROLLER_MCP23017: return "MCP23017";
        default: return "UNKNOWN";
    }
}

bool isSupportedController(uint8_t controller) {
    return controller == CONTROLLER_XL9535 ||
           controller == CONTROLLER_MCP23017;
}

bool isSupportedChannelCount(uint8_t channelCount) {
    return channelCount == 1 || channelCount == 2 ||
           channelCount == 4 || channelCount == 8;
}

uint8_t normalizeChannelCount(uint8_t channelCount) {
    if (channelCount <= 1) return 1;
    if (channelCount <= 2) return 2;
    if (channelCount <= 4) return 4;
    return 8;
}

uint8_t defaultAddressForController(uint8_t controller) {
    switch (controller) {
        case CONTROLLER_MCP23017:
            return 0x20;
        case CONTROLLER_XL9535:
        default:
            return XL9535_ADDR;
    }
}

void clear(RelayTopologyConfig& topology) {
    for (uint8_t b = 0; b < MAX_RELAY_BOARDS; b++) {
        topology.boards[b] = RelayBoardConfig{};
    }
    for (uint8_t z = 0; z < MAX_ZONES; z++) {
        topology.mappings[z] = ZoneRelayMapping{};
    }
}

void buildLegacyCompatibleTopology(
    RelayTopologyConfig& topology,
    uint8_t nbZones,
    uint8_t nbRelaisPhysical,
    uint8_t controller,
    uint8_t logic
) {
    clear(topology);

    nbZones = constrain(nbZones, (uint8_t)1, (uint8_t)MAX_ZONES);
    nbRelaisPhysical = constrain(nbRelaisPhysical, (uint8_t)1, nbZones);
    controller = isSupportedController(controller) ? controller : CONTROLLER_XL9535;
    logic = (logic <= 1) ? logic : LOGIC_DIRECT;

    RelayBoardConfig& board0 = topology.boards[0];
    board0.enabled = true;
    board0.controller = controller;
    board0.i2cAddress = defaultAddressForController(controller);
    board0.channelCount = normalizeChannelCount(nbRelaisPhysical);
    board0.logic = logic;

    for (uint8_t z = 0; z < nbZones && z < board0.channelCount; z++) {
        topology.mappings[z].enabled = true;
        topology.mappings[z].boardIndex = 0;
        topology.mappings[z].channelIndex = z;
    }
}

bool validateBoard(const RelayBoardConfig& board) {
    if (!board.enabled) return false;
    if (!isSupportedController(board.controller)) return false;
    if (!isSupportedChannelCount(board.channelCount)) return false;
    if (board.logic > 1) return false;

    // Les contrôleurs retenus utilisent une base d'adresses 0x20..0x27
    // avec trois broches d'adressage. On reste volontairement strict ici.
    if (board.i2cAddress < 0x20 || board.i2cAddress > 0x27) return false;

    return true;
}

bool validateMapping(
    const RelayTopologyConfig& topology,
    uint8_t zone,
    uint8_t nbZones
) {
    if (zone >= nbZones || zone >= MAX_ZONES) return false;

    const ZoneRelayMapping& mapping = topology.mappings[zone];
    if (!mapping.enabled) return false;
    if (mapping.boardIndex >= MAX_RELAY_BOARDS) return false;

    const RelayBoardConfig& board = topology.boards[mapping.boardIndex];
    if (!validateBoard(board)) return false;
    if (mapping.channelIndex >= board.channelCount) return false;

    return true;
}

MappingResolution resolveMapping(
    const RelayTopologyConfig& topology,
    uint8_t zone,
    uint8_t nbZones
) {
    MappingResolution result;
    if (!validateMapping(topology, zone, nbZones)) return result;

    const ZoneRelayMapping& mapping = topology.mappings[zone];
    const RelayBoardConfig& board = topology.boards[mapping.boardIndex];

    result.valid = true;
    result.boardIndex = mapping.boardIndex;
    result.channelIndex = mapping.channelIndex;
    result.controller = board.controller;
    result.i2cAddress = board.i2cAddress;
    result.logic = board.logic;
    return result;
}

uint8_t totalEnabledChannels(const RelayTopologyConfig& topology) {
    uint8_t total = 0;
    for (uint8_t b = 0; b < MAX_RELAY_BOARDS; b++) {
        const RelayBoardConfig& board = topology.boards[b];
        if (!validateBoard(board)) continue;
        total = constrain((uint16_t)total + board.channelCount, 0, 255);
    }
    return total;
}

bool hasDuplicateMappings(const RelayTopologyConfig& topology, uint8_t nbZones) {
    nbZones = constrain(nbZones, (uint8_t)0, (uint8_t)MAX_ZONES);

    for (uint8_t a = 0; a < nbZones; a++) {
        if (!validateMapping(topology, a, nbZones)) continue;
        const ZoneRelayMapping& ma = topology.mappings[a];

        for (uint8_t b = a + 1; b < nbZones; b++) {
            if (!validateMapping(topology, b, nbZones)) continue;
            const ZoneRelayMapping& mb = topology.mappings[b];

            if (ma.boardIndex == mb.boardIndex &&
                ma.channelIndex == mb.channelIndex) {
                return true;
            }
        }
    }

    return false;
}

} // namespace RelayTopology
