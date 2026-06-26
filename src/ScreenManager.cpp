#include "ScreenManager.h"

// ─────────────────────────────────────────────────────────────
void ScreenManager::begin(ConfigManager* config) {
    _config = config;

    // Pins rétroéclairage
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);  // allumé au boot

    // Pins LED RGB (logique inverse)
    pinMode(PIN_LED_RED,   OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE,  OUTPUT);
    ledOff();

    _lastActivity = millis();
    _sleeping     = false;

    Serial.println("[Screen] ScreenManager OK");
}

// ─────────────────────────────────────────────────────────────
//  update() — appelé dans loop()
// ─────────────────────────────────────────────────────────────
void ScreenManager::update(bool anyRelayActive) {
    const uint32_t now = millis();

    // Un relais vient de s'activer → réveil forcé
    if (anyRelayActive && !_relayWasActive) {
        wakeUp();
    }
    _relayWasActive = anyRelayActive;

    uint8_t timeoutMin = _config ? _config->system().screenTimeoutMin : 5;

    if (!_sleeping) {
        // Vérifier si on doit passer en veille
        if (timeoutMin > 0 &&
            (now - _lastActivity) >= (uint32_t)timeoutMin * 60000UL) {
            screenOff();
        }
    } else {
        // En veille — animer la LED signe de vie
        updateLed(anyRelayActive);
    }
}

// ─────────────────────────────────────────────────────────────
//  wakeUp() — réveil depuis touch ou événement externe
// ─────────────────────────────────────────────────────────────
void ScreenManager::wakeUp() {
    _lastActivity = millis();
    if (_sleeping) {
        screenOn();
    }
}

// ─────────────────────────────────────────────────────────────
//  Contrôle rétroéclairage
// ─────────────────────────────────────────────────────────────
void ScreenManager::screenOn() {
    _sleeping = false;
    digitalWrite(PIN_TFT_BL, HIGH);
    ledOff();
    Serial.println("[Screen] Réveil");
}

void ScreenManager::screenOff() {
    _sleeping  = true;
    _ledTimer  = 0;
    _ledPhase  = 0;
    digitalWrite(PIN_TFT_BL, LOW);
    Serial.println("[Screen] Veille");
}

// ─────────────────────────────────────────────────────────────
//  LED signe de vie
//
//  Mode 0 — off : rien
//  Mode 1 — pulse : vert monte/descend sur 4s via PWM simulé
//             (pas de PWM sur ces pins → on fait une succession
//              de ON/OFF courts pour créer l'illusion)
//  Mode 2 — flash : vert allumé 100ms toutes les 5s
//  Relay actif → rouge clignotant 500ms, override tout
// ─────────────────────────────────────────────────────────────
// Séquence arc-en-ciel : 6 couleurs RVB pures
static const bool LED_RAINBOW[6][3] = {
    {false, true,  false},  // vert
    {false, true,  true },  // cyan
    {false, false, true },  // bleu
    {true,  false, true },  // violet
    {true,  false, false},  // rouge
    {true,  true,  false},  // jaune
};

// Séquence bicolore : vert + bleu
static const bool LED_BICOLOR[2][3] = {
    {false, true,  false},  // vert
    {false, false, true },  // bleu
};

void ScreenManager::updateLed(bool relayActive) {
    const uint32_t now = millis();
    uint8_t mode = _config ? _config->system().ledMode : 1;

    // Override relais actif : LED rouge clignote toutes les 500ms — quel que soit le mode
    if (relayActive) {
        if (now - _ledTimer >= 500) {
            _ledTimer = now;
            _ledPhase ^= 1;
            if (_ledPhase) ledSet(true, false, false);
            else           ledOff();
        }
        return;
    }

    switch (mode) {
        case 0:
            // Off total
            ledOff();
            break;

        case 1:
            // Flash discret vert — 150ms allumé toutes les 4s
            if (_ledPhase == 0 && (now - _ledTimer) >= 4000) {
                _ledTimer = now; _ledPhase = 1;
                ledSet(false, true, false);
            } else if (_ledPhase == 1 && (now - _ledTimer) >= 150) {
                _ledPhase = 0; ledOff();
            }
            break;

        case 2:
            // Respiration verte — pseudo-PWM logiciel
            // Cycle 2s : 20 paliers ON/OFF de durée croissante puis décroissante
            {
                static uint8_t breathStep = 0;
                // Durées ON pour chaque palier (ms) — simule une courbe sinusoïdale
                static const uint8_t BREATH_ON[]  = {5,10,20,35,55,75,95,110,120,125,
                                                     125,120,110,95,75,55,35,20,10,5};
                static const uint8_t BREATH_OFF[] = {120,115,105,90,70,50,30,15,5,0,
                                                      0,5,15,30,50,70,90,105,115,120};
                uint8_t onMs  = BREATH_ON[breathStep];
                uint8_t offMs = BREATH_OFF[breathStep];

                if (_ledPhase == 0 && (now - _ledTimer) >= offMs) {
                    _ledTimer = now; _ledPhase = 1;
                    if (onMs > 0) ledSet(false, true, false);
                } else if (_ledPhase == 1 && (now - _ledTimer) >= onMs) {
                    _ledPhase = 0; ledOff();
                    breathStep = (breathStep + 1) % 20;
                }
            }
            break;

        case 3:
            // Arc-en-ciel lent — change de couleur toutes les 2s
            if (now - _ledTimer >= 2000) {
                _ledTimer = now;
                _ledPhase = (_ledPhase + 1) % 6;
                ledSet(LED_RAINBOW[_ledPhase][0],
                       LED_RAINBOW[_ledPhase][1],
                       LED_RAINBOW[_ledPhase][2]);
            }
            break;

        case 4:
            // Bicolore séquence — alterne vert/bleu toutes les 3s
            if (now - _ledTimer >= 3000) {
                _ledTimer = now;
                _ledPhase ^= 1;
                ledSet(LED_BICOLOR[_ledPhase][0],
                       LED_BICOLOR[_ledPhase][1],
                       LED_BICOLOR[_ledPhase][2]);
            }
            break;

        default:
            ledOff();
            break;
    }
}

// ─────────────────────────────────────────────────────────────
//  Helpers LED (logique inverse : LOW = allumé)
// ─────────────────────────────────────────────────────────────
void ScreenManager::ledOff() {
    digitalWrite(PIN_LED_RED,   HIGH);
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_BLUE,  HIGH);
}

void ScreenManager::ledSet(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_RED,   r ? LOW : HIGH);
    digitalWrite(PIN_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(PIN_LED_BLUE,  b ? LOW : HIGH);
}
