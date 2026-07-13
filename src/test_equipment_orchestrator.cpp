#include <Arduino.h>

#include "EquipmentManager.h"
#include "EquipmentModel.h"
#include "EquipmentOrchestrator.h"
#include "RelayTopology.h"

using AquaLook::Application::EquipmentOrchestrator;

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

void configureBoard(RelayTopology::RelayTopologyConfig& topology) {
    RelayTopology::clear(topology);
    RelayTopology::RelayBoardConfig& board = topology.boards[0];
    board.enabled = true;
    board.controller = RelayTopology::CONTROLLER_XL9535;
    board.i2cAddress = XL9535_ADDR;
    board.channelCount = 8U;
    board.logic = RelayTopology::LOGIC_DIRECT;
}

void configureAssignment(
    RelayTopology::RelayTopologyConfig& topology,
    uint8_t assignmentIndex,
    uint8_t role,
    uint8_t targetIndex,
    uint8_t channelIndex
) {
    RelayTopology::RelayAssignment& assignment = topology.assignments[assignmentIndex];
    assignment.enabled = true;
    assignment.role = role;
    assignment.targetIndex = targetIndex;
    assignment.boardIndex = 0U;
    assignment.channelIndex = channelIndex;
}

void configureValveOnlyModel(
    EquipmentModel::EquipmentConfigSet& model,
    RelayTopology::RelayTopologyConfig& topology
) {
    EquipmentModel::clear(model);
    configureBoard(topology);
    configureAssignment(topology, 0U, RelayTopology::ROLE_ZONE_VALVE, 0U, 0U);

    EquipmentModel::EquipmentConfig& valve = model.equipments[0];
    valve.enabled = true;
    valve.type = EquipmentModel::EQUIP_ZONE_VALVE;
    valve.targetIndex = 0U;
    valve.relayAssignmentIndex = 0U;

    EquipmentModel::ZoneEquipmentLink& link = model.zoneLinks[0];
    link.enabled = true;
    link.zoneIndex = 0U;
    link.valveEquipmentIndex = 0U;
    link.pumpEquipmentIndex = EquipmentModel::INVALID_INDEX;
}

void configurePumpModel(
    EquipmentModel::EquipmentConfigSet& model,
    RelayTopology::RelayTopologyConfig& topology
) {
    configureValveOnlyModel(model, topology);
    configureAssignment(topology, 1U, RelayTopology::ROLE_PUMP, 0U, 1U);

    EquipmentModel::EquipmentConfig& pump = model.equipments[1];
    pump.enabled = true;
    pump.type = EquipmentModel::EQUIP_PUMP;
    pump.targetIndex = 0U;
    pump.relayAssignmentIndex = 1U;
    pump.startupDelayMs = 500U;
    pump.shutdownDelayMs = 250U;

    model.zoneLinks[0].pumpEquipmentIndex = 1U;
}

void testNotInitialized() {
    Serial.println("\n[TEST] orchestrator not initialized");
    EquipmentOrchestrator orchestrator;

    const EquipmentOrchestrator::Preview preview = orchestrator.previewStartZone(0U);
    check(!orchestrator.isInitialized(), "orchestrator starts uninitialized");
    check(preview.status == EquipmentOrchestrator::PREVIEW_NOT_INITIALIZED,
          "uninitialized preview is rejected");
    check(!preview.ready(), "uninitialized preview is not ready");
}

void testInvalidZone() {
    Serial.println("\n[TEST] invalid zone");
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    configureValveOnlyModel(model, topology);

    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);

    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const EquipmentOrchestrator::Preview preview = orchestrator.previewStartZone(1U);
    check(orchestrator.isInitialized(), "orchestrator initializes with manager");
    check(preview.status == EquipmentOrchestrator::PREVIEW_INVALID_ZONE,
          "zone outside configured range is rejected");
}

void testValveOnlyPlans() {
    Serial.println("\n[TEST] valve-only start and stop previews");
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    configureValveOnlyModel(model, topology);

    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);

    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const EquipmentOrchestrator::Preview start = orchestrator.previewStartZone(0U);
    const EquipmentOrchestrator::Preview stop = orchestrator.previewStopZone(0U);

    check(start.ready(), "valve-only start preview is ready");
    check(start.intent == EquipmentOrchestrator::INTENT_START_ZONE,
          "start intent is preserved");
    check(!start.requiresPump, "valve-only start does not require pump");
    check(start.stepCount == 1U, "valve-only start has one step");

    check(stop.ready(), "valve-only stop preview is ready");
    check(stop.intent == EquipmentOrchestrator::INTENT_STOP_ZONE,
          "stop intent is preserved");
    check(!stop.requiresPump, "valve-only stop does not require pump");
    check(stop.stepCount == 1U, "valve-only stop has one step");
}

void testPumpPlans() {
    Serial.println("\n[TEST] pump dependency previews");
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    configurePumpModel(model, topology);

    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);

    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const EquipmentOrchestrator::Preview start = orchestrator.previewStartZone(0U);
    const EquipmentOrchestrator::Preview stop = orchestrator.previewStopZone(0U);

    check(start.ready(), "pump start preview is ready");
    check(start.requiresPump, "pump dependency is exposed on start");
    check(start.stepCount == 3U, "pump start includes valve, wait and pump");

    check(stop.ready(), "pump stop preview is ready");
    check(stop.requiresPump, "pump dependency is exposed on stop");
    check(stop.stepCount == 3U, "pump stop includes pump, wait and valve");
}

void testRejectedPlanPropagation() {
    Serial.println("\n[TEST] rejected plan propagation");
    EquipmentModel::EquipmentConfigSet model;
    RelayTopology::RelayTopologyConfig topology;
    EquipmentModel::clear(model);
    RelayTopology::clear(topology);

    EquipmentManager manager;
    manager.begin(&model, &topology, 1U);

    EquipmentOrchestrator orchestrator;
    orchestrator.begin(&manager, 1U);

    const EquipmentOrchestrator::Preview preview = orchestrator.previewStartZone(0U);
    check(preview.status == EquipmentOrchestrator::PREVIEW_PLAN_REJECTED,
          "manager rejection is exposed by orchestrator");
    check(preview.planResult == EquipmentManager::ACTION_ZONE_LINK_NOT_FOUND,
          "manager action result is preserved");
    check(!preview.ready(), "rejected preview is not ready");
}

void runAllTests() {
    Serial.println("============================================================");
    Serial.println("AquaLook V4 - RUN7.2 - Passive equipment orchestrator bench");
    Serial.println("No relay, adapter or physical backend is exercised.");
    Serial.println("============================================================");

    testNotInitialized();
    testInvalidZone();
    testValveOnlyPlans();
    testPumpPlans();
    testRejectedPlanPropagation();

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
