#pragma once

#include <stdint.h>

#include "domain/BinaryActuatorDriverRegistry.h"
#include "domain/GpioBinaryActuatorDriver.h"
#include "domain/SimulatedBinaryActuatorDriver.h"
#include "domain/Xl9535BinaryActuatorDriver.h"

namespace AquaLook { namespace Domain {

enum BinaryActuatorDriverBootstrapFlags : uint8_t {
    BOOTSTRAP_DRIVER_NONE = 0U,
    BOOTSTRAP_DRIVER_SIMULATED = 1U << 0,
    BOOTSTRAP_DRIVER_GPIO = 1U << 1,
    BOOTSTRAP_DRIVER_XL9535 = 1U << 2
};

struct BinaryActuatorDriverBootstrapPlan {
    SimulatedBinaryActuatorContext* simulated;
#if AQUALOOK_V4_ENABLE_GPIO
    GpioBinaryActuatorContext* gpio;
#endif
#if AQUALOOK_V4_ENABLE_I2C
    Xl9535BinaryActuatorContext* xl9535;
#endif
    uint8_t enabledDrivers;
    uint8_t reserved[3];

    constexpr BinaryActuatorDriverBootstrapPlan()
        : simulated(nullptr),
#if AQUALOOK_V4_ENABLE_GPIO
          gpio(nullptr),
#endif
#if AQUALOOK_V4_ENABLE_I2C
          xl9535(nullptr),
#endif
          enabledDrivers(BOOTSTRAP_DRIVER_NONE), reserved{0U, 0U, 0U} {}
};

struct BinaryActuatorDriverBootstrapResult {
    DriverRegistryError error;
    uint8_t requestedDrivers;
    uint8_t registeredDrivers;
    ControllerTypeId failedControllerTypeId;

    constexpr BinaryActuatorDriverBootstrapResult(
        DriverRegistryError value = DriverRegistryError::NONE,
        uint8_t requested = 0U,
        uint8_t registered = 0U,
        ControllerTypeId failedType = ControllerTypeId()
    ) : error(value), requestedDrivers(requested),
        registeredDrivers(registered), failedControllerTypeId(failedType) {}

    constexpr bool ok() const { return error == DriverRegistryError::NONE; }
};

constexpr uint8_t compiledBinaryActuatorDriverMask() {
    return BOOTSTRAP_DRIVER_SIMULATED |
#if AQUALOOK_V4_ENABLE_GPIO
           BOOTSTRAP_DRIVER_GPIO |
#endif
#if AQUALOOK_V4_ENABLE_I2C
           BOOTSTRAP_DRIVER_XL9535 |
#endif
           BOOTSTRAP_DRIVER_NONE;
}

constexpr uint8_t defaultBinaryActuatorDriverCapacity() {
    return static_cast<uint8_t>(
        1U +
#if AQUALOOK_V4_ENABLE_GPIO
        1U +
#endif
#if AQUALOOK_V4_ENABLE_I2C
        1U +
#endif
        0U
    );
}

BinaryActuatorDriverBootstrapResult bootstrapBinaryActuatorDrivers(
    BinaryActuatorDriverRegistry& registry,
    const BinaryActuatorDriverBootstrapPlan& plan
);

}} // namespace AquaLook::Domain
