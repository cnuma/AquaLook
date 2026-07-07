#pragma once

#include <stdint.h>

#include "domain/BinaryActuatorDriver.h"

namespace AquaLook { namespace Domain {

enum SimulatedBinaryActuatorFault : uint8_t {
    SIMULATED_FAULT_NONE = 0U,
    SIMULATED_FAULT_CONFIGURE = 1U << 0,
    SIMULATED_FAULT_WRITE = 1U << 1,
    SIMULATED_FAULT_READ = 1U << 2,
    SIMULATED_FAULT_SAFE_STATE = 1U << 3,
    SIMULATED_FAULT_UNAVAILABLE = 1U << 4,
    SIMULATED_FAULT_READBACK_MISMATCH = 1U << 5
};

struct SimulatedBinaryActuatorContext {
    BinaryActuatorState state;
    BinaryActuatorHealth health;
    uint32_t configureCount;
    uint32_t writeCount;
    uint32_t readCount;
    uint32_t safeStateCount;
    uint8_t faultMask;
    uint8_t configured;
    uint16_t reserved;

    constexpr SimulatedBinaryActuatorContext()
        : state(BinaryActuatorState::INACTIVE),
          health(BinaryActuatorHealth::HEALTHY),
          configureCount(0U), writeCount(0U), readCount(0U),
          safeStateCount(0U), faultMask(SIMULATED_FAULT_NONE),
          configured(0U), reserved(0U) {}
};

const BinaryActuatorDriverOps& simulatedBinaryActuatorDriverOps();

void resetSimulatedBinaryActuatorContext(
    SimulatedBinaryActuatorContext& context,
    BinaryActuatorState initialState = BinaryActuatorState::INACTIVE
);

void setSimulatedBinaryActuatorFaults(
    SimulatedBinaryActuatorContext& context,
    uint8_t faultMask
);

static_assert(sizeof(SimulatedBinaryActuatorContext) == 24U,
              "SimulatedBinaryActuatorContext layout changed");

}} // namespace AquaLook::Domain
