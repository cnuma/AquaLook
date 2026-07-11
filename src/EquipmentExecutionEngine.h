#pragma once

#include <stdint.h>

#include "EquipmentManager.h"
#include "domain/DomainIdentifiers.h"

namespace AquaLook { namespace Runtime {

enum class PassiveExecutionState : uint8_t {
    IDLE = 0,
    READY,
    RUNNING,
    WAITING,
    SUCCEEDED,
    FAILED,
    CANCELLED
};

enum class PassiveExecutionError : uint8_t {
    NONE = 0,
    INVALID_CONTEXT,
    INVALID_PLAN,
    BUSY,
    INVALID_STEP
};

struct PumpContext {
    bool required;
    uint8_t equipmentIndex;
    bool plannedOn;
    uint8_t plannedTransitions;

    constexpr PumpContext()
        : required(false),
          equipmentIndex(EquipmentModel::INVALID_INDEX),
          plannedOn(false),
          plannedTransitions(0U) {}
};

struct ExecutionContext {
    AquaLook::Domain::WorkflowId workflowId;
    AquaLook::Domain::ActivityId activityId;
    AquaLook::Domain::ExecutionId executionId;
    EquipmentManager::ZoneExecutionPlan plan;
    PassiveExecutionState state;
    PassiveExecutionError error;
    uint8_t currentStep;
    uint32_t createdAtMs;
    uint32_t startedAtMs;
    uint32_t lastProgressAtMs;
    uint32_t completedAtMs;
    uint32_t waitStartedAtMs;
    bool waitArmed;

    constexpr ExecutionContext()
        : workflowId(),
          activityId(),
          executionId(),
          plan(),
          state(PassiveExecutionState::IDLE),
          error(PassiveExecutionError::NONE),
          currentStep(0U),
          createdAtMs(0U),
          startedAtMs(0U),
          lastProgressAtMs(0U),
          completedAtMs(0U),
          waitStartedAtMs(0U),
          waitArmed(false) {}
};

// Moteur passif du Run 6.12.
// Il consomme un plan en mémoire et fait progresser une machine d'états
// non bloquante. Il ne possède aucun adaptateur, driver ou accès matériel.
class EquipmentExecutionEngine {
public:
    EquipmentExecutionEngine();

    void reset();

    bool load(
        const EquipmentManager::ZoneExecutionPlan& plan,
        AquaLook::Domain::WorkflowId workflowId,
        AquaLook::Domain::ActivityId activityId,
        AquaLook::Domain::ExecutionId executionId,
        uint32_t nowMs
    );

    bool tick(uint32_t nowMs);
    bool cancel(uint32_t nowMs);

    bool isActive() const;
    bool isTerminal() const;

    const ExecutionContext& context() const;
    const PumpContext& pumpContext() const;

private:
    ExecutionContext _context;
    PumpContext _pump;

    bool consumeCurrentStep(uint32_t nowMs);
    void completeSuccess(uint32_t nowMs);
    void fail(PassiveExecutionError error, uint32_t nowMs);
    void initializePumpContext();
};

}} // namespace AquaLook::Runtime
