#include "FaultManager.h"

#include <TFT_eSPI.h>
#include "Theme.h"

extern TFT_eSPI* g_tftPtr;

uint32_t FaultManager::_activeMask = 0;
bool FaultManager::_unacknowledged = false;
bool FaultManager::_started = false;

namespace {
constexpr uint32_t ERROR_DARK_BEFORE_MS = 500;
constexpr uint32_t ERROR_FLASH_ON_MS = 120;
constexpr uint32_t ERROR_FLASH_OFF_MS = 120;
constexpr uint32_t ERROR_FLASH_1_START_MS = ERROR_DARK_BEFORE_MS;
constexpr uint32_t ERROR_FLASH_2_START_MS = ERROR_FLASH_1_START_MS + ERROR_FLASH_ON_MS + ERROR_FLASH_OFF_MS;
constexpr uint32_t ERROR_FLASH_3_START_MS = ERROR_FLASH_2_START_MS + ERROR_FLASH_ON_MS + ERROR_FLASH_OFF_MS;
constexpr uint32_t ERROR_PATTERN_PERIOD_MS = ERROR_FLASH_3_START_MS + ERROR_FLASH_ON_MS + 500;
constexpr uint32_t ACK_REMINDER_PERIOD_MS = 5000;
constexpr uint32_t ACK_REMINDER_RED_MS = 300;

constexpr uint32_t LCD_FAULT_REFRESH_MS = 200;
constexpr uint32_t LCD_FAULT_BLINK_MS = 500;
constexpr int16_t LCD_FAULT_X = 91;
constexpr int16_t LCD_FAULT_Y = 4;
constexpr int16_t LCD_FAULT_W = 17;
constexpr int16_t LCD_FAULT_H = 17;

uint32_t s_lastLcdFaultRefresh = 0;
uint8_t s_lastLcdFaultState = 0xFF;

uint8_t lcdFaultState(uint32_t now) {
    if (FaultManager::hasUnacknowledgedErrors()) {
        return ((now / LCD_FAULT_BLINK_MS) % 2U) ? 3 : 2;
    }
    if (FaultManager::hasActiveFaults()) return 1;
    return 0;
}

void drawWarningTriangle(TFT_eSPI& tft,
                         int16_t x,
                         int16_t y,
                         uint16_t fill,
                         uint16_t outline) {
    const int16_t x1 = x + 8;
    const int16_t y1 = y;
    const int16_t x2 = x;
    const int16_t y2 = y + 15;
    const int16_t x3 = x + 16;
    const int16_t y3 = y + 15;

    if (outline != fill) {
        tft.fillTriangle(x1, y1, x2, y2, x3, y3, outline);
        tft.fillTriangle(x1, y1 + 2, x2 + 2, y2 - 1, x3 - 2, y3 - 1, fill);
    } else {
        tft.fillTriangle(x1, y1, x2, y2, x3, y3, fill);
    }

    tft.drawFastVLine(x + 8, y + 5, 5, TFT_WHITE);
    tft.fillCircle(x + 8, y + 12, 1, TFT_WHITE);
}

void drawLcdFaultIndicator(uint8_t state) {
    if (!g_tftPtr) return;

    g_tftPtr->fillRect(LCD_FAULT_X, LCD_FAULT_Y,
                       LCD_FAULT_W, LCD_FAULT_H, Theme::SURFACE);

    if (state == 0 || state == 2) return;

    if (state == 3) {
        drawWarningTriangle(*g_tftPtr, LCD_FAULT_X, LCD_FAULT_Y,
                            TFT_RED, TFT_RED);
    } else {
        drawWarningTriangle(*g_tftPtr, LCD_FAULT_X, LCD_FAULT_Y,
                            TFT_RED, TFT_GREEN);
    }
}
}

void FaultManager::begin() {
    _activeMask = 0;
    _unacknowledged = false;
    _started = true;
    s_lastLcdFaultState = 0xFF;
    s_lastLcdFaultRefresh = 0;
}

void FaultManager::update() {
    if (!_started || !g_tftPtr) return;

    const uint32_t now = millis();
    const uint8_t state = lcdFaultState(now);

    if (state != s_lastLcdFaultState ||
        (state != 0 && now - s_lastLcdFaultRefresh >= LCD_FAULT_REFRESH_MS)) {
        drawLcdFaultIndicator(state);
        s_lastLcdFaultState = state;
        s_lastLcdFaultRefresh = now;
    }
}

void FaultManager::setActive(FaultId id, bool active) {
    const uint32_t bit = 1UL << static_cast<uint8_t>(id);
    const bool wasActive = (_activeMask & bit) != 0;

    if (active) {
        _activeMask |= bit;
        if (!wasActive) _unacknowledged = true;
    } else {
        _activeMask &= ~bit;
    }
}

void FaultManager::notifyError() {
    _unacknowledged = true;
}

void FaultManager::acknowledge() {
    _unacknowledged = false;
}

bool FaultManager::hasActiveFaults() {
    return _activeMask != 0;
}

bool FaultManager::hasUnacknowledgedErrors() {
    return _unacknowledged;
}

bool FaultManager::isAcknowledged() {
    return !_unacknowledged;
}

uint32_t FaultManager::activeMask() {
    return _activeMask;
}

void FaultManager::resolveColor(uint8_t normalRed,
                                uint8_t normalGreen,
                                uint8_t normalBlue,
                                uint8_t& outRed,
                                uint8_t& outGreen,
                                uint8_t& outBlue) {
    outRed = normalRed;
    outGreen = normalGreen;
    outBlue = normalBlue;

    if (!_started) return;

    const uint32_t now = millis();

    if (_unacknowledged) {
        const uint32_t phase = now % ERROR_PATTERN_PERIOD_MS;
        const bool flash1 = phase >= ERROR_FLASH_1_START_MS &&
                            phase < ERROR_FLASH_1_START_MS + ERROR_FLASH_ON_MS;
        const bool flash2 = phase >= ERROR_FLASH_2_START_MS &&
                            phase < ERROR_FLASH_2_START_MS + ERROR_FLASH_ON_MS;
        const bool flash3 = phase >= ERROR_FLASH_3_START_MS &&
                            phase < ERROR_FLASH_3_START_MS + ERROR_FLASH_ON_MS;

        outRed = (flash1 || flash2 || flash3) ? 255 : 0;
        outGreen = 0;
        outBlue = 0;
        return;
    }

    if (hasActiveFaults() &&
        (now % ACK_REMINDER_PERIOD_MS) < ACK_REMINDER_RED_MS) {
        outRed = 255;
        outGreen = 0;
        outBlue = 0;
    }
}
