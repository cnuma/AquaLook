#include "drivers/ArduinoI2cPlatform.h"

#if AQUALOOK_V4_ENABLE_I2C

#include <Wire.h>

namespace AquaLook { namespace Drivers {

namespace {

TwoWire& busFromContext(void* context) {
    return context ? *static_cast<TwoWire*>(context) : Wire;
}

bool probe(void* context, uint8_t address) {
    TwoWire& bus = busFromContext(context);
    bus.beginTransmission(address);
    return bus.endTransmission() == 0;
}

bool writeRegister16(
    void* context,
    uint8_t address,
    uint8_t registerAddress,
    uint16_t value
) {
    TwoWire& bus = busFromContext(context);
    bus.beginTransmission(address);
    bus.write(registerAddress);
    bus.write(static_cast<uint8_t>(value & 0xFFU));
    bus.write(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    return bus.endTransmission() == 0;
}

bool readRegister16(
    void* context,
    uint8_t address,
    uint8_t registerAddress,
    uint16_t& value
) {
    TwoWire& bus = busFromContext(context);
    bus.beginTransmission(address);
    bus.write(registerAddress);
    if (bus.endTransmission(false) != 0) {
        return false;
    }

    const uint8_t received = bus.requestFrom(
        static_cast<int>(address),
        static_cast<int>(2)
    );
    if (received != 2U) {
        return false;
    }

    const uint8_t low = bus.read();
    const uint8_t high = bus.read();
    value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8U));
    return true;
}

const Domain::Xl9535I2cOps OPERATIONS = {
    probe,
    writeRegister16,
    readRegister16
};

} // namespace

const Domain::Xl9535I2cOps& arduinoI2cPlatformOps() {
    return OPERATIONS;
}

}} // namespace AquaLook::Drivers

#endif
