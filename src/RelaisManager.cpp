#include "RelaisManager.h"
#include "ConfigManager.h"
#include "EventBus.h"

// ─────────────────────────────────────────────────────────────
void RelaisManager::begin(ConfigManager* config) {
    _config = config;

    // Ordre critique XL9535 :
    // 1. Ecrire OUTPUT en premier (etat souhaite avant activation sortie)
    // 2. Configurer en sortie ensuite
    // Si on inverse l'ordre, les pins passent en sortie avec OUTPUT=0x00 (reset HW)
    // ce qui avec logique inverse = tous les relais ON au boot.

    // Valeur OFF selon logique hardware configuree
    // inverse (relayLogic=0) : OFF = bit a 1 -> 0xFF
    // direct  (relayLogic=1) : OFF = bit a 0 -> 0x00
    const bool inv = (_config && _config->relayLogic() == 0);
    _regP0 = inv ? 0xFF : 0x00;
    _regP1 = inv ? 0xFF : 0x00;

    // 1. Fixer l'etat de sortie souhaite (tous OFF)
    writeReg(XL9535_REG_OUTPUT_P0, _regP0);
    writeReg(XL9535_REG_OUTPUT_P1, _regP1);

    // 2. Configurer les ports en sortie
    writeReg(XL9535_REG_CONFIG_P0, 0x00);
    writeReg(XL9535_REG_CONFIG_P1, 0x00);

    // 3. Réécrire OUTPUT après activation des sorties (sécurité)
    writeReg(XL9535_REG_OUTPUT_P0, _regP0);
    writeReg(XL9535_REG_OUTPUT_P1, _regP1);

    // Verification lecture
    uint8_t out0 = readReg(XL9535_REG_OUTPUT_P0);
    uint8_t cfg0 = readReg(XL9535_REG_CONFIG_P0);
    Serial.printf("[Relais] XL9535 @ 0x%02X -- OUTPUT_P0=0x%02X CONFIG_P0=0x%02X logique=%s\n",
                  XL9535_ADDR, out0, cfg0, inv ? "inverse" : "directe");

    // Initialiser les tableaux d'état
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        _state[i]   = false;
        _startMs[i] = 0;
    }

    Serial.printf("[Relais] %d relais physiques -- init OK\n", nbRelaisPhysical());
}

// ─────────────────────────────────────────────────────────────
//  update() — sécurité durée max (invariant I7)
// ─────────────────────────────────────────────────────────────
void RelaisManager::update() {
    const uint32_t now     = millis();
    const uint32_t maxMs   = maxWateringMs();
    const uint8_t  nbZ     = _config ? _config->nbZones() : NB_ZONES;

    for (uint8_t i = 0; i < nbZ; i++) {
        if (_state[i] && _startMs[i] > 0) {
            if (now - _startMs[i] >= maxMs) {
                Serial.printf("[Relais] SECURITE — Zone %d coupée après %lus\n",
                              i + 1, maxMs / 1000UL);
                setRelay(i, false);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  setRelay — activation logique + hardware si zone physique
// ─────────────────────────────────────────────────────────────
void RelaisManager::setRelay(uint8_t relay, bool state) {
    if (relay >= MAX_ZONES) return;

    _state[relay] = state;

    if (state) {
        _startMs[relay] = millis();
    } else {
        _startMs[relay] = 0;
    }

    // Guard nbRelaisPhysical : écrire sur le hardware seulement si relais câblé
    const uint8_t nbPhys = nbRelaisPhysical();
    if (relay < nbPhys) {
        // Logique configurable : 0=inverse (bit=0→ON), 1=direct (bit=1→ON)
        const bool inv = (_config && _config->relayLogic() == 0);
        if (relay < 8) {
            if (inv) {
                if (state) _regP0 &= ~(1 << relay);  // inverse : ON = bit a 0
                else       _regP0 |=  (1 << relay);  // inverse : OFF = bit a 1
            } else {
                if (state) _regP0 |=  (1 << relay);  // direct  : ON = bit a 1
                else       _regP0 &= ~(1 << relay);  // direct  : OFF = bit a 0
            }
        } else {
            uint8_t bit = relay - 8;
            if (inv) {
                if (state) _regP1 &= ~(1 << bit);
                else       _regP1 |=  (1 << bit);
            } else {
                if (state) _regP1 |=  (1 << bit);
                else       _regP1 &= ~(1 << bit);
            }
        }
        applyHardware();
        Serial.printf("[Relais] Zone %d HW : %s (logique %s)\n",
                      relay + 1, state ? "ON" : "OFF", inv ? "inverse" : "directe");
    } else {
        // Zone logique sans hardware — log uniquement
        Serial.printf("[Relais] Zone %d (logique) : %s\n",
                      relay + 1, state ? "ON" : "OFF");
    }

    EventBus::displayDirty = true;
}

bool RelaisManager::getState(uint8_t relay) const {
    if (relay >= MAX_ZONES) return false;
    return _state[relay];
}

// ─────────────────────────────────────────────────────────────
//  Privé
// ─────────────────────────────────────────────────────────────
void RelaisManager::applyHardware() {
    writeReg(XL9535_REG_OUTPUT_P0, _regP0);
    // P1 uniquement si on a plus de 8 relais physiques
    if (nbRelaisPhysical() > 8) {
        writeReg(XL9535_REG_OUTPUT_P1, _regP1);
    }
}

void RelaisManager::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    Wire.write(val);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("[Relais] ERREUR I2C write reg=0x%02X err=%d\n", reg, err);
    }
}

uint8_t RelaisManager::readReg(uint8_t reg) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)XL9535_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

uint8_t RelaisManager::nbRelaisPhysical() const {
    if (_config) return _config->nbRelais();
    return NB_ZONES;  // fallback compile-time
}

uint32_t RelaisManager::maxWateringMs() const {
    if (_config) return (uint32_t)_config->system().maxWateringMin * 60000UL;
    return MAX_WATERING_DURATION_MS;
}
