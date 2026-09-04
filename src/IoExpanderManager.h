#pragma once
#include <Arduino.h>
#include "IoExpanderConfig.h"

class RelaisManager;

// ═══════════════════════════════════════════════════════════════
//  IoExpanderManager — runtime de la couche d'E/S TOR
//
//  Non bloquant, appele depuis loop(). Scrute les entrees a la periode
//  configuree, pilote les sorties, et n'interprete la presence d'une
//  electrovanne QUE lorsque sa zone est au repos (relais ouvert) -- une
//  bobine alimentee ne laisse pas voir sa fuite de repos.
//
//  Reste inerte tant que la configuration n'est pas activee : aucun effet
//  sur un module qui n'a pas de carte d'E/S declaree.
// ═══════════════════════════════════════════════════════════════
class IoExpanderManager {
public:
    // Etat logique d'une broche declaree.
    enum State : uint8_t {
        ST_UNKNOWN  = 0,  // pas encore lu, ou presence non evaluable (zone active)
        ST_INACTIVE = 1,  // entree : absente/inactive ; sortie : commandee OFF
        ST_ACTIVE   = 2   // entree : presente/active  ; sortie : commandee ON
    };

    void begin(RelaisManager* relais);
    // nbZones sert a valider les zones associees et n'est lu qu'ici.
    void update(uint32_t nowMs, uint8_t nbZones);

    bool enabled() const { return _cfg.enabled != 0; }
    const IoExpander::Config& config() const { return _cfg; }

    // Remplace la configuration (validee par l'appelant), la persiste et
    // reconfigure les cartes. Rend false si l'ecriture NVS echoue.
    bool applyConfig(const IoExpander::Config& cfg);

    // Commande une sortie (eclairage, ventilation, TOR). Rend false si le
    // binding n'est pas une sortie active.
    bool setOutput(uint8_t bindingIndex, bool on);

    // Lecture d'etat pour l'API et l'affichage.
    State   inputState(uint8_t bindingIndex) const;
    bool    outputCommand(uint8_t bindingIndex) const;
    bool    valveMissing(uint8_t bindingIndex) const;
    bool    boardReady(uint8_t boardIndex) const;

private:
    IoExpander::Config _cfg;
    RelaisManager*     _relais = nullptr;

    uint16_t _gpio[IoExpander::MAX_BOARDS]  = {};  // dernieres entrees lues (16 bits)
    uint16_t _olat[IoExpander::MAX_BOARDS]  = {};  // sorties courantes (16 bits)
    bool     _boardReady[IoExpander::MAX_BOARDS] = {};

    State _inState[IoExpander::MAX_BINDINGS]  = {};
    bool  _outCmd[IoExpander::MAX_BINDINGS]   = {};
    bool  _missing[IoExpander::MAX_BINDINGS]  = {};

    uint32_t _nextPollMs = 0U;

    void configureAllBoards();
    void configureBoard(uint8_t boardIndex);
    void applyOutputs(uint8_t boardIndex);
    void poll(uint8_t nbZones);

    // I2C MCP23017 sur le bus partage (RELAY_WIRE_BUS).
    bool wr(uint8_t addr, uint8_t reg, uint8_t val);
    bool rd(uint8_t addr, uint8_t reg, uint8_t& val);
};
