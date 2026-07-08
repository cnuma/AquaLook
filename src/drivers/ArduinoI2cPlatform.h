#pragma once

#include "domain/Xl9535BinaryActuatorDriver.h"

namespace AquaLook { namespace Drivers {

#if AQUALOOK_V4_ENABLE_I2C

const Domain::Xl9535I2cOps& arduinoI2cPlatformOps();

#endif

}} // namespace AquaLook::Drivers
