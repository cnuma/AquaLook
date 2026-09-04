#include "IoExpanderManager.h"

#include <Wire.h>
#include "config.h"
#include "EventLog.h"
#include "RelaisManager.h"

namespace {
// Registres MCP23017, mapping IOCON.BANK=0 (defaut).
constexpr uint8_t REG_IODIRA = 0x00;
constexpr uint8_t REG_IODIRB = 0x01;
constexpr uint8_t REG_GPPUA  = 0x0C;
constexpr uint8_t REG_GPPUB  = 0x0D;
constexpr uint8_t REG_GPIOA  = 0x12;
constexpr uint8_t REG_GPIOB  = 0x13;
constexpr uint8_t REG_OLATA  = 0x14;
constexpr uint8_t REG_OLATB  = 0x15;
}  // namespace

void IoExpanderManager::begin(RelaisManager* relais) {
    _relais = relais;
    IoExpander::load(_cfg);   // defauts surs si rien n'est stocke
    if (_cfg.enabled) {
        configureAllBoards();
        EventLog::log(LOG_INFO, "IoExpander: actif, %u binding(s)",
                      (unsigned)IoExpander::MAX_BINDINGS);
    }
}

bool IoExpanderManager::applyConfig(const IoExpander::Config& cfg) {
    _cfg = cfg;
    if (!IoExpander::save(_cfg)) return false;
    // Repartir d'un etat propre : on ne sait pas ce que la nouvelle config
    // change, on reconfigure tout et on oublie les etats precedents.
    for (uint8_t i = 0; i < IoExpander::MAX_BINDINGS; ++i) {
        _inState[i] = ST_UNKNOWN;
        _outCmd[i] = false;
        _missing[i] = false;
    }
    _nextPollMs = 0U;
    if (_cfg.enabled) configureAllBoards();
    return true;
}

// ── Configuration des cartes ──────────────────────────────────

void IoExpanderManager::configureAllBoards() {
    for (uint8_t b = 0; b < IoExpander::MAX_BOARDS; ++b) {
        _boardReady[b] = false;
        if (_cfg.boards[b].enabled && IoExpander::validAddress(_cfg.boards[b].i2cAddress)) {
            configureBoard(b);
        }
    }
}

void IoExpanderManager::configureBoard(uint8_t boardIndex) {
    const IoExpander::Board& board = _cfg.boards[boardIndex];

    // Par defaut toutes les broches en entree (bit=1) : un MCP non declare
    // reste passif, jamais en sortie par accident. Pas de pull-up par defaut.
    uint16_t iodir = 0xFFFFU;
    uint16_t gppu  = 0x0000U;
    uint16_t olat  = 0x0000U;

    for (uint8_t i = 0; i < IoExpander::MAX_BINDINGS; ++i) {
        const IoExpander::Binding& bd = _cfg.bindings[i];
        if (!bd.enabled || bd.boardIndex != boardIndex || bd.pin >= IoExpander::PINS_PER_BOARD) {
            continue;
        }
        const uint16_t mask = (uint16_t)1U << bd.pin;
        if (bd.direction == IoExpander::DIR_OUTPUT) {
            iodir &= ~mask;                 // 0 = sortie
            // Etat initial : inactif. La valeur physique depend du niveau actif.
            if (bd.activeLevel == 0) olat |= mask;   // actif=0 -> repos=1
        } else {
            iodir |= mask;                  // 1 = entree
            if (bd.pullup) gppu |= mask;
        }
    }

    _olat[boardIndex] = olat;

    const uint8_t addr = board.i2cAddress;
    bool ok = true;
    ok &= wr(addr, REG_IODIRA, (uint8_t)(iodir & 0xFF));
    ok &= wr(addr, REG_IODIRB, (uint8_t)(iodir >> 8));
    ok &= wr(addr, REG_GPPUA,  (uint8_t)(gppu & 0xFF));
    ok &= wr(addr, REG_GPPUB,  (uint8_t)(gppu >> 8));
    ok &= wr(addr, REG_OLATA,  (uint8_t)(olat & 0xFF));
    ok &= wr(addr, REG_OLATB,  (uint8_t)(olat >> 8));

    _boardReady[boardIndex] = ok;
    EventLog::log(ok ? LOG_INFO : LOG_WARN,
                  "IoExpander: carte %u @0x%02X %s (iodir=0x%04X)",
                  (unsigned)boardIndex, addr, ok ? "prete" : "absente",
                  (unsigned)iodir);
}

void IoExpanderManager::applyOutputs(uint8_t boardIndex) {
    if (!_boardReady[boardIndex]) return;
    const uint8_t addr = _cfg.boards[boardIndex].i2cAddress;
    wr(addr, REG_OLATA, (uint8_t)(_olat[boardIndex] & 0xFF));
    wr(addr, REG_OLATB, (uint8_t)(_olat[boardIndex] >> 8));
}

// ── Scrutation ────────────────────────────────────────────────

void IoExpanderManager::update(uint32_t nowMs, uint8_t nbZones) {
    if (!_cfg.enabled) return;
    if ((int32_t)(nowMs - _nextPollMs) < 0) return;
    const uint16_t period = _cfg.pollSeconds ? _cfg.pollSeconds : 5U;
    _nextPollMs = nowMs + (uint32_t)period * 1000U;
    poll(nbZones);
}

