#pragma once

#include <stdint.h>

#include "domain/BoardPortModel.h"
#include "domain/EquipmentRuntimeState.h"

namespace AquaLook { namespace Domain {

enum class BinaryActuatorState : uint8_t {
    UNKNOWN = 0,
    INACTIVE = 1,
    ACTIVE = 2
};

enum class BinaryActuatorHealth : uint8_t {
    UNKNOWN = 0,
    HEALTHY = 1,
    DEGRADED = 2,
    UNAVAILABLE = 3,
    FAULTED = 4
};

enum class BinaryActuatorDriverError : uint8_t {
    NONE = 0,
    INVALID_ARGUMENT,
    NOT_CONFIGURED,
    UNSUPPORTED_PORT,
    UNAVAILABLE,
    COMMUNICATION_ERROR,
    READBACK_ERROR,
    SAFE_STATE_UNSUPPORTED,
    INTERNAL_ERROR
};

enum class BinaryActuatorCommandStatus : uint8_t {
    NONE = 0,
    APPLIED,
    ALREADY_APPLIED,
    FAILED
};

struct BinaryActuatorDriverResult {
    BinaryActuatorCommandStatus status;
    BinaryActuatorDriverError error;
    BinaryActuatorState state;
    uint8_t reserved;
    uint32_t detail;

    constexpr BinaryActuatorDriverResult()
        : status(BinaryActuatorCommandStatus::NONE),
          error(BinaryActuatorDriverError::NONE),
          state(BinaryActuatorState::UNKNOWN), reserved(0U), detail(0U) {}

    constexpr bool succeeded() const {
        return status == BinaryActuatorCommandStatus::APPLIED ||
               status == BinaryActuatorCommandStatus::ALREADY_APPLIED;
    }
};

struct BinaryActuatorDriverOps {
    BinaryActuatorDriverResult (*configure)(
        void* context,
        const ControllerDefinition& controller,
        const PortDefinition& port
    );

    BinaryActuatorDriverResult (*write)(
        void* context,
        const PortDefinition& port,
        BinaryActuatorState requested
    );

    BinaryActuatorDriverResult (*read)(
        void* context,
        const PortDefinition& port
    );

    BinaryActuatorDriverResult (*applySafeState)(
        void* context,
        const PortDefinition& port
    );

    BinaryActuatorHealth (*health)(
        const void* context,
        const PortDefinition& port
    );
};

struct BinaryActuatorDriverBinding {
    ControllerTypeId controllerTypeId;
    const BinaryActuatorDriverOps* operations;
    void* context;

    constexpr BinaryActuatorDriverBinding()
        : controllerTypeId(), operations(nullptr), context(nullptr) {}
};

struct BinaryActuatorSession {
    PortId portId;
    BinaryActuatorState lastApplied;
    uint8_t configured;
    uint16_t revision;

    constexpr BinaryActuatorSession()
        : portId(), lastApplied(BinaryActuatorState::UNKNOWN),
          configured(0U), revision(0U) {}
};

bool isBinaryOutputPort(const PortDefinition& port);
bool hasCompleteBinaryActuatorOps(const BinaryActuatorDriverOps& operations);

BinaryActuatorDriverResult configureBinaryActuator(
    const BinaryActuatorDriverBinding& driver,
    const ControllerDefinition& controller,
    const PortDefinition& port,
    BinaryActuatorSession& session
);

BinaryActuatorDriverResult commandBinaryActuator(
    const BinaryActuatorDriverBinding& driver,
    const PortDefinition& port,
    BinaryActuatorState requested,
    BinaryActuatorSession& session
);

BinaryActuatorDriverResult readBinaryActuator(
    const BinaryActuatorDriverBinding& driver,
    const PortDefinition& port,
    BinaryActuatorSession& session
);

BinaryActuatorDriverResult applyBinaryActuatorSafeState(
    const BinaryActuatorDriverBinding& driver,
    const PortDefinition& port,
    BinaryActuatorSession& session
);

OperationError toOperationError(BinaryActuatorDriverError error);

static_assert(sizeof(BinaryActuatorDriverResult) == 8U,
              "BinaryActuatorDriverResult layout changed");
static_assert(sizeof(BinaryActuatorSession) == 6U,
              "BinaryActuatorSession layout changed");

}} // namespace AquaLook::Domain
