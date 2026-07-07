#include "domain/ExecutionModel.h"

namespace AquaLook { namespace Domain {

static bool isKnownStatus(ExecutionStatus status) {
    return status == ExecutionStatus::CREATED ||
           status == ExecutionStatus::RUNNING ||
           status == ExecutionStatus::SUCCEEDED ||
           status == ExecutionStatus::FAILED ||
           status == ExecutionStatus::CANCELLED ||
           status == ExecutionStatus::TIMED_OUT ||
           status == ExecutionStatus::COMPENSATING ||
           status == ExecutionStatus::COMPENSATED;
}

static bool isKnownStep(ExecutionStep step) {
    return step == ExecutionStep::NONE ||
           step == ExecutionStep::PREPARE ||
           step == ExecutionStep::AUTHORIZE ||
           step == ExecutionStep::APPLY ||
           step == ExecutionStep::OBSERVE ||
           step == ExecutionStep::FINALIZE ||
           step == ExecutionStep::COMPENSATE;
}

static void incrementRevision(EquipmentExecution& execution) {
    execution.revision = static_cast<uint8_t>(execution.revision + 1U);
    if (execution.revision == 0U) execution.revision = 1U;
}

ExecutionValidationResult validateExecution(const EquipmentExecution& execution) {
    if (!execution.id.isValid()) return ExecutionValidationError::INVALID_ID;
    if (!execution.intentId.isValid()) return ExecutionValidationError::INVALID_INTENT;
    if (!execution.targetId.isValid()) return ExecutionValidationError::INVALID_TARGET;
    if (execution.requestedState.kind == StateValueKind::UNKNOWN ||
        execution.requestedState.validity != StateValidity::VALID) {
        return ExecutionValidationError::INVALID_STATE;
    }
    if (!isKnownStatus(execution.status)) return ExecutionValidationError::INVALID_STATUS;
    if (!isKnownStep(execution.step)) return ExecutionValidationError::INVALID_STEP;
    if (execution.deadlineAtMs != 0U &&
        static_cast<int32_t>(execution.deadlineAtMs - execution.createdAtMs) <= 0) {
        return ExecutionValidationError::INVALID_DEADLINE;
    }
    return ExecutionValidationError::NONE;
}

EquipmentExecution createExecutionFromIntent(
    ExecutionId executionId,
    const EquipmentIntent& intent,
    uint32_t nowMs,
    uint32_t timeoutMs
) {
    EquipmentExecution execution;
    execution.id = executionId;
    execution.intentId = intent.id;
    execution.targetId = intent.targetId;
    execution.correlationId = intent.correlationId;
    execution.requestedState = intent.requestedState;
    execution.createdAtMs = nowMs;
    execution.deadlineAtMs = timeoutMs == 0U ? 0U : nowMs + timeoutMs;
    if ((intent.flags & INTENT_FLAG_REQUIRES_OBSERVATION) != 0U) {
        execution.flags |= EXECUTION_FLAG_OBSERVATION_REQUIRED;
    }
    return execution;
}

bool isTerminal(const EquipmentExecution& execution) {
    return execution.status == ExecutionStatus::SUCCEEDED ||
           execution.status == ExecutionStatus::FAILED ||
           execution.status == ExecutionStatus::CANCELLED ||
           execution.status == ExecutionStatus::TIMED_OUT ||
           execution.status == ExecutionStatus::COMPENSATED;
}

bool hasExecutionTimedOut(const EquipmentExecution& execution, uint32_t nowMs) {
    if (execution.deadlineAtMs == 0U || isTerminal(execution)) return false;
    return static_cast<int32_t>(nowMs - execution.deadlineAtMs) >= 0;
}

bool canTransitionTo(const EquipmentExecution& execution, ExecutionStatus nextStatus) {
    switch (execution.status) {
        case ExecutionStatus::CREATED:
            return nextStatus == ExecutionStatus::RUNNING ||
                   nextStatus == ExecutionStatus::CANCELLED;
        case ExecutionStatus::RUNNING:
            return nextStatus == ExecutionStatus::SUCCEEDED ||
                   nextStatus == ExecutionStatus::FAILED ||
                   nextStatus == ExecutionStatus::CANCELLED ||
                   nextStatus == ExecutionStatus::TIMED_OUT ||
                   nextStatus == ExecutionStatus::COMPENSATING;
        case ExecutionStatus::FAILED:
        case ExecutionStatus::TIMED_OUT:
            return nextStatus == ExecutionStatus::COMPENSATING;
        case ExecutionStatus::COMPENSATING:
            return nextStatus == ExecutionStatus::COMPENSATED ||
                   nextStatus == ExecutionStatus::FAILED;
        default:
            return false;
    }
}

bool startExecution(EquipmentExecution& execution, uint32_t nowMs) {
    if (!canTransitionTo(execution, ExecutionStatus::RUNNING)) return false;
    execution.status = ExecutionStatus::RUNNING;
    execution.step = ExecutionStep::PREPARE;
    execution.startedAtMs = nowMs;
    incrementRevision(execution);
    return true;
}

bool advanceExecutionStep(EquipmentExecution& execution, ExecutionStep nextStep) {
    if (execution.status != ExecutionStatus::RUNNING || !isKnownStep(nextStep)) return false;
    const uint8_t current = static_cast<uint8_t>(execution.step);
    const uint8_t next = static_cast<uint8_t>(nextStep);
    if (next <= current || next > static_cast<uint8_t>(ExecutionStep::FINALIZE)) return false;
    execution.step = nextStep;
    incrementRevision(execution);
    return true;
}

bool requestCancellation(EquipmentExecution& execution) {
    if (isTerminal(execution)) return false;
    execution.flags |= EXECUTION_FLAG_CANCELLATION_REQUESTED;
    incrementRevision(execution);
    return true;
}

bool markExecutionCancelled(EquipmentExecution& execution, uint32_t nowMs) {
    if ((execution.flags & EXECUTION_FLAG_CANCELLATION_REQUESTED) == 0U ||
        !canTransitionTo(execution, ExecutionStatus::CANCELLED)) return false;
    execution.status = ExecutionStatus::CANCELLED;
    execution.completedAtMs = nowMs;
    execution.error = OperationError::NONE;
    execution.flags |= EXECUTION_FLAG_RESULT_READY;
    incrementRevision(execution);
    return true;
}

bool markExecutionSucceeded(EquipmentExecution& execution, uint32_t nowMs) {
    if (!canTransitionTo(execution, ExecutionStatus::SUCCEEDED)) return false;
    execution.status = ExecutionStatus::SUCCEEDED;
    execution.step = ExecutionStep::FINALIZE;
    execution.completedAtMs = nowMs;
    execution.error = OperationError::NONE;
    execution.flags |= EXECUTION_FLAG_RESULT_READY;
    incrementRevision(execution);
    return true;
}

bool markExecutionFailed(EquipmentExecution& execution, OperationError error, uint32_t nowMs) {
    if (error == OperationError::NONE ||
        !canTransitionTo(execution, ExecutionStatus::FAILED)) return false;
    execution.status = ExecutionStatus::FAILED;
    execution.completedAtMs = nowMs;
    execution.error = error;
    execution.flags |= EXECUTION_FLAG_RESULT_READY;
    incrementRevision(execution);
    return true;
}

bool markExecutionTimedOut(EquipmentExecution& execution, uint32_t nowMs) {
    if (!canTransitionTo(execution, ExecutionStatus::TIMED_OUT)) return false;
    execution.status = ExecutionStatus::TIMED_OUT;
    execution.completedAtMs = nowMs;
    execution.error = OperationError::TIMEOUT;
    execution.flags |= EXECUTION_FLAG_RESULT_READY;
    incrementRevision(execution);
    return true;
}

bool beginCompensation(EquipmentExecution& execution) {
    if (!canTransitionTo(execution, ExecutionStatus::COMPENSATING)) return false;
    execution.status = ExecutionStatus::COMPENSATING;
    execution.step = ExecutionStep::COMPENSATE;
    execution.flags |= EXECUTION_FLAG_COMPENSATION_REQUIRED;
    execution.flags &= static_cast<uint8_t>(~EXECUTION_FLAG_RESULT_READY);
    incrementRevision(execution);
    return true;
}

bool markExecutionCompensated(EquipmentExecution& execution, uint32_t nowMs) {
    if (!canTransitionTo(execution, ExecutionStatus::COMPENSATED)) return false;
    execution.status = ExecutionStatus::COMPENSATED;
    execution.completedAtMs = nowMs;
    execution.flags &= static_cast<uint8_t>(~EXECUTION_FLAG_COMPENSATION_REQUIRED);
    execution.flags |= EXECUTION_FLAG_RESULT_READY;
    incrementRevision(execution);
    return true;
}

OperationResult makeOperationResult(const EquipmentExecution& execution) {
    OperationResult result;
    result.executionId = execution.id;
    result.equipmentId = execution.targetId;
    result.completedAtMs = execution.completedAtMs;
    result.error = execution.error;

    switch (execution.status) {
        case ExecutionStatus::SUCCEEDED:
            result.status = OperationStatus::APPLIED;
            break;
        case ExecutionStatus::CANCELLED:
            result.status = OperationStatus::CANCELLED;
            break;
        case ExecutionStatus::TIMED_OUT:
            result.status = OperationStatus::TIMED_OUT;
            break;
        case ExecutionStatus::FAILED:
            result.status = OperationStatus::FAILED;
            break;
        case ExecutionStatus::COMPENSATED:
            result.status = OperationStatus::FAILED;
            result.detail = 1U;
            break;
        default:
            result.status = OperationStatus::NONE;
            break;
    }

    switch (execution.step) {
        case ExecutionStep::AUTHORIZE:
            result.stage = OperationStage::AUTHORIZATION;
            break;
        case ExecutionStep::APPLY:
            result.stage = OperationStage::APPLICATION;
            break;
        case ExecutionStep::OBSERVE:
            result.stage = OperationStage::OBSERVATION;
            break;
        case ExecutionStep::COMPENSATE:
            result.stage = OperationStage::COMPENSATION;
            break;
        default:
            result.stage = OperationStage::REQUEST;
            break;
    }

    return result;
}

}} // namespace AquaLook::Domain
