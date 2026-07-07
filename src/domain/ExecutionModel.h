#pragma once

#include <stdint.h>

#include "domain/DomainIdentifiers.h"
#include "domain/EquipmentRuntimeState.h"
#include "domain/IntentModel.h"

namespace AquaLook { namespace Domain {

enum class ExecutionStatus : uint8_t {
    CREATED = 0,
    RUNNING = 1,
    SUCCEEDED = 2,
    FAILED = 3,
    CANCELLED = 4,
    TIMED_OUT = 5,
    COMPENSATING = 6,
    COMPENSATED = 7
};

enum class ExecutionStep : uint8_t {
    NONE = 0,
    PREPARE = 1,
    AUTHORIZE = 2,
    APPLY = 3,
    OBSERVE = 4,
    FINALIZE = 5,
    COMPENSATE = 6
};

enum ExecutionFlags : uint8_t {
    EXECUTION_FLAG_NONE = 0U,
    EXECUTION_FLAG_CANCELLATION_REQUESTED = 1U << 0,
    EXECUTION_FLAG_COMPENSATION_REQUIRED = 1U << 1,
    EXECUTION_FLAG_OBSERVATION_REQUIRED = 1U << 2,
    EXECUTION_FLAG_RESULT_READY = 1U << 3
};

struct EquipmentExecution {
    EquipmentStateValue requestedState;
    uint32_t createdAtMs;
    uint32_t startedAtMs;
    uint32_t deadlineAtMs;
    uint32_t completedAtMs;
    ExecutionId id;
    IntentId intentId;
    EquipmentId targetId;
    CorrelationId correlationId;
    OperationError error;
    ExecutionStatus status;
    ExecutionStep step;
    uint8_t flags;
    uint8_t revision;

    constexpr EquipmentExecution()
        : requestedState(), createdAtMs(0U), startedAtMs(0U), deadlineAtMs(0U),
          completedAtMs(0U), id(), intentId(), targetId(), correlationId(),
          error(OperationError::NONE), status(ExecutionStatus::CREATED),
          step(ExecutionStep::NONE), flags(EXECUTION_FLAG_NONE), revision(0U) {}
};

enum class ExecutionValidationError : uint8_t {
    NONE = 0,
    INVALID_ID,
    INVALID_INTENT,
    INVALID_TARGET,
    INVALID_STATE,
    INVALID_STATUS,
    INVALID_STEP,
    INVALID_DEADLINE
};

struct ExecutionValidationResult {
    ExecutionValidationError error;

    constexpr ExecutionValidationResult(ExecutionValidationError value = ExecutionValidationError::NONE)
        : error(value) {}

    constexpr bool ok() const { return error == ExecutionValidationError::NONE; }
};

ExecutionValidationResult validateExecution(const EquipmentExecution& execution);
EquipmentExecution createExecutionFromIntent(
    ExecutionId executionId,
    const EquipmentIntent& intent,
    uint32_t nowMs,
    uint32_t timeoutMs
);

bool isTerminal(const EquipmentExecution& execution);
bool hasExecutionTimedOut(const EquipmentExecution& execution, uint32_t nowMs);
bool canTransitionTo(const EquipmentExecution& execution, ExecutionStatus nextStatus);

bool startExecution(EquipmentExecution& execution, uint32_t nowMs);
bool advanceExecutionStep(EquipmentExecution& execution, ExecutionStep nextStep);
bool requestCancellation(EquipmentExecution& execution);
bool markExecutionCancelled(EquipmentExecution& execution, uint32_t nowMs);
bool markExecutionSucceeded(EquipmentExecution& execution, uint32_t nowMs);
bool markExecutionFailed(EquipmentExecution& execution, OperationError error, uint32_t nowMs);
bool markExecutionTimedOut(EquipmentExecution& execution, uint32_t nowMs);
bool beginCompensation(EquipmentExecution& execution);
bool markExecutionCompensated(EquipmentExecution& execution, uint32_t nowMs);

OperationResult makeOperationResult(const EquipmentExecution& execution);

static_assert(sizeof(EquipmentExecution) == 40U, "EquipmentExecution layout changed");

}} // namespace AquaLook::Domain
