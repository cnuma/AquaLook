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
static constexpr uint8_t RESERVED_AUXILIARY_ASSIGNMENTS = 4;
// Les ENTREES partagent la meme table que les sorties : une voie est une
// voie, sur la meme carte, au bout du meme transport. Seul son SENS change.
// Les separer aurait duplique la validation, le stockage et l'editeur pour
// decrire la meme chose.
static constexpr uint8_t RESERVED_INPUT_ASSIGNMENTS = 8;
static constexpr uint8_t MAX_RELAY_ASSIGNMENTS =
    MAX_ZONES + RESERVED_AUXILIARY_ASSIGNMENTS + RESERVED_INPUT_ASSIGNMENTS;

// Valeurs alignées sur le modèle existant ConfigManager :
// 0 = XL9535, 1 = MCP23017.
static constexpr uint8_t CONTROLLER_XL9535 = 0;
static constexpr uint8_t CONTROLLER_MCP23017 = 1;

// Ou vit la carte. Le transport est une DONNEE, pas une hypothese du code :
// une carte peut etre locale sur I2C, au bout d un RS485, joignable en IP,
// ou sur un lien LoRa. Les couches du dessus ne doivent jamais le savoir --
// ajouter un transport doit se reduire a un pilote et une valeur de config.
//
// Seul I2C_LOCAL dispose d un pilote a ce jour ; les autres valeurs sont
// reservees et refusees a la validation tant que leur pilote n existe pas,
// pour ne pas laisser croire a une capacite absente.
static constexpr uint8_t TRANSPORT_I2C_LOCAL = 0;
static constexpr uint8_t TRANSPORT_RS485     = 1;
static constexpr uint8_t TRANSPORT_IP        = 2;
static constexpr uint8_t TRANSPORT_LORA      = 3;

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

// Roles d'ENTREE, a partir de 16 pour qu'un simple seuil suffise a les
// distinguer d'un coup d'oeil dans un journal ou un export JSON.
static constexpr uint8_t ROLE_INPUT_FIRST = 16;
static constexpr uint8_t ROLE_INPUT_TOR = 16;    // contact sec quelconque
static constexpr uint8_t ROLE_INPUT_LEVEL = 17;  // niveau de cuve
static constexpr uint8_t ROLE_INPUT_RAIN = 18;   // pluviometre tout ou rien
static constexpr uint8_t ROLE_INPUT_PRESENCE = 19;

// Sens d'une voie.
static constexpr uint8_t DIRECTION_OUTPUT = 0;
static constexpr uint8_t DIRECTION_INPUT = 1;

// Options d'une entree. Un flotteur de cuve se cable presque toujours en
// contact a la masse avec resistance de tirage : actif a l'etat BAS.
static constexpr uint8_t INPUT_FLAG_ACTIVE_LOW = 0x01;
static constexpr uint8_t INPUT_FLAG_PULLUP     = 0x02;

inline bool isInputRole(uint8_t role) { return role >= ROLE_INPUT_FIRST; }

struct RelayBoardConfig {
    bool    enabled;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t channelCount;
    uint8_t logic;
    uint8_t transport;      // TRANSPORT_* : ou vit la carte
    uint8_t node;           // adresse sur le transport (noeud RS485, id LoRa)

    RelayBoardConfig()
        : enabled(false),
          controller(CONTROLLER_XL9535),
          i2cAddress(XL9535_ADDR),
          channelCount(0),
          logic(LOGIC_DIRECT),
          transport(TRANSPORT_I2C_LOCAL),
          node(0) {}
};

struct RelayAssignment {
    bool     enabled;
    uint8_t  role;
    uint8_t  targetIndex;
    uint8_t  boardIndex;
    uint8_t  channelIndex;
    uint8_t  direction;   // DIRECTION_OUTPUT ou DIRECTION_INPUT
    uint8_t  flags;       // INPUT_FLAG_* : n'a de sens que pour une entree
    uint16_t id;          // identifiant STABLE, cite par les scripts

    RelayAssignment()
        : enabled(false), role(ROLE_UNUSED), targetIndex(0),
          boardIndex(0), channelIndex(0),
          direction(DIRECTION_OUTPUT), flags(0), id(0) {}

    bool isInput() const { return direction == DIRECTION_INPUT; }
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
bool isSupportedTransport(uint8_t transport);
uint8_t normalizeChannelCount(uint8_t channelCount);
uint8_t defaultAddressForController(uint8_t controller);

void clear(RelayTopologyConfig& topology);

// true si au moins une carte est declaree ET valide. Un module dont le
// cablage n'a jamais ete renseigne repond false : il ne pilote rien, ce qui
// est un etat NORMAL de premiere mise en service, pas une panne.
bool isWired(const RelayTopologyConfig& topology);

bool validateBoard(const RelayBoardConfig& board);
// Resolution d'une ENTREE par son identifiant stable. C'est ainsi qu'un
// script la designe : jamais par un index de table, qui bougerait.
MappingResolution resolveInputById(
    const RelayTopologyConfig& topology,
    uint16_t inputId
);
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
