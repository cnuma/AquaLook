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
//    SLEEP  → écran éteint, LED anime en signe de vie
//
//  Réveil sur :
//    - touch (appelé par DisplayManager::handleTouch)
//    - changement d'état relais (EventBus::displayDirty)
//    - appel explicite wakeUp()
//
//  Modes LED (config) :
//    0 = off        → LED toujours éteinte en veille
//    1 = pulse      → battement lent vert, 1 cycle/4s
//    2 = flash      → flash court 100ms toutes les 5s
//    Relais actif   → override LED rouge clignotant quelle que soit config
// ═══════════════════════════════════════════════════════════════

// Pins CYD ESP32-2432S028
#define PIN_TFT_BL    21
#define PIN_LED_RED    4
#define PIN_LED_GREEN 16
#define PIN_LED_BLUE  17

class ScreenManager {
public:
    void begin(ConfigManager* config = nullptr);
    void update(bool anyRelayActive);

    // Réveil explicite — appelé par touch ou EventBus
    void wakeUp();

    bool isAsleep() const { return _sleeping; }

private:
    ConfigManager* _config    = nullptr;
    bool           _sleeping  = false;
    uint32_t       _lastActivity = 0;  // millis() du dernier événement

    // LED
    uint32_t _ledTimer   = 0;
    uint8_t  _ledPhase   = 0;    // phase pour pulse/flash
    bool     _relayWasActive = false;

    void screenOn();
    void screenOff();
    void updateLed(bool relayActive);
    void ledOff();
    void ledSet(bool r, bool g, bool b);
};
