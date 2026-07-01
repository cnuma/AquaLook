#include "ScreenManager.h"
#include <WiFi.h>
#include <time.h>

void ScreenManager::begin(ConfigManager* config) {
    _config = config;

    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);

    pinMode(PIN_LED_RED,   OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE,  OUTPUT);
    ledOff();

    _lastActivity = millis();
    _sleeping     = false;

    Serial.println("[Screen] ScreenManager OK");
}

void ScreenManager::update(bool anyRelayActive) {
    const uint32_t now = millis();

    if (anyRelayActive && !_relayWasActive) {
        wakeUp();
        _ledTimer = 0;
        _ledPhase = 0;
        _wateringStep = 0;
    }
    _relayWasActive = anyRelayActive;

    const uint8_t timeoutMin = _config ? _config->system().screenTimeoutMin : 5;

    if (!_sleeping && timeoutMin > 0 &&
        (now - _lastActivity) >= (uint32_t)timeoutMin * 60000UL) {
        screenOff();
    }

    // Les états prioritaires (erreur et arrosage) doivent rester visibles
    // même lorsque l'écran est allumé. Les modes configurables restent,
    // eux, des signes de vie réservés à la veille.
    updateLed(anyRelayActive);
}

void ScreenManager::wakeUp() {
    _lastActivity = millis();
    if (_sleeping) screenOn();
}

void ScreenManager::screenOn() {
    _sleeping = false;
    digitalWrite(PIN_TFT_BL, HIGH);
    ledOff();
    Serial.println("[Screen] Réveil");
}

void ScreenManager::screenOff() {
    _sleeping     = true;
    _ledTimer     = 0;
    _ledPhase     = 0;
    _wateringStep = 0;
    digitalWrite(PIN_TFT_BL, LOW);
    Serial.println("[Screen] Veille");
}

static const bool LED_RAINBOW[6][3] = {
    {false, true,  false},
    {false, true,  true },
    {false, false, true },
    {true,  false, true },
    {true,  false, false},
    {true,  true,  false},
};

static const bool LED_BICOLOR[2][3] = {
    {false, true,  false},
    {false, false, true },
};

void ScreenManager::updateLed(bool relayActive) {
    const uint32_t now = millis();
    const uint8_t mode = _config ? _config->system().ledMode : 1;

    // Priorité 1 : rouge réservé aux anomalies.
    if (hasSystemError()) {
        if (now - _ledTimer >= 350UL) {
            _ledTimer = now;
            _ledPhase ^= 1;
            if (_ledPhase) ledSet(true, false, false);
            else           ledOff();
        }
        return;
    }

    // Priorité 2 : respiration bleue rapide dès qu'une vanne est ouverte,
    // que l'écran soit allumé ou en veille.
    if (relayActive) {
        updateWateringBreath(now);
        return;
    }

    // Ecran allumé et aucun état prioritaire : LED éteinte.
    if (!_sleeping) {
        ledOff();
        return;
    }

    // Veille normale : comportement choisi dans la page Web.
    switch (mode) {
        case 0:
            ledOff();
            break;

        case 1:
            if (_ledPhase == 0 && (now - _ledTimer) >= 4000UL) {
                _ledTimer = now;
                _ledPhase = 1;
                ledSet(false, true, false);
            } else if (_ledPhase == 1 && (now - _ledTimer) >= 150UL) {
                _ledPhase = 0;
                ledOff();
            }
            break;

        case 2: {
            static uint8_t breathStep = 0;
            static const uint8_t BREATH_ON[] = {
                5,10,20,35,55,75,95,110,120,125,
                125,120,110,95,75,55,35,20,10,5
            };
            static const uint8_t BREATH_OFF[] = {
                120,115,105,90,70,50,30,15,5,0,
                0,5,15,30,50,70,90,105,115,120
            };
            const uint8_t onMs  = BREATH_ON[breathStep];
            const uint8_t offMs = BREATH_OFF[breathStep];

            if (_ledPhase == 0 && (now - _ledTimer) >= offMs) {
                _ledTimer = now;
                _ledPhase = 1;
                if (onMs > 0) ledSet(false, true, false);
            } else if (_ledPhase == 1 && (now - _ledTimer) >= onMs) {
                _ledTimer = now;
                _ledPhase = 0;
                ledOff();
                breathStep = (breathStep + 1) % 20;
            }
            break;
        }

        case 3:
            if (now - _ledTimer >= 2000UL) {
                _ledTimer = now;
                _ledPhase = (_ledPhase + 1) % 6;
                ledSet(LED_RAINBOW[_ledPhase][0],
                       LED_RAINBOW[_ledPhase][1],
                       LED_RAINBOW[_ledPhase][2]);
            }
            break;

        case 4:
            if (now - _ledTimer >= 3000UL) {
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

void ScreenManager::updateWateringBreath(uint32_t now) {
    static const uint8_t BREATH_ON[] = {
        8,16,28,42,58,74,88,100,108,112,
        112,108,100,88,74,58,42,28,16,8
    };
    static const uint8_t BREATH_OFF[] = {
        70,62,52,42,32,24,16,10,6,4,
        4,6,10,16,24,32,42,52,62,70
    };

    const uint8_t onMs  = BREATH_ON[_wateringStep];
    const uint8_t offMs = BREATH_OFF[_wateringStep];

    if (_ledPhase == 0 && (now - _ledTimer) >= offMs) {
        _ledTimer = now;
        _ledPhase = 1;
        ledSet(false, false, true);
    } else if (_ledPhase == 1 && (now - _ledTimer) >= onMs) {
        _ledTimer = now;
        _ledPhase = 0;
        ledOff();
        _wateringStep = (_wateringStep + 1) % 20;
    }
}

bool ScreenManager::hasSystemError() const {
    if (millis() < LED_ERROR_GRACE_MS) return false;
    if (WiFi.status() != WL_CONNECTED) return true;

    const time_t now = time(nullptr);
    return now < 1700000000;
}

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
