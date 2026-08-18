#pragma once
#include <Arduino.h>
#include "ConfigManager.h"

#define PIN_TFT_BL    21
#define PIN_LED_RED    4
#define PIN_LED_GREEN 16
#define PIN_LED_BLUE  17

class ScreenManager {
public:
    void begin(ConfigManager* config = nullptr);
    // wifiSearching : WiFi ni connecte ni en portail captif (connexion en
    // cours ou reconnexion apres detection zombie) — priorite d'affichage
    // juste sous l'arrosage actif, au-dessus du mode LED normal.
    void update(bool anyRelayActive, bool wifiSearching);
    void wakeUp();

    bool isAsleep() const { return _sleeping; }

private:
    ConfigManager* _config = nullptr;
    bool _sleeping = false;
    uint32_t _lastActivity = 0;

    uint32_t _ledTimer = 0;
    uint8_t _ledPhase = 0;
    bool _relayWasActive = false;

    uint8_t _normalLedRed = 0;
    uint8_t _normalLedGreen = 0;
    uint8_t _normalLedBlue = 0;

    // Mise a jour en attente (firmware ou ressources Web) : chargee une
    // seule fois, jamais rafraichie en cours de fonctionnement. Comme pour
    // DisplayPlanningDecor::loadUpdateState(), c'est suffisant : toute
    // verification de mise a jour redemarre le module (UpdateCheckScheduler
    // via BootLoopGuard::restartDeliberately()), donc l'etat ne peut
    // changer qu'au demarrage suivant.
    bool _updateStateLoaded = false;
    bool _updatePending = false;
    void loadUpdateState();

    static constexpr uint8_t LED_CH_RED = 5;
    static constexpr uint8_t LED_CH_GREEN = 6;
    static constexpr uint8_t LED_CH_BLUE = 7;

    void screenOn();
    void screenOff();
    void updateLed(bool relayActive, bool wifiSearching);
    void renderLed();
    void ledOff();
    void ledSet(bool r, bool g, bool b);
    void ledSetBrightness(uint8_t r, uint8_t g, uint8_t b);
};
