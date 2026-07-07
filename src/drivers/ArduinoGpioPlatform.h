#pragma once

#include "domain/GpioBinaryActuatorDriver.h"

namespace AquaLook { namespace Drivers {

#if AQUALOOK_V4_ENABLE_GPIO

const Domain::GpioPlatformOps& arduinoGpioPlatformOps();

#endif

}} // namespace AquaLook::Drivers
