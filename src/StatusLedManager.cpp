#include "StatusLedManager.h"

static constexpr uint8_t CH_RED = 5;
static constexpr uint8_t CH_GREEN = 6;
static constexpr uint8_t CH_BLUE = 7;

void StatusLedManager::begin() {
    _bootMs = millis();
    ledcSetup(CH_RED, 5000, 8);
    ledcSetup(CH_GREEN, 5000, 8);
    ledcSetup(CH_BLUE, 5000, 8);
    ledcAttachPin(PIN_RED, CH_RED);
    ledcAttachPin(PIN_GREEN, CH_GREEN);
    ledcAttachPin(PIN_BLUE, CH_BLUE);
    setRgb(0, 0, 12);
}

void StatusLedManager::update(bool wifiConnected, bool ntpSynced, bool wateringActive) {
    const uint32_t now = millis();
    const bool graceElapsed = (now - _bootMs) >= STARTUP_GRACE_MS;

    State requested = State::NORMAL;
    if (graceElapsed && !wifiConnected) requested = State::ERROR_WIFI;
    else if (graceElapsed && !ntpSynced) requested = State::ERROR_NTP;
    else if (wateringActive) requested = State::WATERING;
    setState(requested);

    if (_state == State::ERROR_WIFI || _state == State::ERROR_NTP) {
        const bool on = ((now / ERROR_BLINK_MS) % 2UL) == 0;
        setRgb(on ? 255 : 0, 0, 0);
    } else if (_state == State::WATERING) {
        setRgb(0, 0, breathingLevel(now));
    } else {
        setRgb(0, 0, 12);
    }
}

void StatusLedManager::setState(State state) {
    _state = state;
}

uint8_t StatusLedManager::breathingLevel(uint32_t now) const {
    const uint32_t phase = now % BREATH_PERIOD_MS;
    const uint32_t half = BREATH_PERIOD_MS / 2UL;
    uint32_t level = phase < half
        ? (phase * 255UL) / half
        : ((BREATH_PERIOD_MS - phase) * 255UL) / half;
    constexpr uint8_t minLevel = 18;
    return minLevel + (uint8_t)((level * (255UL - minLevel)) / 255UL);
}

void StatusLedManager::setRgb(uint8_t red, uint8_t green, uint8_t blue) {
    ledcWrite(CH_RED, 255 - red);
    ledcWrite(CH_GREEN, 255 - green);
    ledcWrite(CH_BLUE, 255 - blue);
}
