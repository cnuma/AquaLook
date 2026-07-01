#include "ScreenManager.h"
#include "SystemHealth.h"
#include <WiFi.h>
#include <time.h>

void ScreenManager::begin(ConfigManager* config) {
    _config = config;

    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);

    ledcSetup(LED_CH_RED, 5000, 8);
    ledcSetup(LED_CH_GREEN, 5000, 8);
    ledcSetup(LED_CH_BLUE, 5000, 8);
    ledcAttachPin(PIN_LED_RED, LED_CH_RED);
    ledcAttachPin(PIN_LED_GREEN, LED_CH_GREEN);
    ledcAttachPin(PIN_LED_BLUE, LED_CH_BLUE);
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
    }
    _relayWasActive = anyRelayActive;

    const uint8_t timeoutMin = _config ? _config->system().screenTimeoutMin : 5;

    if (!_sleeping && timeoutMin > 0 &&
        (now - _lastActivity) >= (uint32_t)timeoutMin * 60000UL) {
        screenOff();
    }

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
    _sleeping = true;
    _ledTimer = 0;
    _ledPhase = 0;
    digitalWrite(PIN_TFT_BL, LOW);
    Serial.println("[Screen] Veille");
}

static const uint8_t LED_RAINBOW[6][3] = {
    {  0, 255,   0},
    {  0, 255, 255},
    {  0,   0, 255},
    {255,   0, 255},
    {255,   0,   0},
    {255, 255,   0},
};

static const bool LED_BICOLOR[2][3] = {
    {false, true,  false},
    {false, false, true },
};

void ScreenManager::updateLed(bool relayActive) {
    const uint32_t now = millis();
    const uint8_t mode = _config ? _config->system().ledMode : 1;

    if (hasSystemError()) {
        if (now - _ledTimer >= 350UL) {
            _ledTimer = now;
            _ledPhase ^= 1;
            if (_ledPhase) ledSet(true, false, false);
            else           ledOff();
        }
        return;
    }

    if (relayActive) {
        updateWateringBreath(now);
        return;
    }

    if (!_sleeping) {
        ledOff();
        return;
    }

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
                _ledTimer = now;
                _ledPhase = 0;
                ledOff();
            }
            break;

        case 2: {
            const uint32_t phase = now % 4000UL;
            const uint32_t half  = 2000UL;
            uint32_t level = phase < half
                           ? (phase * 180UL) / half
                           : ((4000UL - phase) * 180UL) / half;
            ledSetBrightness(0, (uint8_t)(12UL + level), 0);
            break;
        }

        case 3: {
            constexpr uint32_t STEP_MS = 2000UL;
            constexpr uint32_t CYCLE_MS = STEP_MS * 6UL;
            const uint32_t cyclePos = now % CYCLE_MS;
            const uint8_t from = cyclePos / STEP_MS;
            const uint8_t to   = (from + 1U) % 6U;
            const uint32_t local = cyclePos % STEP_MS;

            const uint8_t r = LED_RAINBOW[from][0] +
                (int32_t)(LED_RAINBOW[to][0] - LED_RAINBOW[from][0]) * (int32_t)local / (int32_t)STEP_MS;
            const uint8_t g = LED_RAINBOW[from][1] +
                (int32_t)(LED_RAINBOW[to][1] - LED_RAINBOW[from][1]) * (int32_t)local / (int32_t)STEP_MS;
            const uint8_t b = LED_RAINBOW[from][2] +
                (int32_t)(LED_RAINBOW[to][2] - LED_RAINBOW[from][2]) * (int32_t)local / (int32_t)STEP_MS;

            ledSetBrightness(r, g, b);
            break;
        }

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
    constexpr uint32_t PERIOD_MS = 1200UL;
    constexpr uint32_t HALF_MS   = PERIOD_MS / 2UL;
    constexpr uint8_t  MIN_BLUE  = 24;

    const uint32_t phase = now % PERIOD_MS;
    uint32_t level = phase < HALF_MS
                   ? (phase * 255UL) / HALF_MS
                   : ((PERIOD_MS - phase) * 255UL) / HALF_MS;

    const uint8_t blue = MIN_BLUE +
        (uint8_t)((level * (255UL - MIN_BLUE)) / 255UL);

    ledSetBrightness(0, 0, blue);
}

bool ScreenManager::hasSystemError() const {
    return SystemHealth::hasAny();
}

void ScreenManager::ledOff() {
    ledSetBrightness(0, 0, 0);
}

void ScreenManager::ledSet(bool r, bool g, bool b) {
    ledSetBrightness(r ? 255 : 0,
                     g ? 255 : 0,
                     b ? 255 : 0);
}

void ScreenManager::ledSetBrightness(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(LED_CH_RED,   255 - r);
    ledcWrite(LED_CH_GREEN, 255 - g);
    ledcWrite(LED_CH_BLUE,  255 - b);
}
