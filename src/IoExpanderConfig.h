#pragma once
#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════
//  IoExpander — couche d'entrees/sorties TOR configurable
//
//  Petites cartes MCP23017 sur le bus I2C partage. Chaque broche est
//  declaree independamment : entree ou sortie, un role, une zone associee,
//  son niveau actif. Rien n'est cable en dur -- tout se regle par la
//  configuration, comme la topologie relais.
//
//  Usages prevus :
//    - ENTREE  : presence d'electrovanne (fuite de courant au repos, via
//      opto), etat de volet, capteur TOR quelconque ;
//    - SORTIE  : eclairage, ventilation, contact auxiliaire.
//
//  Persistance dans un espace NVS DEDIE (aq_io) : la configuration
//  d'arrosage (blob "aqualook") n'est jamais touchee, et la couche entiere
//  reste desactivee par defaut. Aucun effet sur la carte de production tant
//  qu'elle n'est pas activee.
// ═══════════════════════════════════════════════════════════════

namespace IoExpander {

constexpr uint8_t MAX_BOARDS   = 8;    // MCP23017 : adresses 0x20..0x27
constexpr uint8_t MAX_BINDINGS = 32;   // broches declarees, tous roles confondus
constexpr uint8_t PINS_PER_BOARD = 16; // GPIOA 0..7, GPIOB 8..15

constexpr uint8_t ZONE_NONE = 0xFF;    // binding sans zone associee

// Direction d'une broche.
enum Direction : uint8_t {
    DIR_INPUT  = 0,
    DIR_OUTPUT = 1
};

// Role logique d'une broche. Le sens (entree/sortie) est porte par Direction ;
// le role dit ce que la broche represente pour l'application.
enum Role : uint8_t {
    ROLE_NONE           = 0,
    ROLE_VALVE_PRESENCE = 1,  // entree, associee a une zone, lue seulement au repos
    ROLE_TOR_INPUT      = 2,  // entree generique (volet, capteur)
    ROLE_LIGHTING       = 3,  // sortie
    ROLE_VENTILATION    = 4,  // sortie
    ROLE_TOR_OUTPUT     = 5   // sortie generique
};

// Une carte MCP23017 presente sur le bus.
struct Board {
    uint8_t enabled;     // 0 / 1
    uint8_t i2cAddress;  // 0x20..0x27
    uint8_t reserved[2];

    Board() : enabled(0), i2cAddress(0x21), reserved{0, 0} {}
};

// Declaration d'une broche : la brique configurable de base.
struct Binding {
    uint8_t enabled;      // 0 / 1
    uint8_t boardIndex;   // index dans boards[]
    uint8_t pin;          // 0..15
    uint8_t direction;    // Direction
    uint8_t role;         // Role
    uint8_t zone;         // zone associee (0..MAX_ZONES-1) ou ZONE_NONE
    uint8_t activeLevel;  // niveau logique (0/1) signifiant "actif" / "present"
    uint8_t pullup;       // entree : pull-up interne du MCP23017 (0/1)

    Binding()
        : enabled(0), boardIndex(0), pin(0), direction(DIR_INPUT),
          role(ROLE_NONE), zone(ZONE_NONE), activeLevel(1), pullup(1) {}
};

struct Config {
    uint16_t schemaVersion;
    uint8_t  enabled;      // interrupteur global de la couche
    uint8_t  pollSeconds;  // periode de scrutation des entrees (1..60)
    Board    boards[MAX_BOARDS];
    Binding  bindings[MAX_BINDINGS];

    Config();
};

// ── Validation ────────────────────────────────────────────────
const char* directionName(uint8_t direction);
const char* roleName(uint8_t role);
bool roleIsInput(uint8_t role);
bool roleIsOutput(uint8_t role);
bool validAddress(uint8_t addr);
bool validBoard(const Board& board);
// Un binding est coherent : carte activee, broche dans les bornes, direction
// compatible avec le role, zone valide pour les roles qui en exigent une.
bool validBinding(const Config& cfg, uint8_t bindingIndex, uint8_t nbZones);

Config makeSafeDefault();

// ── Persistance (espace NVS dedie aq_io) ──────────────────────
// load rend false si rien n'est stocke (defauts en place) ou si le bloc est
// invalide (entete/CRC) -- jamais d'ecrasement silencieux d'une config valide.
bool load(Config& out);
bool save(const Config& cfg);

} // namespace IoExpander
