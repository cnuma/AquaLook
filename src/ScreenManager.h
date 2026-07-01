#pragma once
#include <Arduino.h>
#include "ConfigManager.h"

// ═══════════════════════════════════════════════════════════════
//  ScreenManager — veille écran + LED "signe de vie"
//
//  Rétroéclairage : pin TFT_BL (21) — HIGH=allumé, LOW=éteint
//  LED RGB arrière : logique inverse (LOW=allumé)
//    R=4, G=16, B=17
//
//  États :
//    AWAKE  → écran allumé, LED éteinte
//    SLEEP  → écran éteint, LED animée
//
//  Priorités LED en veille :
//    1. Anomalie Wi-Fi / heure non synchronisée → rouge clignotant
//    2. Vanne ouverte → respiration bleue rapide
//    3. Fonctionnement normal → mode choisi dans la page Web
//
//  Modes LED configurables :
//    0 = off
//    1 = flash discret vert
//    2 = respiration verte
//    3 = arc-en-ciel lent
//    4 = alternance vert / bleu
// ═══════════════════════════════════════════════════════════════

#define PIN_TFT_BL    21
#define PIN_LED_RED    4
#define PIN_LED_GREEN 16
#define PIN_LED_BLUE  17

class ScreenManager {
public:
    void begin(ConfigManager* config = nullptr);
    void update(bool anyRelayActive);

    void wakeUp();
    bool isAsleep() const { return _sleeping; }

private:
    ConfigManager* _config       = nullptr;
    bool           _sleeping     = false;
    uint32_t       _lastActivity = 0;

    uint32_t _ledTimer      = 0;
    uint8_t  _ledPhase      = 0;
    bool     _relayWasActive = false;
    uint8_t  _wateringStep   = 0;

    static constexpr uint32_t LED_ERROR_GRACE_MS = 30000UL;

    void screenOn();
    void screenOff();
    void updateLed(bool relayActive);
    void updateWateringBreath(uint32_t now);
    bool hasSystemError() const;
    void ledOff();
    void ledSet(bool r, bool g, bool b);
};
