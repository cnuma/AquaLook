#pragma once

#include <Arduino.h>
#include "EquipmentManager.h"

namespace AquaLook { namespace Application {

class EquipmentOrchestrator {
public:
    enum Intent : uint8_t {
        INTENT_NONE = 0,
        INTENT_START_ZONE,
        INTENT_STOP_ZONE
    };

    enum PreviewStatus : uint8_t {
        PREVIEW_READY = 0,
        PREVIEW_NOT_INITIALIZED,
        PREVIEW_INVALID_ZONE,
        PREVIEW_PLAN_REJECTED
    };

    struct Preview {
        PreviewStatus status;
        Intent intent;
        uint8_t zone;
        bool requiresPump;
        uint8_t stepCount;
        EquipmentManager::ActionResult planResult;
        EquipmentManager::ZoneExecutionPlan plan;

        Preview()
            : status(PREVIEW_NOT_INITIALIZED),
              intent(INTENT_NONE),
              zone(0U),
              requiresPump(false),
              stepCount(0U),
              planResult(EquipmentManager::ACTION_NOT_INITIALIZED),
              plan() {}

        bool ready() const {
            return status == PREVIEW_READY;
        }
    };

    struct ExecutionResult {
        Preview preview;
        EquipmentManager::ActionResult actionResult;
        bool executed;

        ExecutionResult()
            : preview(),
              actionResult(EquipmentManager::ACTION_NOT_INITIALIZED),
              executed(false) {}

        bool success() const {
            return executed && actionResult == EquipmentManager::ACTION_OK;
        }
    };

    struct ObservationStats {
        uint32_t totalRequests;
        uint32_t startRequests;
        uint32_t stopRequests;
        uint32_t readyPlans;
        uint32_t rejectedPlans;
        uint32_t notInitialized;
        uint32_t invalidZones;
        uint32_t managerRejections;
        uint32_t plansWithPump;
        uint32_t plannedSteps;

        constexpr ObservationStats()
            : totalRequests(0U),
              startRequests(0U),
              stopRequests(0U),
              readyPlans(0U),
              rejectedPlans(0U),
              notInitialized(0U),
              invalidZones(0U),
              managerRejections(0U),
              plansWithPump(0U),
              plannedSteps(0U) {}
    };

    void begin(EquipmentManager* equipmentManager, uint8_t nbZones);
    bool isInitialized() const;
    uint8_t zoneCount() const;

    Preview previewStartZone(uint8_t zone) const;
    Preview previewStopZone(uint8_t zone) const;

    ExecutionResult executeStartZone(uint8_t zone);
    ExecutionResult executeStopZone(uint8_t zone);

    const ObservationStats& stats() const;
    void resetStats();

private:
    EquipmentManager* _equipmentManager = nullptr;
    uint8_t _nbZones = 0U;
    mutable ObservationStats _stats;

    Preview previewZone(Intent intent, uint8_t zone) const;
    ExecutionResult executeZone(Intent intent, uint8_t zone);
    void recordPreview(const Preview& preview) const;
};

}} // namespace AquaLook::Application
