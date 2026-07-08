#pragma once

#include <stdint.h>

#include "domain/DomainIdentifiers.h"

namespace AquaLook { namespace Domain {

enum class StateValueKind : uint8_t {
    UNKNOWN = 0,
    BINARY = 1,
    PERCENT = 2,
    POSITION = 3,
    ENUMERATED = 4,
    RAW_SIGNED = 5
};

enum class StateValidity : uint8_t {
    UNKNOWN = 0,
    VALID = 1,
    STALE = 2,
    INVALID = 3,
    NOT_SUPPORTED = 4
};

struct EquipmentStateValue {
    int32_t value;
    StateValueKind kind;
    StateValidity validity;
    uint16_t reserved;

    constexpr EquipmentStateValue()
        : value(0), kind(StateValueKind::UNKNOWN),
          validity(StateValidity::UNKNOWN), reserved(0U) {}

    constexpr EquipmentStateValue(
        int32_t raw,
        StateValueKind valueKind,
        StateValidity valueValidity = StateValidity::VALID
    ) : value(raw), kind(valueKind), validity(valueValidity), reserved(0U) {}
};

enum class EquipmentHealth : uint8_t {
    UNKNOWN = 0,
    HEALTHY = 1,
    DEGRADED = 2,
    FAULTED = 3,
    UNAVAILABLE = 4
};

enum RuntimeStateFlags : uint8_t {
    RUNTIME_FLAG_NONE = 0U,
    RUNTIME_FLAG_AUTHORIZED = 1U << 0,
    RUNTIME_FLAG_COMMAND_PENDING = 1U << 1,
    RUNTIME_FLAG_OBSERVATION_EXPECTED = 1U << 2,
    RUNTIME_FLAG_INTERLOCKED = 1U << 3
};

struct EquipmentRuntimeState {
    EquipmentId equipmentId;
    uint16_t revision;

    EquipmentStateValue requested;
    EquipmentStateValue authorized;
    EquipmentStateValue applied;
    EquipmentStateValue observed;

    uint32_t requestedAtMs;
    uint32_t authorizedAtMs;
    uint32_t appliedAtMs;
    uint32_t observedAtMs;

    EquipmentHealth health;
    uint8_t flags;
    uint16_t activeFaultCount;

    constexpr EquipmentRuntimeState()
        : equipmentId(), revision(0U), requested(), authorized(), applied(), observed(),
          requestedAtMs(0U), authorizedAtMs(0U), appliedAtMs(0U), observedAtMs(0U),
          health(EquipmentHealth::UNKNOWN), flags(RUNTIME_FLAG_NONE),
          activeFaultCount(0U) {}
};

enum class FaultDomain : uint8_t {
    NONE = 0,
    CONFIGURATION = 1,
    AUTHORIZATION = 2,
    ACTUATOR = 3,
    SENSOR = 4,
    COMMUNICATION = 5,
    SAFETY = 6,
    INTERNAL = 7
};

enum class FaultSeverity : uint8_t {
    INFO = 0,
    WARNING = 1,
    ERROR = 2,
    CRITICAL = 3
};

enum FaultFlags : uint8_t {
    FAULT_FLAG_NONE = 0U,
    FAULT_FLAG_ACTIVE = 1U << 0,
    FAULT_FLAG_ACKNOWLEDGED = 1U << 1,
    FAULT_FLAG_BLOCKING = 1U << 2,
    FAULT_FLAG_LATCHED = 1U << 3
};

struct EquipmentFault {
    EquipmentId equipmentId;
    uint16_t code;
    FaultDomain domain;
    FaultSeverity severity;
    uint8_t flags;
    uint8_t occurrenceCount;
    uint32_t firstSeenAtMs;
    uint32_t lastSeenAtMs;

    constexpr EquipmentFault()
        : equipmentId(), code(0U), domain(FaultDomain::NONE),
          severity(FaultSeverity::INFO), flags(FAULT_FLAG_NONE),
          occurrenceCount(0U), firstSeenAtMs(0U), lastSeenAtMs(0U) {}
};

enum class OperationStatus : uint8_t {
    NONE = 0,
    ACCEPTED = 1,
    REJECTED = 2,
    APPLIED = 3,
    FAILED = 4,
    CANCELLED = 5,
    TIMED_OUT = 6
};

enum class OperationStage : uint8_t {
    NONE = 0,
    REQUEST = 1,
    AUTHORIZATION = 2,
    APPLICATION = 3,
    OBSERVATION = 4,
    COMPENSATION = 5
};

enum class OperationError : uint16_t {
    NONE = 0,
    INVALID_TARGET = 1,
    INVALID_STATE = 2,
    TARGET_DISABLED = 3,
    INTERLOCKED = 4,
    DEPENDENCY_UNAVAILABLE = 5,
    CAPABILITY_NOT_SUPPORTED = 6,
    ACTUATOR_UNAVAILABLE = 7,
    COMMUNICATION_ERROR = 8,
    OBSERVATION_MISMATCH = 9,
    TIMEOUT = 10,
    INTERNAL_ERROR = 11
};

struct OperationResult {
    ExecutionId executionId;
    EquipmentId equipmentId;
    OperationStatus status;
    OperationStage stage;
    OperationError error;
    uint32_t completedAtMs;
    uint32_t detail;

    constexpr OperationResult()
        : executionId(), equipmentId(), status(OperationStatus::NONE),
          stage(OperationStage::NONE), error(OperationError::NONE),
          completedAtMs(0U), detail(0U) {}

    constexpr bool succeeded() const {
        return status == OperationStatus::ACCEPTED ||
               status == OperationStatus::APPLIED;
    }
};

bool sameStateValue(const EquipmentStateValue& lhs, const EquipmentStateValue& rhs);
bool isConverged(const EquipmentRuntimeState& state);
bool hasBlockingFault(const EquipmentFault& fault);
bool isFaultActive(const EquipmentFault& fault);

void recordRequestedState(
    EquipmentRuntimeState& runtime,
    const EquipmentStateValue& value,
    uint32_t nowMs
);
void recordAuthorizedState(
    EquipmentRuntimeState& runtime,
    const EquipmentStateValue& value,
    bool authorized,
    uint32_t nowMs
);
void recordAppliedState(
    EquipmentRuntimeState& runtime,
    const EquipmentStateValue& value,
    uint32_t nowMs
);
void recordObservedState(
    EquipmentRuntimeState& runtime,
    const EquipmentStateValue& value,
    uint32_t nowMs
);
void activateFault(EquipmentFault& fault, uint32_t nowMs);
void clearFault(EquipmentFault& fault, uint32_t nowMs);

static_assert(sizeof(EquipmentStateValue) == 8U, "EquipmentStateValue layout changed");
static_assert(sizeof(EquipmentFault) == 16U, "EquipmentFault layout changed");
static_assert(sizeof(OperationResult) == 16U, "OperationResult layout changed");
static_assert(sizeof(EquipmentRuntimeState) <= 56U, "EquipmentRuntimeState must remain compact");

}} // namespace AquaLook::Domain
