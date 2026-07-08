#pragma once

#include <stdint.h>

#include "domain/BinaryActuatorDriver.h"
#include "domain/ProtocolBuildProfile.h"

namespace AquaLook { namespace Domain {

#if AQUALOOK_V4_ENABLE_I2C

struct Xl9535I2cOps {
    bool (*probe)(void* platformContext, uint8_t address);
    bool (*writeRegister16)(
        void* platformContext,
        uint8_t address,
        uint8_t registerAddress,
        uint16_t value
    );
    bool (*readRegister16)(
        void* platformContext,
        uint8_t address,
        uint8_t registerAddress,
        uint16_t& value
    );
};

struct Xl9535BinaryActuatorContext {
    const Xl9535I2cOps* i2c;
    void* platformContext;
    BinaryActuatorHealth health;
    uint8_t configured;
    uint8_t address;
    uint16_t outputLatch;
    uint16_t directionMask;
    BinaryActuatorState lastObserved;
    uint8_t reserved;

    constexpr Xl9535BinaryActuatorContext()
        : i2c(nullptr), platformContext(nullptr),
          health(BinaryActuatorHealth::UNKNOWN), configured(0U), address(0U),
          outputLatch(0U), directionMask(0xFFFFU),
          lastObserved(BinaryActuatorState::UNKNOWN), reserved(0U) {}
};

namespace Xl9535Registers {
constexpr uint8_t INPUT_PORT = 0x00U;
constexpr uint8_t OUTPUT_PORT = 0x02U;
constexpr uint8_t POLARITY_INVERSION = 0x04U;
constexpr uint8_t CONFIGURATION = 0x06U;
}

const BinaryActuatorDriverOps& xl9535BinaryActuatorDriverOps();

BinaryActuatorDriverBinding makeXl9535BinaryActuatorDriverBinding(
    Xl9535BinaryActuatorContext& context
);

bool hasCompleteXl9535I2cOps(const Xl9535I2cOps& operations);

#endif

}} // namespace AquaLook::Domain
