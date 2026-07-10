#pragma once

#include <Arduino.h>
#include "EquipmentModel.h"
#include "RelayTopology.h"

class RelaisManager;

namespace AquaLook { namespace Runtime {
class EquipmentOutputRuntimeAdapter;
}}

// Orchestration des équipements AquaLook.
// À ce stade, seules les électrovannes de zones sont exécutables.
// Les dépendances pompe sont résolues et planifiées, mais pas encore exécutées.
class EquipmentManager {
public:
    enum ActionResult : uint8_t {
        ACTION_OK = 0,
        ACTION_NOT_INITIALIZED,
        ACTION_INVALID_ZONE,
        ACTION_ZONE_LINK_NOT_FOUND,
        ACTION_EQUIPMENT_NOT_FOUND,
        ACTION_INVALID_EQUIPMENT,
        ACTION_RELAY_MAPPING_NOT_FOUND,
        ACTION_RELAY_ROLE_MISMATCH,
        ACTION_EXECUTOR_NOT_CONNECTED,
        ACTION_EXECUTION_FAILED
    };

    enum PlanAction : uint8_t {
        PLAN_ACTION_NONE = 0,
        PLAN_ACTION_VALVE_ON,
        PLAN_ACTION_VALVE_OFF,
        PLAN_ACTION_PUMP_ON,
        PLAN_ACTION_PUMP_OFF,
        PLAN_ACTION_WAIT
    };

    struct PlanStep {
        PlanAction action;
        uint8_t equipmentIndex;
        uint32_t delayMs;

        constexpr PlanStep(
            PlanAction requestedAction = PLAN_ACTION_NONE,
            uint8_t requestedEquipmentIndex = EquipmentModel::INVALID_INDEX,
            uint32_t requestedDelayMs = 0U
        ) : action(requestedAction),
            equipmentIndex(requestedEquipmentIndex),
            delayMs(requestedDelayMs) {}
    };

    static constexpr uint8_t MAX_PLAN_STEPS = 4U;

    struct ZoneExecutionPlan {
        ActionResult result;
        uint8_t zone;
        bool requiresPump;
        uint8_t stepCount;
        PlanStep steps[MAX_PLAN_STEPS];

        constexpr ZoneExecutionPlan()
            : result(ACTION_NOT_INITIALIZED),
              zone(0U),
              requiresPump(false),
              stepCount(0U),
              steps{} {}

        constexpr bool valid() const {
            return result == ACTION_OK && stepCount > 0U;
        }
    };

    struct EquipmentResolution {
        ActionResult result;
        uint8_t equipmentIndex;
        uint8_t relayAssignmentIndex;
        RelayTopology::MappingResolution relay;

        EquipmentResolution();
        bool valid() const;
    };

    struct ZoneDependencyResolution {
        ActionResult result;
        uint8_t linkIndex;
        EquipmentResolution valve;
        bool requiresPump;
        EquipmentResolution pump;

        ZoneDependencyResolution();
        bool valid() const;
    };

    using ZoneResolution = EquipmentResolution;

    void begin(
        const EquipmentModel::EquipmentConfigSet* equipmentModel,
        const RelayTopology::RelayTopologyConfig* relayTopology,
        uint8_t nbZones,
        RelaisManager* relayExecutor = nullptr
    );

    void setRelayExecutor(RelaisManager* relayExecutor);
    void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputAdapter);
    bool isInitialized() const;
    bool hasRelayExecutor() const;
    bool hasOutputAdapter() const;
    bool hasExecutor() const;
    ZoneResolution resolveZone(uint8_t zone) const;
    ZoneDependencyResolution resolveZoneDependencies(uint8_t zone) const;
    ZoneExecutionPlan buildZoneStartPlan(uint8_t zone) const;
    ZoneExecutionPlan buildZoneStopPlan(uint8_t zone) const;

    ActionResult startZone(uint8_t zone);
    ActionResult stopZone(uint8_t zone);

private:
    const EquipmentModel::EquipmentConfigSet* _equipmentModel = nullptr;
    const RelayTopology::RelayTopologyConfig* _relayTopology = nullptr;
    RelaisManager* _relayExecutor = nullptr;
    AquaLook::Runtime::EquipmentOutputRuntimeAdapter* _outputAdapter = nullptr;
    uint8_t _nbZones = 0;

    ActionResult resolveEquipment(
        uint8_t equipmentIndex,
        EquipmentResolution& resolution
    ) const;
    ActionResult validateZoneRequest(uint8_t zone, ZoneResolution& resolution) const;
    ActionResult executeZone(uint8_t zone, bool state);
    ZoneExecutionPlan buildZonePlan(uint8_t zone, bool starting) const;
};
