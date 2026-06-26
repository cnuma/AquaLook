#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// Forward declaration
class ConfigManager;

// ═══════════════════════════════════════════════════════════════
//  RelaisManager — XL9535 I2C, logique inverse (bit=0 → ON)
//
//  Registres XL9535 :
//    0x02 OUTPUT_P0 — état sorties port 0 (relais 0..7)
//    0x03 OUTPUT_P1 — état sorties port 1 (relais 8..15)
//    0x06 CONFIG_P0 — direction port 0 (0=sortie)
//    0x07 CONFIG_P1 — direction port 1 (0=sortie)
//
//  Invariant I17 : bit=0 → relais ON, bit=1 → relais OFF
//  Invariant I7  : sécurité durée max dans update()
//
//  nbRelaisPhysical (depuis ConfigManager) :
//    Les zones < nbRelaisPhysical activent le hardware XL9535.
//    Les zones >= nbRelaisPhysical sont gérées en logique pure
//    (état, timer) mais n'écrivent pas sur le bus I2C.
//    Cela permet de tester le planning sans relais câblés.
// ═══════════════════════════════════════════════════════════════

#define XL9535_REG_OUTPUT_P0  0x02
#define XL9535_REG_OUTPUT_P1  0x03
#define XL9535_REG_CONFIG_P0  0x06
#define XL9535_REG_CONFIG_P1  0x07

class RelaisManager {
public:
    // ── Cycle de vie ──────────────────────────
    void begin(ConfigManager* config = nullptr);

    /// Sécurité durée max — appelé dans loop()
    void update();

    // ── Contrôle ──────────────────────────────
    void setRelay(uint8_t relay, bool state);
    bool getState(uint8_t relay) const;

private:
    ConfigManager* _config = nullptr;

    // Tableaux dimensionnés à MAX_ZONES — actifs = nbRelaisPhysical
    bool     _state[MAX_ZONES]     = {};
    uint32_t _startMs[MAX_ZONES]   = {};

    // Cache du registre hardware pour éviter un read I2C à chaque write
    uint8_t  _regP0 = 0xFF;  // tous OFF au départ (logique inverse)
    uint8_t  _regP1 = 0xFF;

    void    writeReg(uint8_t reg, uint8_t val);
    uint8_t readReg(uint8_t reg);
    void    applyHardware();  // écrit _regP0/_regP1 vers XL9535

    uint8_t nbRelaisPhysical() const;  // depuis _config ou NB_ZONES
    uint32_t maxWateringMs()   const;  // depuis _config ou défaut
};
