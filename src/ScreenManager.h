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
//    SLEEP  → écran éteint, LED animée en signe de vie
//
//  Réveil sur :
//    - touch (appelé par DisplayManager::handleTouch)
//    - changement d'état relais (EventBus::displayDirty)
//    - appel explicite wakeUp()
//
//  Modes LED (config) :
//    0 = off
//    1 = flash discret vert
//    2 = respiration verte
//    3 = arc-en-ciel progressif
//    4 = alternance vert / bleu
//    Relais actif → override LED rouge clignotant quelle que soit config
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
    ConfigManager* _config       = nullptr;
    bool           _sleeping     = false;
    uint32_t       _lastActivity = 0;

    // LED
    uint32_t _ledTimer        = 0;
    uint8_t  _ledPhase        = 0;
    bool     _relayWasActive  = false;

    static constexpr uint8_t LED_CH_RED   = 5;
    static constexpr uint8_t LED_CH_GREEN = 6;
    static constexpr uint8_t LED_CH_BLUE  = 7;

    void screenOn();
    void screenOff();
    void updateLed(bool relayActive);
    void ledOff();
    void ledSet(bool r, bool g, bool b);
    void ledSetBrightness(uint8_t r, uint8_t g, uint8_t b);
};
