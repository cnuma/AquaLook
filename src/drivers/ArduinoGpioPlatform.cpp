#include "drivers/ArduinoGpioPlatform.h"

#if AQUALOOK_V4_ENABLE_GPIO

#include <Arduino.h>
#include <driver/gpio.h>

namespace AquaLook { namespace Drivers {

namespace {

bool setMode(
    void*,
    uint16_t pin,
    Domain::GpioPinMode mode
) {
    switch (mode) {
        case Domain::GpioPinMode::INPUT:
            pinMode(pin, INPUT);
            return true;
        case Domain::GpioPinMode::OUTPUT:
            pinMode(pin, OUTPUT);
            return true;
        case Domain::GpioPinMode::INPUT_PULLUP:
            pinMode(pin, INPUT_PULLUP);
            return true;
        case Domain::GpioPinMode::INPUT_PULLDOWN:
            pinMode(pin, INPUT_PULLDOWN);
            return true;
        default:
            return false;
    }
}

bool writeLevel(
    void*,
    uint16_t pin,
    Domain::GpioLevel level
) {
    digitalWrite(pin, level == Domain::GpioLevel::HIGH ? HIGH : LOW);
    return true;
}

bool readLevel(
    void*,
    uint16_t pin,
    Domain::GpioLevel& level
) {
    level = digitalRead(pin) == HIGH
        ? Domain::GpioLevel::HIGH
        : Domain::GpioLevel::LOW;
    return true;
}

bool isPinValid(const void*, uint16_t pin) {
    return GPIO_IS_VALID_OUTPUT_GPIO(static_cast<gpio_num_t>(pin));
}

const Domain::GpioPlatformOps OPERATIONS = {
    setMode,
    writeLevel,
    readLevel,
    isPinValid
};

} // namespace

const Domain::GpioPlatformOps& arduinoGpioPlatformOps() {
    return OPERATIONS;
}

}} // namespace AquaLook::Drivers

#endif
