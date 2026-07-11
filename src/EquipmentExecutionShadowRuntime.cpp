#include "EquipmentExecutionShadowRuntime.h"
#include "EventLog.h"

namespace AquaLook { namespace Runtime {

EquipmentExecutionShadowRuntime::ZoneSlot::ZoneSlot()
    : engine(),
      observedState(PassiveExecutionState::IDLE),
      observedStep(0U),
      occupied(false),
      pumpRequested(false) {}

EquipmentExecutionShadowRuntime::EquipmentExecutionShadowRuntime()
    : _slots{},
      _nbZones(0U),
      _sharedPumpUsers(0U),
      _nextActivityId(1U),
      _nextExecutionId(1U),
      _enabled(false) {}

void EquipmentExecutionShadowRuntime::begin(uint8_t nbZones) {
    _nbZones = constrain(nbZones, (uint8_t)0U, (uint8_t)MAX_ZONES);
    _sharedPumpUsers = 0U;
    _nextActivityId = 1U;
    _nextExecutionId = 1U;
    _enabled = _nbZones > 0U;
    for (uint8_t zone = 0U; zone < MAX_ZONES; ++zone) {
        _slots[zone] = ZoneSlot();
    }
    EventLog::log(
        _enabled ? LOG_INFO : LOG_WARN,
        _enabled
            ? "Shadow engine: actif pour %u zone(s), shared_pump=yes passive=yes"
            : "Shadow engine: desactive, aucune zone",
        _nbZones
    );
}

bool EquipmentExecutionShadowRuntime::submit(
    uint8_t zone,
    const EquipmentManager::ZoneExecutionPlan& plan,
    bool starting,
    uint32_t nowMs
) {
    if (!_enabled || zone >= _nbZones || zone >= MAX_ZONES) return false;

    ZoneSlot& slot = _slots[zone];
    if (slot.engine.isActive()) {
        const ExecutionContext& previous = slot.engine.context();
        EventLog::log(
            LOG_WARN,
            "[Activity#%u][Exec#%u] Shadow: zone %u plan remplace passive=yes",
            previous.activityId.value,
            previous.executionId.value,
            zone + 1U
        );
        slot.engine.cancel(nowMs);
        slot.engine.reset();
    }

    bool keepPumpTransition = true;
    uint8_t usersBefore = _sharedPumpUsers;
    uint8_t usersAfter = _sharedPumpUsers;

    if (plan.requiresPump) {
        if (starting) {
            if (!slot.pumpRequested) {
                slot.pumpRequested = true;
                if (_sharedPumpUsers < MAX_ZONES) ++_sharedPumpUsers;
            }
            usersAfter = _sharedPumpUsers;
            keepPumpTransition = usersBefore == 0U;
        } else {
            if (slot.pumpRequested) {
                slot.pumpRequested = false;
                if (_sharedPumpUsers > 0U) --_sharedPumpUsers;
            }
            usersAfter = _sharedPumpUsers;
            keepPumpTransition = usersAfter == 0U;
        }

        EventLog::log(
            LOG_INFO,
            "Shadow pump arbiter: zone %u %s users=%u->%u transition=%s passive=yes",
            zone + 1U,
            starting ? "ACQUIRE" : "RELEASE",
            usersBefore,
            usersAfter,
            keepPumpTransition
                ? (starting ? "PUMP_ON" : "PUMP_OFF")
                : "KEEP_ON"
        );
    }

    const EquipmentManager::ZoneExecutionPlan effectivePlan =
        buildArbitratedPlan(plan, keepPumpTransition);

    const uint16_t activityValue = _nextActivityId;
    const uint16_t executionValue = _nextExecutionId;
    _nextActivityId = nextValidId(_nextActivityId);
    _nextExecutionId = nextValidId(_nextExecutionId);

    const bool loaded = slot.engine.load(
        effectivePlan,
        AquaLook::Domain::WorkflowId((uint16_t)(zone + 1U)),
        AquaLook::Domain::ActivityId(activityValue),
        AquaLook::Domain::ExecutionId(executionValue),
        nowMs
    );

    slot.occupied = loaded;
    slot.observedState = slot.engine.context().state;
    slot.observedStep = slot.engine.context().currentStep;

    EventLog::log(
        loaded ? LOG_INFO : LOG_WARN,
        "[Activity#%u][Exec#%u] Shadow: zone %u %s accepted=%s steps=%u source_steps=%u pump=%s users=%u passive=yes",
        activityValue,
        executionValue,
        zone + 1U,
        starting ? "START" : "STOP",
        loaded ? "yes" : "no",
        effectivePlan.stepCount,
        plan.stepCount,
        effectivePlan.requiresPump ? "yes" : "no",
        _sharedPumpUsers
    );
    return loaded;
}

void EquipmentExecutionShadowRuntime::update(uint32_t nowMs) {
    if (!_enabled) return;
    for (uint8_t zone = 0U; zone < _nbZones; ++zone) {
        ZoneSlot& slot = _slots[zone];
        if (!slot.occupied) continue;
        const bool progressed = slot.engine.tick(nowMs);
        if (progressed ||
            slot.observedState != slot.engine.context().state ||
            slot.observedStep != slot.engine.context().currentStep) {
            logProgress(zone, slot, nowMs);
        }
        if (slot.engine.isTerminal()) slot.occupied = false;
    }
}

bool EquipmentExecutionShadowRuntime::isEnabled() const {
    return _enabled;
}

uint8_t EquipmentExecutionShadowRuntime::zoneCount() const {
    return _nbZones;
}

uint8_t EquipmentExecutionShadowRuntime::sharedPumpUserCount() const {
    return _sharedPumpUsers;
}

uint16_t EquipmentExecutionShadowRuntime::nextValidId(uint16_t current) {
    uint16_t next = (uint16_t)(current + 1U);
    if (next == 0U || next == 0xFFFFU) next = 1U;
    return next;
}

EquipmentManager::ZoneExecutionPlan
EquipmentExecutionShadowRuntime::buildArbitratedPlan(
    const EquipmentManager::ZoneExecutionPlan& source,
    bool keepPumpTransition
) {
    if (!source.valid() || !source.requiresPump || keepPumpTransition) {
        return source;
    }

    EquipmentManager::ZoneExecutionPlan result;
    result.result = source.result;
    result.zone = source.zone;
    result.requiresPump = true;

    for (uint8_t index = 0U; index < source.stepCount; ++index) {
        const EquipmentManager::PlanStep& step = source.steps[index];
        if (step.action == EquipmentManager::PLAN_ACTION_PUMP_ON ||
            step.action == EquipmentManager::PLAN_ACTION_PUMP_OFF ||
            step.action == EquipmentManager::PLAN_ACTION_WAIT) {
            continue;
        }
        if (result.stepCount < EquipmentManager::MAX_PLAN_STEPS) {
            result.steps[result.stepCount++] = step;
        }
    }

    if (result.stepCount == 0U) {
        result.result = EquipmentManager::ACTION_EXECUTION_FAILED;
    }
    return result;
}

void EquipmentExecutionShadowRuntime::logProgress(
    uint8_t zone,
    ZoneSlot& slot,
    uint32_t nowMs
) {
    const ExecutionContext& context = slot.engine.context();

    if (context.currentStep > slot.observedStep &&
        slot.observedStep < context.plan.stepCount) {
        const EquipmentManager::PlanStep& consumed =
            context.plan.steps[slot.observedStep];
        EventLog::log(
            LOG_INFO,
            "[Activity#%u][Exec#%u] Shadow: zone %u step=%u/%u action=%s consumed passive=yes",
            context.activityId.value,
            context.executionId.value,
            zone + 1U,
            slot.observedStep + 1U,
            context.plan.stepCount,
            EquipmentManager::planActionName(consumed.action)
        );
    }

    if (context.state != slot.observedState) {
        if (context.state == PassiveExecutionState::RUNNING) {
            EventLog::log(
                LOG_INFO,
                "[Activity#%u][Exec#%u] Shadow: zone %u state=RUNNING passive=yes",
                context.activityId.value,
                context.executionId.value,
                zone + 1U
            );
        } else if (context.state == PassiveExecutionState::WAITING) {
            const EquipmentManager::PlanStep& waitStep =
                context.plan.steps[context.currentStep];
            EventLog::log(
                LOG_INFO,
                "[Activity#%u][Exec#%u] Shadow: zone %u state=WAITING delay=%lu passive=yes",
                context.activityId.value,
                context.executionId.value,
                zone + 1U,
                (unsigned long)waitStep.delayMs
            );
        } else if (context.state == PassiveExecutionState::SUCCEEDED) {
            EventLog::log(
                LOG_INFO,
                "[Activity#%u][Exec#%u] Shadow: zone %u state=SUCCEEDED duration=%lu users=%u passive=yes",
                context.activityId.value,
                context.executionId.value,
                zone + 1U,
                (unsigned long)(nowMs - context.startedAtMs),
                _sharedPumpUsers
            );
        } else if (context.state == PassiveExecutionState::FAILED) {
            EventLog::log(
                LOG_WARN,
                "[Activity#%u][Exec#%u] Shadow: zone %u state=FAILED error=%u passive=yes",
                context.activityId.value,
                context.executionId.value,
                zone + 1U,
                (unsigned)context.error
            );
        } else if (context.state == PassiveExecutionState::CANCELLED) {
            EventLog::log(
                LOG_WARN,
                "[Activity#%u][Exec#%u] Shadow: zone %u state=CANCELLED passive=yes",
                context.activityId.value,
                context.executionId.value,
                zone + 1U
            );
        }
    }

    slot.observedState = context.state;
    slot.observedStep = context.currentStep;
}

}} // namespace AquaLook::Runtime
