#include "FaultManager.h"

#include <TFT_eSPI.h>
#include "Theme.h"

// Pointeur initialise par DisplayManager pendant le splash puis conserve
// pendant toute l'execution. Le voyant d'erreur reste ainsi independant
// de l'ecran courant et des rafraichissements partiels.
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

constexpr uint32_t LCD_FAULT_REFRESH_MS = 250;
constexpr int16_t LCD_FAULT_X = 306;
constexpr int16_t LCD_FAULT_Y = 1;
constexpr int16_t LCD_FAULT_W = 13;
constexpr int16_t LCD_FAULT_H = 13;

uint32_t s_lastLcdFaultRefresh = 0;
uint8_t s_lastLcdFaultState = 0xFF;

uint8_t lcdFaultState() {
    if (FaultManager::hasUnacknowledgedErrors()) return 2;
    if (FaultManager::hasActiveFaults()) return 1;
    return 0;
}

void drawLcdFaultIndicator(uint8_t state) {
    if (!g_tftPtr) return;

    g_tftPtr->fillRect(LCD_FAULT_X, LCD_FAULT_Y,
                       LCD_FAULT_W, LCD_FAULT_H, Theme::SURFACE);

    if (state == 0) return;

    const int16_t cx = LCD_FAULT_X + LCD_FAULT_W / 2;
    const int16_t cy = LCD_FAULT_Y + LCD_FAULT_H / 2;

    if (state == 2) {
        // Erreur non acquittee : voyant plein et point d'exclamation.
        g_tftPtr->fillCircle(cx, cy, 6, TFT_RED);
        g_tftPtr->drawFastVLine(cx, cy - 3, 5, TFT_WHITE);
        g_tftPtr->fillCircle(cx, cy + 4, 1, TFT_WHITE);
    } else {
        // Defaut toujours actif mais acquitte : rappel discret permanent.
        g_tftPtr->drawCircle(cx, cy, 5, TFT_RED);
        g_tftPtr->drawCircle(cx, cy, 4, TFT_RED);
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
    const uint8_t state = lcdFaultState();

    // Redessiner regulierement tant qu'un defaut existe : les sprites du
    // bandeau peuvent recouvrir ponctuellement le voyant lors d'un refresh.
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
