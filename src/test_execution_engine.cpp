#include <Arduino.h>

#include "EquipmentExecutionEngine.h"

using AquaLook::Domain::ActivityId;
using AquaLook::Domain::ExecutionId;
using AquaLook::Domain::WorkflowId;
using AquaLook::Runtime::EquipmentExecutionEngine;
using AquaLook::Runtime::PassiveExecutionError;
using AquaLook::Runtime::PassiveExecutionState;

namespace {

uint16_t testsPassed = 0U;
uint16_t testsFailed = 0U;

void check(bool condition, const char* label) {
    if (condition) {
        ++testsPassed;
        Serial.printf("[PASS] %s\n", label);
    } else {
        ++testsFailed;
        Serial.printf("[FAIL] %s\n", label);
    }
}

EquipmentManager::ZoneExecutionPlan makeValveOnlyPlan() {
    EquipmentManager::ZoneExecutionPlan plan;
    plan.result = EquipmentManager::ACTION_OK;
    plan.zone = 0U;
    plan.requiresPump = false;
    plan.stepCount = 1U;
    plan.steps[0] = EquipmentManager::PlanStep(
        EquipmentManager::PLAN_ACTION_VALVE_ON,
        0U
    );
    return plan;
}

EquipmentManager::ZoneExecutionPlan makePumpStartPlan(uint32_t delayMs) {
    EquipmentManager::ZoneExecutionPlan plan;
    plan.result = EquipmentManager::ACTION_OK;
    plan.zone = 0U;
    plan.requiresPump = true;
    plan.stepCount = 3U;
    plan.steps[0] = EquipmentManager::PlanStep(
        EquipmentManager::PLAN_ACTION_VALVE_ON,
        0U
    );
    plan.steps[1] = EquipmentManager::PlanStep(
        EquipmentManager::PLAN_ACTION_WAIT,
        EquipmentModel::INVALID_INDEX,
        delayMs
    );
    plan.steps[2] = EquipmentManager::PlanStep(
        EquipmentManager::PLAN_ACTION_PUMP_ON,
        4U
    );
    return plan;
}

EquipmentManager::ZoneExecutionPlan makeWaitOnlyPlan(uint32_t delayMs) {
    EquipmentManager::ZoneExecutionPlan plan;
    plan.result = EquipmentManager::ACTION_OK;
    plan.zone = 0U;
    plan.requiresPump = false;
    plan.stepCount = 1U;
    plan.steps[0] = EquipmentManager::PlanStep(
        EquipmentManager::PLAN_ACTION_WAIT,
        EquipmentModel::INVALID_INDEX,
        delayMs
    );
    return plan;
}

void testValveOnlyPlan() {
    Serial.println("\n[TEST] valve-only passive plan");

    EquipmentExecutionEngine engine;
    const bool loaded = engine.load(
        makeValveOnlyPlan(),
        WorkflowId(1U),
        ActivityId(1U),
        ExecutionId(1U),
        100U
    );

    check(loaded, "valve plan loads");
    check(engine.context().state == PassiveExecutionState::READY, "state READY after load");
    check(engine.tick(101U), "first tick starts execution");
    check(engine.context().state == PassiveExecutionState::RUNNING, "state RUNNING");
    check(engine.tick(102U), "valve step consumed passively");
    check(engine.context().state == PassiveExecutionState::SUCCEEDED, "valve plan succeeds");
    check(engine.context().currentStep == 1U, "one step consumed");
    check(engine.pumpContext().plannedTransitions == 0U, "no pump transition recorded");
}

void testPumpPlanWithWait() {
    Serial.println("\n[TEST] pump plan with non-blocking wait");

    EquipmentExecutionEngine engine;
    check(
        engine.load(
            makePumpStartPlan(500U),
            WorkflowId(2U),
            ActivityId(2U),
            ExecutionId(2U),
            1000U
        ),
        "pump plan loads"
    );

    engine.tick(1000U);
    engine.tick(1001U);
    check(engine.context().currentStep == 1U, "valve step consumed before wait");

    engine.tick(1002U);
    check(engine.context().state == PassiveExecutionState::WAITING, "state WAITING");
    check(!engine.tick(1501U), "wait remains pending before deadline");
    check(engine.context().currentStep == 1U, "wait step not consumed early");
    check(engine.tick(1502U), "wait completes at deadline");
    check(engine.context().currentStep == 2U, "wait step consumed at deadline");
    check(engine.tick(1503U), "pump step consumed passively");
    check(engine.context().state == PassiveExecutionState::SUCCEEDED, "pump plan succeeds");
    check(engine.pumpContext().required, "pump dependency recorded");
    check(engine.pumpContext().equipmentIndex == 4U, "pump equipment recorded");
    check(engine.pumpContext().plannedOn, "pump ON recorded as planned state");
    check(engine.pumpContext().plannedTransitions == 1U, "one pump transition recorded");
}

void testMillisWraparound() {
    Serial.println("\n[TEST] millis wraparound");

    EquipmentExecutionEngine engine;
    check(
        engine.load(
            makeWaitOnlyPlan(32U),
            WorkflowId(3U),
            ActivityId(3U),
            ExecutionId(3U),
            0xFFFFFFE0UL
        ),
        "wraparound plan loads"
    );

    engine.tick(0xFFFFFFE1UL);
    engine.tick(0xFFFFFFF0UL);
    check(engine.context().state == PassiveExecutionState::WAITING, "wraparound wait armed");
    check(!engine.tick(0x0000000FUL), "31 ms across wrap remains pending");
    check(engine.tick(0x00000010UL), "32 ms across wrap completes");
    check(engine.context().state == PassiveExecutionState::SUCCEEDED, "wraparound plan succeeds");
}

void testCancellation() {
    Serial.println("\n[TEST] cancellation while waiting");

    EquipmentExecutionEngine engine;
    engine.load(
        makeWaitOnlyPlan(1000U),
        WorkflowId(4U),
        ActivityId(4U),
        ExecutionId(4U),
        2000U
    );
    engine.tick(2001U);
    engine.tick(2002U);

    check(engine.context().state == PassiveExecutionState::WAITING, "cancellation test reaches WAITING");
    check(engine.cancel(2100U), "active execution can be cancelled");
    check(engine.context().state == PassiveExecutionState::CANCELLED, "state CANCELLED");
    check(engine.context().completedAtMs == 2100U, "cancellation completion time recorded");
    check(!engine.tick(2200U), "cancelled execution no longer advances");
}

void testInvalidContext() {
    Serial.println("\n[TEST] invalid execution context");

    EquipmentExecutionEngine engine;
    const bool loaded = engine.load(
        makeValveOnlyPlan(),
        WorkflowId(),
        ActivityId(5U),
        ExecutionId(5U),
        3000U
    );

    check(!loaded, "invalid workflow id rejected");
    check(engine.context().state == PassiveExecutionState::FAILED, "invalid context sets FAILED");
    check(engine.context().error == PassiveExecutionError::INVALID_CONTEXT, "invalid context error recorded");
    check(engine.isTerminal(), "failed invalid context is terminal");
}

void runAllTests() {
    Serial.println("============================================================");
    Serial.println("AquaLook V4 - Run 6.13 - Passive execution engine bench");
    Serial.println("No relay, adapter or physical backend is used by this test.");
    Serial.println("============================================================");

    testValveOnlyPlan();
    testPumpPlanWithWait();
    testMillisWraparound();
    testCancellation();
    testInvalidContext();

    Serial.println("\n============================================================");
    Serial.printf("RESULT: passed=%u failed=%u status=%s\n",
                  testsPassed,
                  testsFailed,
                  testsFailed == 0U ? "SUCCESS" : "FAILED");
    Serial.println("============================================================");
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(1000U);
    runAllTests();
}

void loop() {
    delay(1000U);
}
