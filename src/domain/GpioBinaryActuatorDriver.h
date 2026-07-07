#pragma once

#include <stdint.h>

#include "domain/BinaryActuatorDriver.h"
#include "domain/ProtocolBuildProfile.h"

namespace AquaLook { namespace Domain {

#if AQUALOOK_V4_ENABLE_GPIO

enum class GpioPinMode : uint8_t {
    MODE_INPUT = 0,
    MODE_OUTPUT = 1,
    MODE_INPUT_PULLUP = 2,
    MODE_INPUT_PULLDOWN = 3
};

enum class GpioLevel : uint8_t {
    LEVEL_LOW = 0,
    LEVEL_HIGH = 1
};

struct GpioPlatformOps {
    bool (*setMode)(void* platformContext, uint16_t pin, GpioPinMode mode);
    bool (*writeLevel)(void* platformContext, uint16_t pin, GpioLevel level);
    bool (*readLevel)(void* platformContext, uint16_t pin, GpioLevel& level);
    bool (*isPinValid)(const void* platformContext, uint16_t pin);
};

struct GpioBinaryActuatorContext {
    const GpioPlatformOps* platform;
    void* platformContext;
    BinaryActuatorHealth health;
    uint8_t configured;
    uint16_t configuredPin;
    BinaryActuatorState lastObserved;
    uint8_t reserved;

    constexpr GpioBinaryActuatorContext()
        : platform(nullptr), platformContext(nullptr),
          health(BinaryActuatorHealth::UNKNOWN), configured(0U),
          configuredPin(0U), lastObserved(BinaryActuatorState::UNKNOWN),
          reserved(0U) {}
};

const BinaryActuatorDriverOps& gpioBinaryActuatorDriverOps();

BinaryActuatorDriverBinding makeGpioBinaryActuatorDriverBinding(
    GpioBinaryActuatorContext& context
);

bool hasCompleteGpioPlatformOps(const GpioPlatformOps& operations);

#endif

}} // namespace AquaLook::Domain
