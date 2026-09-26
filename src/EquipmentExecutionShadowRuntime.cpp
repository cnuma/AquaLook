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
    // [SHDW-ENGINE] : moteur d'execution parallele en cours de validation
    // (ne pilote aucune sortie reelle) -- "inactif" est normal tant que le
    // chantier n'est pas termine. Voir /logs/messages.tsv.
    EventLog::log(
        LOG_INFO,
        _enabled
            ? "[SHDW-ENGINE] actif zones=%u"
            : "[SHDW-ENGINE] inactif, aucune zone",
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

    repairConsistency();
    ZoneSlot& slot = _slots[zone];
    if (slot.engine.isActive()) {
        const ExecutionContext& previous = slot.engine.context();
        // [SHDW-ACTIVITY] : supersession normale (nouvelle commande pour une
        // zone dont la simulation precedente tournait encore). Voir
        // /logs/messages.tsv.
        EventLog::log(
            LOG_INFO,
            "[SHDW-ACTIVITY] A%u/E%u zone=%u plan remplace",
            previous.activityId.value,
            previous.executionId.value,
            zone + 1U
        );
        slot.engine.cancel(nowMs);
        slot.engine.reset();
        slot.occupied = false;
    }

    const bool requestedBefore = slot.pumpRequested;
    const uint8_t usersBefore = _sharedPumpUsers;
    bool keepPumpTransition = false;
    const char* transition = "NONE";

    if (plan.requiresPump) {
        if (starting) {
            if (!slot.pumpRequested) {
                slot.pumpRequested = true;
                ++_sharedPumpUsers;
                keepPumpTransition = usersBefore == 0U;
                transition = keepPumpTransition ? "PUMP_ON" : "KEEP_ON";
            } else {
                transition = "DUPLICATE_START";
            }
        } else {
            if (slot.pumpRequested) {
                slot.pumpRequested = false;
                if (_sharedPumpUsers > 0U) --_sharedPumpUsers;
                keepPumpTransition = _sharedPumpUsers == 0U;
                transition = keepPumpTransition ? "PUMP_OFF" : "KEEP_ON";
            } else {
                transition = "ORPHAN_STOP";
            }
        }

        // [SHDW-PUMP] : transition normale de l'arbitre (meme code que les
        // incoherences ci-dessous -- moteur en validation, ne pilote rien).
        EventLog::log(
            LOG_INFO,
            "[SHDW-PUMP] zone=%u %s users=%u->%u %s",
            zone + 1U,
            starting ? "ACQUIRE" : "RELEASE",
            usersBefore,
            _sharedPumpUsers,
            transition
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

    if (!loaded) {
        slot.pumpRequested = requestedBefore;
        _sharedPumpUsers = usersBefore;
        repairConsistency();
    }

    slot.occupied = loaded;
    slot.observedState = slot.engine.context().state;
    slot.observedStep = slot.engine.context().currentStep;

    // [SHDW-ACTIVITY] : "accepte=non" signale un plan que le simulateur n'a
    // pas reussi a charger -- ne pilote rien de reel, mais merite un coup
    // d'oeil si ca se repete. Voir /logs/messages.tsv.
    EventLog::log(
        loaded ? LOG_INFO : LOG_WARN,
        "[SHDW-ACTIVITY] A%u/E%u zone=%u %s accepte=%s",
        activityValue,
        executionValue,
        zone + 1U,
        starting ? "START" : "STOP",
        loaded ? "oui" : "non"
    );
    return loaded;
}

void EquipmentExecutionShadowRuntime::update(uint32_t nowMs) {
    if (!_enabled) return;
    repairConsistency();
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

void EquipmentExecutionShadowRuntime::emergencyStopAll(uint32_t nowMs) {
    for (uint8_t zone = 0U; zone < MAX_ZONES; ++zone) {
        ZoneSlot& slot = _slots[zone];
        if (slot.engine.isActive()) slot.engine.cancel(nowMs);
        slot.engine.reset();
        slot.observedState = PassiveExecutionState::IDLE;
        slot.observedStep = 0U;
        slot.occupied = false;
        slot.pumpRequested = false;
    }
    _sharedPumpUsers = 0U;
    // [SHDW-PUMP] : incoherence interne a l'arbitre de pompe du moteur en
    // validation (ne pilote rien) -- a signaler si ca se repete. Voir
    // /logs/messages.tsv.
    EventLog::log(
        LOG_WARN,
        "[SHDW-PUMP] EMERGENCY_STOP inattendu"
    );
}

bool EquipmentExecutionShadowRuntime::isEnabled() const { return _enabled; }
bool EquipmentExecutionShadowRuntime::isConsistent() const {
    return _sharedPumpUsers == countPumpRequests() && _sharedPumpUsers <= _nbZones;
}
uint8_t EquipmentExecutionShadowRuntime::zoneCount() const { return _nbZones; }
uint8_t EquipmentExecutionShadowRuntime::sharedPumpUserCount() const {
    return _sharedPumpUsers;
}

uint8_t EquipmentExecutionShadowRuntime::countPumpRequests() const {
    uint8_t count = 0U;
    for (uint8_t zone = 0U; zone < _nbZones; ++zone) {
        if (_slots[zone].pumpRequested) ++count;
    }
    return count;
}

void EquipmentExecutionShadowRuntime::repairConsistency() {
    const uint8_t counted = countPumpRequests();
    if (_sharedPumpUsers == counted && _sharedPumpUsers <= _nbZones) return;
    // [SHDW-PUMP] : meme code que EMERGENCY_STOP ci-dessus, meme moteur
    // (ne pilote rien) -- voir /logs/messages.tsv.
    EventLog::log(
        LOG_ERROR,
        "[SHDW-PUMP] incoherence users=%u counted=%u",
        _sharedPumpUsers,
        counted
    );
    _sharedPumpUsers = counted;
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
        // [SHDW-ACTIVITY] : progression normale de la simulation. Voir
        // /logs/messages.tsv.
        EventLog::log(
            LOG_INFO,
            "[SHDW-ACTIVITY] A%u/E%u zone=%u etape %u/%u %s",
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
            EventLog::log(LOG_INFO,
                "[SHDW-ACTIVITY] A%u/E%u zone=%u en_cours",
                context.activityId.value, context.executionId.value, zone + 1U);
        } else if (context.state == PassiveExecutionState::WAITING) {
            const EquipmentManager::PlanStep& waitStep =
                context.plan.steps[context.currentStep];
            EventLog::log(LOG_INFO,
                "[SHDW-ACTIVITY] A%u/E%u zone=%u attente %lums",
                context.activityId.value, context.executionId.value, zone + 1U,
                (unsigned long)waitStep.delayMs);
        } else if (context.state == PassiveExecutionState::SUCCEEDED) {
            EventLog::log(LOG_INFO,
                "[SHDW-ACTIVITY] A%u/E%u zone=%u succes %lums",
                context.activityId.value, context.executionId.value, zone + 1U,
                (unsigned long)(nowMs - context.startedAtMs));
        } else if (context.state == PassiveExecutionState::FAILED) {
            // [SHDW-ACTIVITY] : la simulation elle-meme a echoue -- ne
            // pilote rien de reel, mais merite un coup d'oeil si ca se
            // repete (bug potentiel du simulateur).
            EventLog::log(LOG_WARN,
                "[SHDW-ACTIVITY] A%u/E%u zone=%u echec err=%u",
                context.activityId.value, context.executionId.value, zone + 1U,
                (unsigned)context.error);
        } else if (context.state == PassiveExecutionState::CANCELLED) {
            EventLog::log(LOG_WARN,
                "[SHDW-ACTIVITY] A%u/E%u zone=%u annule",
                context.activityId.value, context.executionId.value, zone + 1U);
        }
    }

    slot.observedState = context.state;
    slot.observedStep = context.currentStep;
}

}} // namespace AquaLook::Runtime
