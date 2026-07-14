#include <Arduino.h>
#include "EquipmentManager.h"
#include "EquipmentModel.h"
#include "EquipmentOrchestrator.h"
#include "RelayTopology.h"

using AquaLook::Application::EquipmentOrchestrator;

namespace {
uint16_t passed = 0U;
uint16_t failed = 0U;

void check(bool condition, const char* label) {
    if (condition) {
        ++passed;
        Serial.printf("[PASS] %s\n", label);
    } else {
        ++failed;
        Serial.printf("[FAIL] %s\n", label);
    }
}

void configureBoard(RelayTopology::RelayTopologyConfig& topology) {
    RelayTopology::clear(topology);
    auto& board = topology.boards[0];
    board.enabled = true;
    board.controller = RelayTopology::CONTROLLER_XL9535;
    board.i2cAddress = XL9535_ADDR;
    board.channelCount = 8U;
    board.logic = RelayTopology::LOGIC_DIRECT;
}

void configureAssignment(RelayTopology::RelayTopologyConfig& topology,
                         uint8_t index,
                         uint8_t role,
                         uint8_t target,
                         uint8_t channel) {
    auto& assignment = topology.assignments[index];
    assignment.enabled = true;
    assignment.role = role;
    assignment.targetIndex = target;
    assignment.boardIndex = 0U;
    assignment.channelIndex = channel;
}

void configureValveOnly(EquipmentModel::EquipmentConfigSet& model,
                        RelayTopology::RelayTopologyConfig& topology) {
    EquipmentModel::clear(model);
    configureBoard(topology);
    configureAssignment(topology, 0U, RelayTopology::ROLE_ZONE_VALVE, 0U, 0U);

    auto& valve = model.equipments[0];
    valve.enabled = true;
    valve.type = EquipmentModel::EQUIP_ZONE_VALVE;
    valve.targetIndex = 0U;
    valve.relayAssignmentIndex = 0U;

    auto& link = model.zoneLinks[0];
    link.enabled = true;
    link.zoneIndex = 0U;
    link.valveEquipmentIndex = 0U;
    link.pumpEquipmentIndex = EquipmentModel::INVALID_INDEX;
}

void configureWithPump(EquipmentModel::EquipmentConfigSet& model,
                       RelayTopology::RelayTopologyConfig& topology) {
    configureValveOnly(model, topology);
    configureAssignment(topology, 1U, RelayTopology::ROLE_PUMP, 0U, 1U);

    auto& pump = model.equipments[1];
    pump.enabled = true;
    pump.type = EquipmentModel::EQUIP_PUMP;
    pump.targetIndex = 0U;
    pump.relayAssignmentIndex = 1U;
    pump.startupDelayMs = 500U;
    pump.shutdownDelayMs = 250U;

    model.zoneLinks[0].pumpEquipmentIndex = 1U;
}

void testNotInitialized() {
    EquipmentOrchestrator orchestrator;
    const auto preview = orchestrator.previewStartZone(0U);
    const auto execution = orchestrator.executeStartZone(0U);
    check(!orchestrator.isInitialized(), "starts uninitialized");
    check(preview.status == EquipmentOrchestrator::PREVIEW_NOT_INITIALIZED,
          "uninitialized preview rejected");
    check(!execution.executed,
          "uninitialized execution not attempted");
    check(execution.actionResult == EquipmentManager::ACTION_NOT_INITIALIZED,
          "uninitialized execution result preserved");
}

void testInvalidZone() {
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    configureValveOnly(model, topology);
    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);
    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const auto preview = orchestrator.previewStartZone(1U);
    const auto execution = orchestrator.executeStartZone(1U);
    check(preview.status == EquipmentOrchestrator::PREVIEW_INVALID_ZONE,
          "out-of-range preview rejected");
    check(!execution.executed,
          "out-of-range execution not attempted");
    check(execution.actionResult == EquipmentManager::ACTION_INVALID_ZONE,
          "out-of-range execution result preserved");
}

