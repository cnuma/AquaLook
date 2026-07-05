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
    void update(bool anyRelayActive);
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

    static constexpr uint8_t LED_CH_RED = 5;
    static constexpr uint8_t LED_CH_GREEN = 6;
    static constexpr uint8_t LED_CH_BLUE = 7;

    void screenOn();
    void screenOff();
    void updateLed(bool relayActive);
    void renderLed();
    void ledOff();
    void ledSet(bool r, bool g, bool b);
    void ledSetBrightness(uint8_t r, uint8_t g, uint8_t b);
};
