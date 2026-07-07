#pragma once

#include <stddef.h>

#include "domain/DependencyModel.h"
#include "domain/EquipmentModel.h"
#include "domain/EquipmentRuntimeState.h"
#include "domain/ExecutionModel.h"
#include "domain/IntentModel.h"

namespace AquaLook { namespace Domain {

struct DomainCapacityPlan {
    size_t equipmentCount;
    size_t runtimeStateCount;
    size_t faultCount;
    size_t intentCount;
    size_t executionCount;
    size_t dependencyCount;
    size_t operationResultCount;
    size_t arenaBytes;
};

struct DomainMemoryBudget {
    size_t equipmentBytes;
    size_t runtimeStateBytes;
    size_t faultBytes;
    size_t intentBytes;
    size_t executionBytes;
    size_t dependencyBytes;
    size_t operationResultBytes;
    size_t dependencyWorkspaceBytes;
    size_t arenaBytes;
    size_t totalBytes;
};

constexpr DomainMemoryBudget calculateDomainMemoryBudget(const DomainCapacityPlan& plan) {
    return DomainMemoryBudget{
        plan.equipmentCount * sizeof(Equipment),
        plan.runtimeStateCount * sizeof(EquipmentRuntimeState),
        plan.faultCount * sizeof(EquipmentFault),
        plan.intentCount * sizeof(EquipmentIntent),
        plan.executionCount * sizeof(EquipmentExecution),
        plan.dependencyCount * sizeof(EquipmentDependency),
        plan.operationResultCount * sizeof(OperationResult),
        plan.equipmentCount * (sizeof(uint16_t) + sizeof(uint8_t)),
        plan.arenaBytes,
        plan.equipmentCount * sizeof(Equipment) +
            plan.runtimeStateCount * sizeof(EquipmentRuntimeState) +
            plan.faultCount * sizeof(EquipmentFault) +
            plan.intentCount * sizeof(EquipmentIntent) +
            plan.executionCount * sizeof(EquipmentExecution) +
            plan.dependencyCount * sizeof(EquipmentDependency) +
            plan.operationResultCount * sizeof(OperationResult) +
            plan.equipmentCount * (sizeof(uint16_t) + sizeof(uint8_t)) +
            plan.arenaBytes
    };
}

constexpr DomainCapacityPlan DOMAIN_PLAN_SMALL = {
    16U, 16U, 16U, 16U, 8U, 24U, 16U, 2048U
};

constexpr DomainCapacityPlan DOMAIN_PLAN_STANDARD = {
    32U, 32U, 32U, 32U, 16U, 64U, 32U, 4096U
};

constexpr DomainCapacityPlan DOMAIN_PLAN_EXTENDED = {
    64U, 64U, 64U, 64U, 32U, 128U, 64U, 8192U
};

constexpr DomainMemoryBudget DOMAIN_BUDGET_SMALL =
    calculateDomainMemoryBudget(DOMAIN_PLAN_SMALL);
constexpr DomainMemoryBudget DOMAIN_BUDGET_STANDARD =
    calculateDomainMemoryBudget(DOMAIN_PLAN_STANDARD);
constexpr DomainMemoryBudget DOMAIN_BUDGET_EXTENDED =
    calculateDomainMemoryBudget(DOMAIN_PLAN_EXTENDED);

static_assert(DOMAIN_BUDGET_SMALL.totalBytes <= 8192U,
              "Small domain plan exceeds 8 KiB");
static_assert(DOMAIN_BUDGET_STANDARD.totalBytes <= 16384U,
              "Standard domain plan exceeds 16 KiB");
static_assert(DOMAIN_BUDGET_EXTENDED.totalBytes <= 32768U,
              "Extended domain plan exceeds 32 KiB");

}} // namespace AquaLook::Domain
