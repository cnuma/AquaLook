#include "FaultManager.h"

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
}

void FaultManager::begin() {
    _activeMask = 0;
    _unacknowledged = false;
    _started = true;
}

void FaultManager::update() {}

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
