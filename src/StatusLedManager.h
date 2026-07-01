#pragma once
#include <Arduino.h>

// LED RGB arrière de la carte ESP32-2432S028R (Cheap Yellow Display).
// Broches actives à l'état bas.
class StatusLedManager {
public:
    enum class State : uint8_t {
        NORMAL,
        WATERING,
        ERROR_WIFI,
        ERROR_NTP
    };

    void begin();
    void update(bool wifiConnected, bool ntpSynced, bool wateringActive);
    State getState() const { return _state; }

private:
    static constexpr uint8_t PIN_RED   = 4;
    static constexpr uint8_t PIN_GREEN = 16;
    static constexpr uint8_t PIN_BLUE  = 17;

    static constexpr uint32_t STARTUP_GRACE_MS = 30000UL;
    static constexpr uint32_t ERROR_BLINK_MS   = 350UL;
    static constexpr uint32_t BREATH_PERIOD_MS = 1200UL;

    State _state = State::NORMAL;
    uint32_t _bootMs = 0;

    void setRgb(uint8_t red, uint8_t green, uint8_t blue);
    void setState(State state);
    uint8_t breathingLevel(uint32_t now) const;
};
