#include "FaultManager.h"
#include "config.h"

uint32_t FaultManager::_activeMask = 0;
bool FaultManager::_unacknowledged = false;
bool FaultManager::_started = false;
bool FaultManager::_normalRed = false;
bool FaultManager::_normalGreen = false;
bool FaultManager::_normalBlue = false;

namespace {
constexpr uint32_t ERROR_BLINK_HALF_PERIOD_MS = 500;
constexpr uint32_t ACK_REMINDER_PERIOD_MS = 5000;
constexpr uint32_t ACK_REMINDER_RED_MS = 300;

void writeChannel(uint8_t pin, bool on) {
#if RGB_LED_ACTIVE_LOW
    digitalWrite(pin, on ? LOW : HIGH);
#else
    digitalWrite(pin, on ? HIGH : LOW);
#endif
}
}

void FaultManager::begin() {
    pinMode(RGB_LED_RED_PIN, OUTPUT);
    pinMode(RGB_LED_GREEN_PIN, OUTPUT);
    pinMode(RGB_LED_BLUE_PIN, OUTPUT);

    _started = true;
    normalOff();
    applyNormalColor();

    writeRgb(true, false, false);
    delay(180);
    applyNormalColor();
}

void FaultManager::update() {
    if (!_started) return;

    const uint32_t now = millis();

    if (_unacknowledged) {
        const bool redOn =
            ((now / ERROR_BLINK_HALF_PERIOD_MS) % 2U) == 0U;
        writeRgb(redOn, false, false);
        return;
    }

    if (hasActiveFaults()) {
        const uint32_t phase = now % ACK_REMINDER_PERIOD_MS;
        if (phase < ACK_REMINDER_RED_MS) {
            writeRgb(true, false, false);
        } else {
            applyNormalColor();
        }
        return;
    }

    applyNormalColor();
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

void FaultManager::setNormalColor(bool red, bool green, bool blue) {
    _normalRed = red;
    _normalGreen = green;
    _normalBlue = blue;
}

void FaultManager::normalOff() {
    setNormalColor(false, false, false);
}

void FaultManager::writeRgb(bool red, bool green, bool blue) {
    writeChannel(RGB_LED_RED_PIN, red);
    writeChannel(RGB_LED_GREEN_PIN, green);
    writeChannel(RGB_LED_BLUE_PIN, blue);
}

void FaultManager::applyNormalColor() {
    writeRgb(_normalRed, _normalGreen, _normalBlue);
}