void IoExpanderManager::poll(uint8_t nbZones) {
    // 1) Lire les entrees de chaque carte prete.
    for (uint8_t b = 0; b < IoExpander::MAX_BOARDS; ++b) {
        if (!_boardReady[b]) {
            // Tenter une reconfiguration : la carte a pu apparaitre.
            if (_cfg.boards[b].enabled && IoExpander::validAddress(_cfg.boards[b].i2cAddress)) {
                configureBoard(b);
            }
            if (!_boardReady[b]) continue;
        }
        const uint8_t addr = _cfg.boards[b].i2cAddress;
        uint8_t a = 0, bb = 0;
        const bool okA = rd(addr, REG_GPIOA, a);
        const bool okB = rd(addr, REG_GPIOB, bb);
        if (!okA || !okB) {
            _boardReady[b] = false;   // carte disparue : on retentera
            continue;
        }
        _gpio[b] = (uint16_t)a | ((uint16_t)bb << 8);
    }

    // 2) Evaluer chaque binding.
    for (uint8_t i = 0; i < IoExpander::MAX_BINDINGS; ++i) {
        const IoExpander::Binding& bd = _cfg.bindings[i];
        if (!bd.enabled || bd.boardIndex >= IoExpander::MAX_BOARDS) {
            _inState[i] = ST_UNKNOWN;
            continue;
        }
        if (!_boardReady[bd.boardIndex]) { _inState[i] = ST_UNKNOWN; continue; }

        if (bd.direction == IoExpander::DIR_INPUT) {
            const uint8_t raw = (uint8_t)((_gpio[bd.boardIndex] >> bd.pin) & 1U);
            const bool active = (raw == bd.activeLevel);

            if (bd.role == IoExpander::ROLE_VALVE_PRESENCE) {
                // Ne se lit qu'au repos : une zone en arrosage alimente la
                // bobine, la fuite de repos n'est plus observable.
                const bool zoneActive =
                    _relais && bd.zone < nbZones && _relais->getState(bd.zone);
                if (zoneActive) {
                    _inState[i] = ST_UNKNOWN;   // presence non evaluable maintenant
                } else {
                    const State st = active ? ST_ACTIVE : ST_INACTIVE;
                    // Transition vers "absente" : la vanne attendue a disparu.
                    if (st == ST_INACTIVE && !_missing[i]) {
                        _missing[i] = true;
                        EventLog::log(LOG_WARN,
                                      "IoExpander: vanne ABSENTE zone=%u (binding %u)",
                                      (unsigned)bd.zone, (unsigned)i);
                    } else if (st == ST_ACTIVE) {
                        _missing[i] = false;
                    }
                    _inState[i] = st;
                }
            } else {
                _inState[i] = active ? ST_ACTIVE : ST_INACTIVE;
            }
        }
        // Les sorties gardent l'etat commande par setOutput ; rien a lire.
    }
}

// ── Commande des sorties ──────────────────────────────────────

bool IoExpanderManager::setOutput(uint8_t bindingIndex, bool on) {
    if (bindingIndex >= IoExpander::MAX_BINDINGS) return false;
    const IoExpander::Binding& bd = _cfg.bindings[bindingIndex];
    if (!bd.enabled || bd.direction != IoExpander::DIR_OUTPUT) return false;
    if (bd.boardIndex >= IoExpander::MAX_BOARDS || !_boardReady[bd.boardIndex]) return false;

    _outCmd[bindingIndex] = on;
    const uint16_t mask = (uint16_t)1U << bd.pin;
    // Niveau physique = niveau actif quand on veut ON, son complement sinon.
    const bool physical = on ? (bd.activeLevel != 0) : (bd.activeLevel == 0);
    if (physical) _olat[bd.boardIndex] |= mask;
    else          _olat[bd.boardIndex] &= ~mask;
    applyOutputs(bd.boardIndex);
    return true;
}

// ── Accesseurs ────────────────────────────────────────────────

IoExpanderManager::State IoExpanderManager::inputState(uint8_t i) const {
    return i < IoExpander::MAX_BINDINGS ? _inState[i] : ST_UNKNOWN;
}
bool IoExpanderManager::outputCommand(uint8_t i) const {
    return i < IoExpander::MAX_BINDINGS ? _outCmd[i] : false;
}
bool IoExpanderManager::valveMissing(uint8_t i) const {
    return i < IoExpander::MAX_BINDINGS ? _missing[i] : false;
}
bool IoExpanderManager::boardReady(uint8_t b) const {
    return b < IoExpander::MAX_BOARDS ? _boardReady[b] : false;
}

// ── I2C ───────────────────────────────────────────────────────

bool IoExpanderManager::wr(uint8_t addr, uint8_t reg, uint8_t val) {
    RELAY_WIRE_BUS.beginTransmission(addr);
    RELAY_WIRE_BUS.write(reg);
    RELAY_WIRE_BUS.write(val);
    return RELAY_WIRE_BUS.endTransmission() == 0;
}

bool IoExpanderManager::rd(uint8_t addr, uint8_t reg, uint8_t& val) {
    RELAY_WIRE_BUS.beginTransmission(addr);
    RELAY_WIRE_BUS.write(reg);
    if (RELAY_WIRE_BUS.endTransmission(false) != 0) return false;
    if (RELAY_WIRE_BUS.requestFrom((int)addr, 1) != 1) return false;
    val = (uint8_t)RELAY_WIRE_BUS.read();
    return true;
}
