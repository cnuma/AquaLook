#include "EquipmentExecutionEngine.h"

namespace AquaLook { namespace Runtime {

EquipmentExecutionEngine::EquipmentExecutionEngine()
    : _context(), _pump() {}

void EquipmentExecutionEngine::reset() {
    _context = ExecutionContext();
    _pump = PumpContext();
}

bool EquipmentExecutionEngine::load(
    const EquipmentManager::ZoneExecutionPlan& plan,
    AquaLook::Domain::WorkflowId workflowId,
    AquaLook::Domain::ActivityId activityId,
    AquaLook::Domain::ExecutionId executionId,
    uint32_t nowMs
) {
    if (isActive()) {
        _context.error = PassiveExecutionError::BUSY;
        return false;
    }

    reset();

    if (!workflowId.isValid() || !activityId.isValid() || !executionId.isValid()) {
        fail(PassiveExecutionError::INVALID_CONTEXT, nowMs);
        return false;
    }

    if (!plan.valid() || plan.stepCount > EquipmentManager::MAX_PLAN_STEPS) {
        fail(PassiveExecutionError::INVALID_PLAN, nowMs);
        return false;
    }

    _context.workflowId = workflowId;
    _context.activityId = activityId;
    _context.executionId = executionId;
    _context.plan = plan;
    _context.state = PassiveExecutionState::READY;
    _context.createdAtMs = nowMs;
    _context.lastProgressAtMs = nowMs;

    initializePumpContext();
    return true;
}

bool EquipmentExecutionEngine::tick(uint32_t nowMs) {
    if (_context.state == PassiveExecutionState::READY) {
        _context.state = PassiveExecutionState::RUNNING;
        _context.startedAtMs = nowMs;
        _context.lastProgressAtMs = nowMs;
        return true;
    }

    if (_context.state != PassiveExecutionState::RUNNING &&
        _context.state != PassiveExecutionState::WAITING) {
        return false;
    }

    if (_context.currentStep >= _context.plan.stepCount) {
        completeSuccess(nowMs);
        return true;
    }

    return consumeCurrentStep(nowMs);
}

bool EquipmentExecutionEngine::cancel(uint32_t nowMs) {
    if (!isActive()) return false;

    _context.state = PassiveExecutionState::CANCELLED;
    _context.completedAtMs = nowMs;
    _context.lastProgressAtMs = nowMs;
    _context.waitArmed = false;
    return true;
}

bool EquipmentExecutionEngine::isActive() const {
    return _context.state == PassiveExecutionState::READY ||
           _context.state == PassiveExecutionState::RUNNING ||
           _context.state == PassiveExecutionState::WAITING;
}

bool EquipmentExecutionEngine::isTerminal() const {
    return _context.state == PassiveExecutionState::SUCCEEDED ||
           _context.state == PassiveExecutionState::FAILED ||
           _context.state == PassiveExecutionState::CANCELLED;
}

const ExecutionContext& EquipmentExecutionEngine::context() const {
    return _context;
}

const PumpContext& EquipmentExecutionEngine::pumpContext() const {
    return _pump;
}

bool EquipmentExecutionEngine::consumeCurrentStep(uint32_t nowMs) {
    const EquipmentManager::PlanStep& step =
        _context.plan.steps[_context.currentStep];

    if (step.action == EquipmentManager::PLAN_ACTION_NONE) {
        fail(PassiveExecutionError::INVALID_STEP, nowMs);
        return true;
    }

    if (step.action == EquipmentManager::PLAN_ACTION_WAIT) {
        if (!_context.waitArmed) {
            _context.waitArmed = true;
            _context.waitStartedAtMs = nowMs;
            _context.state = PassiveExecutionState::WAITING;
            _context.lastProgressAtMs = nowMs;
            return true;
        }

        if (static_cast<uint32_t>(nowMs - _context.waitStartedAtMs) < step.delayMs) {
            return false;
        }

        _context.waitArmed = false;
        _context.state = PassiveExecutionState::RUNNING;
        ++_context.currentStep;
        _context.lastProgressAtMs = nowMs;

        if (_context.currentStep >= _context.plan.stepCount) {
            completeSuccess(nowMs);
        }
        return true;
    }

    if (step.action == EquipmentManager::PLAN_ACTION_PUMP_ON ||
        step.action == EquipmentManager::PLAN_ACTION_PUMP_OFF) {
        _pump.required = true;
        _pump.equipmentIndex = step.equipmentIndex;
        _pump.plannedOn = step.action == EquipmentManager::PLAN_ACTION_PUMP_ON;
        ++_pump.plannedTransitions;
    }

    // Étape consommée passivement : aucune commande n'est transmise au runtime.
    ++_context.currentStep;
    _context.lastProgressAtMs = nowMs;

    if (_context.currentStep >= _context.plan.stepCount) {
        completeSuccess(nowMs);
    }
    return true;
}

void EquipmentExecutionEngine::completeSuccess(uint32_t nowMs) {
    _context.state = PassiveExecutionState::SUCCEEDED;
    _context.error = PassiveExecutionError::NONE;
    _context.completedAtMs = nowMs;
    _context.lastProgressAtMs = nowMs;
    _context.waitArmed = false;
}

void EquipmentExecutionEngine::fail(
    PassiveExecutionError error,
    uint32_t nowMs
) {
    _context.state = PassiveExecutionState::FAILED;
    _context.error = error;
    _context.completedAtMs = nowMs;
    _context.lastProgressAtMs = nowMs;
    _context.waitArmed = false;
}

void EquipmentExecutionEngine::initializePumpContext() {
    _pump = PumpContext();
    _pump.required = _context.plan.requiresPump;

    for (uint8_t index = 0U; index < _context.plan.stepCount; ++index) {
        const EquipmentManager::PlanStep& step = _context.plan.steps[index];
        if (step.action == EquipmentManager::PLAN_ACTION_PUMP_ON ||
            step.action == EquipmentManager::PLAN_ACTION_PUMP_OFF) {
            _pump.equipmentIndex = step.equipmentIndex;
            break;
        }
    }
}

}} // namespace AquaLook::Runtime
