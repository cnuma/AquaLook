#pragma once

#include <stddef.h>

#include "domain/BoardPortModel.h"
#include "domain/EquipmentPortBinding.h"
#include "domain/HardwareInventoryModel.h"

namespace AquaLook { namespace Domain {

struct HardwareCapacityPlan {
    size_t busCount;
    size_t controllerCount;
    size_t boardCount;
    size_t portCount;
    size_t bindingCount;
    size_t legacyEquipmentKeyCount;
    size_t legacyPortKeyCount;
};

struct HardwareMemoryBudget {
    size_t busBytes;
    size_t controllerBytes;
    size_t boardBytes;
    size_t portBytes;
    size_t bindingBytes;
    size_t legacyEquipmentKeyBytes;
    size_t legacyPortKeyBytes;
    size_t totalBytes;
    size_t activeAndCandidateBytes;
};

constexpr HardwareMemoryBudget calculateHardwareMemoryBudget(
    const HardwareCapacityPlan& plan
) {
    return HardwareMemoryBudget{
        plan.busCount * sizeof(BusDefinition),
        plan.controllerCount * sizeof(ControllerDefinition),
        plan.boardCount * sizeof(BoardDefinition),
        plan.portCount * sizeof(PortDefinition),
        plan.bindingCount * sizeof(EquipmentPortBinding),
        plan.legacyEquipmentKeyCount * sizeof(LegacyEquipmentKey),
        plan.legacyPortKeyCount * sizeof(LegacyPortKey),
        plan.busCount * sizeof(BusDefinition) +
            plan.controllerCount * sizeof(ControllerDefinition) +
            plan.boardCount * sizeof(BoardDefinition) +
            plan.portCount * sizeof(PortDefinition) +
            plan.bindingCount * sizeof(EquipmentPortBinding) +
            plan.legacyEquipmentKeyCount * sizeof(LegacyEquipmentKey) +
            plan.legacyPortKeyCount * sizeof(LegacyPortKey),
        2U * (
            plan.busCount * sizeof(BusDefinition) +
            plan.controllerCount * sizeof(ControllerDefinition) +
            plan.boardCount * sizeof(BoardDefinition) +
            plan.portCount * sizeof(PortDefinition) +
            plan.bindingCount * sizeof(EquipmentPortBinding) +
            plan.legacyEquipmentKeyCount * sizeof(LegacyEquipmentKey) +
            plan.legacyPortKeyCount * sizeof(LegacyPortKey)
        )
    };
}

constexpr HardwareCapacityPlan HARDWARE_PLAN_SMALL = {
    2U, 4U, 4U, 16U, 16U, 16U, 16U
};

constexpr HardwareCapacityPlan HARDWARE_PLAN_STANDARD = {
    4U, 8U, 8U, 64U, 64U, 32U, 64U
};

constexpr HardwareCapacityPlan HARDWARE_PLAN_EXTENDED = {
    8U, 16U, 16U, 128U, 128U, 64U, 128U
};

constexpr HardwareMemoryBudget HARDWARE_BUDGET_SMALL =
    calculateHardwareMemoryBudget(HARDWARE_PLAN_SMALL);
constexpr HardwareMemoryBudget HARDWARE_BUDGET_STANDARD =
    calculateHardwareMemoryBudget(HARDWARE_PLAN_STANDARD);
constexpr HardwareMemoryBudget HARDWARE_BUDGET_EXTENDED =
    calculateHardwareMemoryBudget(HARDWARE_PLAN_EXTENDED);

static_assert(HARDWARE_BUDGET_SMALL.totalBytes <= 2048U,
              "Small hardware inventory exceeds 2 KiB");
static_assert(HARDWARE_BUDGET_STANDARD.totalBytes <= 4096U,
              "Standard hardware inventory exceeds 4 KiB");
static_assert(HARDWARE_BUDGET_EXTENDED.totalBytes <= 8192U,
              "Extended hardware inventory exceeds 8 KiB");

}} // namespace AquaLook::Domain
