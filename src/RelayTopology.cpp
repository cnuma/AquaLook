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
        case ROLE_INPUT_TOR: return "entree_tor";
        case ROLE_INPUT_LEVEL: return "niveau_cuve";
        case ROLE_INPUT_RAIN: return "pluie";
        case ROLE_INPUT_PRESENCE: return "presence";
        default: return "unknown";
    }
}

bool isSupportedController(uint8_t controller) {
    return controller == CONTROLLER_XL9535 ||
           controller == CONTROLLER_MCP23017;
}

bool isSupportedChannelCount(uint8_t channelCount) {
    return channelCount == 1 || channelCount == 2 ||
           channelCount == 4 || channelCount == 8 ||
           channelCount == 16;
}

bool isSupportedRole(uint8_t role) {
    return role == ROLE_UNUSED ||
           role == ROLE_ZONE_VALVE ||
           role == ROLE_PUMP ||
           role == ROLE_AUX ||
           role == ROLE_GREENHOUSE_VENT ||
           role == ROLE_LIGHTING ||
           role == ROLE_INPUT_TOR ||
           role == ROLE_INPUT_LEVEL ||
           role == ROLE_INPUT_RAIN ||
           role == ROLE_INPUT_PRESENCE;
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

bool equivalent(const RelayTopologyConfig& a, const RelayTopologyConfig& b) {
    for (uint8_t i = 0U; i < MAX_RELAY_BOARDS; ++i) {
        const RelayBoardConfig& x = a.boards[i];
        const RelayBoardConfig& y = b.boards[i];
        if (x.enabled != y.enabled) return false;
        if (!x.enabled) continue;
        if (x.controller != y.controller || x.i2cAddress != y.i2cAddress ||
            x.channelCount != y.channelCount || x.logic != y.logic ||
            x.transport != y.transport || x.node != y.node) {
            return false;
        }
    }
    for (uint8_t i = 0U; i < MAX_RELAY_ASSIGNMENTS; ++i) {
        const RelayAssignment& x = a.assignments[i];
        const RelayAssignment& y = b.assignments[i];
        if (x.enabled != y.enabled) return false;
        if (!x.enabled) continue;
        if (x.role != y.role || x.targetIndex != y.targetIndex ||
            x.boardIndex != y.boardIndex || x.channelIndex != y.channelIndex ||
            x.direction != y.direction || x.flags != y.flags || x.id != y.id) {
            return false;
        }
    }
    return true;
}

bool isWired(const RelayTopologyConfig& topology) {
    for (uint8_t b = 0; b < MAX_RELAY_BOARDS; b++) {
        if (validateBoard(topology.boards[b])) return true;
    }
    return false;
}

// Un transport sans pilote est refuse plutot qu accepte puis silencieusement
// inoperant : mieux vaut un refus clair a la configuration.
bool isSupportedTransport(uint8_t transport) {
    return transport == TRANSPORT_I2C_LOCAL;
}

bool validateBoard(const RelayBoardConfig& board) {
    if (!board.enabled) return false;
    if (!isSupportedTransport(board.transport)) return false;
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
    // Le SENS et le ROLE doivent s'accorder. Une entree declaree "vanne de
    // zone" ferait croire a une sortie pilotable et laisserait une zone
    // muette sans explication.
    if (assignment.direction > DIRECTION_INPUT) return false;
    if (isInputRole(assignment.role) != assignment.isInput()) return false;
    // Une entree porte un identifiant, sans quoi aucun script ne peut la
    // designer -- et une entree que rien ne peut lire ne sert a rien.
    if (assignment.isInput() && assignment.id == 0U) return false;

    const RelayBoardConfig& board = topology.boards[assignment.boardIndex];
    if (!validateBoard(board)) return false;
    if (assignment.channelIndex >= board.channelCount) return false;
    // La carte relais XL9535 retenue par ce projet (la "YellowCard") n'expose
    // pas ses GPIO sur des points de connexion : chaque broche est cablee en
    // interne vers son driver de relais. Il n'y a physiquement rien a
    // brancher en entree dessus. Contrairement au manque de tirage interne
    // (une limite qu'on peut contourner avec une resistance externe), c'est
    // une impossibilite de cablage, pas une precaution -- d'ou un rejet et
    // non un simple avertissement.
    if (assignment.isInput() && board.controller == CONTROLLER_XL9535) return false;

    return true;
}

// Une entree se resout par son IDENTIFIANT, jamais par sa position : un
// script qui citerait un index se mettrait a lire une autre voie des qu'une
// entree serait ajoutee ou retiree devant elle.
MappingResolution resolveInputById(
    const RelayTopologyConfig& topology,
    uint16_t inputId
) {
    MappingResolution resolution;
    if (inputId == 0U) return resolution;

    for (uint8_t a = 0U; a < MAX_RELAY_ASSIGNMENTS; ++a) {
        const RelayAssignment& assignment = topology.assignments[a];
        if (!assignment.isInput() || assignment.id != inputId) continue;
        if (!validateAssignment(topology, a)) return resolution;
        return resolveAssignment(topology, a);
    }
    return resolution;
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

bool hasDuplicateInputIds(const RelayTopologyConfig& topology) {
    for (uint8_t a = 0; a < MAX_RELAY_ASSIGNMENTS; a++) {
        if (!validateAssignment(topology, a)) continue;
        const RelayAssignment& aa = topology.assignments[a];
        if (!aa.isInput()) continue;

        for (uint8_t b = a + 1; b < MAX_RELAY_ASSIGNMENTS; b++) {
            if (!validateAssignment(topology, b)) continue;
            const RelayAssignment& ab = topology.assignments[b];
            if (ab.isInput() && aa.id == ab.id) return true;
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
