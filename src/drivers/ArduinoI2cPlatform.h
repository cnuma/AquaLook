#pragma once

#include "domain/I2cExpanderBinaryActuatorDriver.h"

namespace AquaLook { namespace Drivers {

#if AQUALOOK_V4_ENABLE_I2C

const Domain::I2cExpanderOps& arduinoI2cPlatformOps();

#endif

}} // namespace AquaLook::Drivers
