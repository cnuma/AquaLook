#include "RelayTopology.h"

namespace RelayTopology {

const char* controllerName(uint8_t controller) {
    switch (controller) {
        case CONTROLLER_XL9535: return "XL9535";
        case CONTROLLER_MCP23017: return "MCP23017";
        default: return "UNKNOWN";
    }
}

const char* roleName(uint8_t role) {
    switch (role) {
        case ROLE_UNUSED: return "unused";
        case ROLE_ZONE_VALVE: return "zone_valve";
        case ROLE_PUMP: return "pump";
        case ROLE_AUX: return "aux";
        case ROLE_GREENHOUSE_VENT: return "greenhouse_vent";
        case ROLE_LIGHTING: return "lighting";
        default: return "unknown";
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

bool isSupportedRole(uint8_t role) {
    return role == ROLE_UNUSED ||
           role == ROLE_ZONE_VALVE ||
           role == ROLE_PUMP ||
           role == ROLE_AUX ||
           role == ROLE_GREENHOUSE_VENT ||
           role == ROLE_LIGHTING;
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
    for (uint8_t a = 0; a < MAX_RELAY_ASSIGNMENTS; a++) {
        topology.assignments[a] = RelayAssignment{};
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
        RelayAssignment& a = topology.assignments[z];
        a.enabled = true;
        a.role = ROLE_ZONE_VALVE;
        a.targetIndex = z;
        a.boardIndex = 0;
        a.channelIndex = z;
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

bool validateAssignment(
    const RelayTopologyConfig& topology,
    uint8_t assignmentIndex
) {
    if (assignmentIndex >= MAX_RELAY_ASSIGNMENTS) return false;

    const RelayAssignment& assignment = topology.assignments[assignmentIndex];
    if (!assignment.enabled) return false;
    if (!isSupportedRole(assignment.role)) return false;
    if (assignment.role == ROLE_UNUSED) return false;
    if (assignment.boardIndex >= MAX_RELAY_BOARDS) return false;

    const RelayBoardConfig& board = topology.boards[assignment.boardIndex];
    if (!validateBoard(board)) return false;
    if (assignment.channelIndex >= board.channelCount) return false;

    return true;
}

MappingResolution resolveAssignment(
    const RelayTopologyConfig& topology,
    uint8_t assignmentIndex
) {
    MappingResolution result;
    if (!validateAssignment(topology, assignmentIndex)) return result;

    const RelayAssignment& assignment = topology.assignments[assignmentIndex];
    const RelayBoardConfig& board = topology.boards[assignment.boardIndex];

    result.valid = true;
    result.role = assignment.role;
    result.targetIndex = assignment.targetIndex;
    result.boardIndex = assignment.boardIndex;
    result.channelIndex = assignment.channelIndex;
    result.controller = board.controller;
    result.i2cAddress = board.i2cAddress;
    result.logic = board.logic;
    return result;
}

MappingResolution resolveZoneValve(
    const RelayTopologyConfig& topology,
    uint8_t zone,
    uint8_t nbZones
) {
    MappingResolution result;
    if (zone >= nbZones || zone >= MAX_ZONES) return result;

    for (uint8_t a = 0; a < MAX_RELAY_ASSIGNMENTS; a++) {
        if (!validateAssignment(topology, a)) continue;
        const RelayAssignment& assignment = topology.assignments[a];
        if (assignment.role == ROLE_ZONE_VALVE &&
            assignment.targetIndex == zone) {
            return resolveAssignment(topology, a);
        }
    }

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

bool hasDuplicateAssignments(const RelayTopologyConfig& topology) {
    for (uint8_t a = 0; a < MAX_RELAY_ASSIGNMENTS; a++) {
        if (!validateAssignment(topology, a)) continue;
        const RelayAssignment& aa = topology.assignments[a];

        for (uint8_t b = a + 1; b < MAX_RELAY_ASSIGNMENTS; b++) {
            if (!validateAssignment(topology, b)) continue;
            const RelayAssignment& ab = topology.assignments[b];

            if (aa.boardIndex == ab.boardIndex &&
                aa.channelIndex == ab.channelIndex) {
                return true;
            }
        }
    }

    return false;
}

bool validateMapping(
    const RelayTopologyConfig& topology,
    uint8_t zone,
    uint8_t nbZones
) {
    return resolveZoneValve(topology, zone, nbZones).valid;
}

MappingResolution resolveMapping(
    const RelayTopologyConfig& topology,
    uint8_t zone,
    uint8_t nbZones
) {
    return resolveZoneValve(topology, zone, nbZones);
}

bool hasDuplicateMappings(const RelayTopologyConfig& topology, uint8_t nbZones) {
    (void)nbZones;
    return hasDuplicateAssignments(topology);
}

} // namespace RelayTopology