void testValveOnly() {
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    configureValveOnly(model, topology);
    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);
    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const auto start = orchestrator.previewStartZone(0U);
    const auto stop = orchestrator.previewStopZone(0U);
    check(start.ready() && start.intent == EquipmentOrchestrator::INTENT_START_ZONE,
          "valve-only start ready");
    check(!start.requiresPump && start.stepCount == 1U,
          "valve-only start has one step");
    check(stop.ready() && stop.intent == EquipmentOrchestrator::INTENT_STOP_ZONE,
          "valve-only stop ready");
    check(!stop.requiresPump && stop.stepCount == 1U,
          "valve-only stop has one step");
}

void testPumpDependency() {
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    configureWithPump(model, topology);
    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);
    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const auto start = orchestrator.previewStartZone(0U);
    const auto stop = orchestrator.previewStopZone(0U);
    check(start.ready() && start.requiresPump && start.stepCount == 3U,
          "pump start preview contains three steps");
    check(stop.ready() && stop.requiresPump && stop.stepCount == 3U,
          "pump stop preview contains three steps");
}

void testManagerRejection() {
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    EquipmentModel::clear(model);
    RelayTopology::clear(topology);
    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);
    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const auto preview = orchestrator.previewStartZone(0U);
    const auto execution = orchestrator.executeStartZone(0U);
    check(preview.status == EquipmentOrchestrator::PREVIEW_PLAN_REJECTED,
          "manager rejection propagated");
    check(preview.planResult == EquipmentManager::ACTION_ZONE_LINK_NOT_FOUND,
          "manager result preserved");
    check(!execution.executed,
          "rejected plan is not executed");
    check(execution.actionResult == EquipmentManager::ACTION_ZONE_LINK_NOT_FOUND,
          "rejected execution result preserved");
}

void testExecutionWithoutExecutor() {
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    configureValveOnly(model, topology);
    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);
    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const auto start = orchestrator.executeStartZone(0U);
    const auto stop = orchestrator.executeStopZone(0U);

    check(start.preview.ready(),
          "start execution keeps validated preview");
    check(start.executed,
          "start execution attempted after valid plan");
    check(start.actionResult == EquipmentManager::ACTION_EXECUTOR_NOT_CONNECTED,
          "start reports missing executor");
    check(!start.success(),
          "start without executor does not succeed");

    check(stop.preview.ready(),
          "stop execution keeps validated preview");
    check(stop.executed,
          "stop execution attempted after valid plan");
    check(stop.actionResult == EquipmentManager::ACTION_EXECUTOR_NOT_CONNECTED,
          "stop reports missing executor");
    check(!stop.success(),
          "stop without executor does not succeed");
}

void testObservationStats() {
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    configureWithPump(model, topology);
    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);
    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    orchestrator.previewStartZone(0U);
    orchestrator.previewStopZone(0U);
    orchestrator.previewStartZone(1U);

    const auto& stats = orchestrator.stats();
    check(stats.totalRequests == 3U, "stats count all requests");
    check(stats.startRequests == 2U && stats.stopRequests == 1U,
          "stats separate start and stop requests");
    check(stats.readyPlans == 2U && stats.rejectedPlans == 1U,
          "stats separate ready and rejected plans");
    check(stats.invalidZones == 1U && stats.managerRejections == 0U,
          "stats preserve rejection cause");
    check(stats.plansWithPump == 2U && stats.plannedSteps == 6U,
          "stats aggregate pump plans and steps");

    orchestrator.resetStats();
    check(orchestrator.stats().totalRequests == 0U,
          "stats reset clears counters");
}

void runAll() {
    Serial.println("============================================================");
    Serial.println("AquaLook V4 - RUN7.6 - Orchestrator observation counters bench");
    Serial.println("No relay or physical backend is exercised.");
    Serial.println("============================================================");
    testNotInitialized();
    testInvalidZone();
    testValveOnly();
    testPumpDependency();
    testManagerRejection();
    testExecutionWithoutExecutor();
    testObservationStats();
    Serial.println("============================================================");
    Serial.printf("RESULT: passed=%u failed=%u status=%s\n",
                  passed, failed, failed == 0U ? "SUCCESS" : "FAILED");
    Serial.println("============================================================");
}
}

void setup() {
    Serial.begin(115200);
    delay(1000U);
    runAll();
}

void loop() {
    delay(1000U);
}
