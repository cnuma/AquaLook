#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/DomainIdentifiers.h"
#include "domain/EquipmentRuntimeState.h"

namespace AquaLook { namespace Domain {

enum class DependencyType : uint8_t {
    REQUIRES_STATE = 0,
    START_AFTER = 1,
    STOP_BEFORE = 2,
    MUTUALLY_EXCLUSIVE = 3,
    INHIBITS = 4
};

enum DependencyFlags : uint8_t {
    DEPENDENCY_FLAG_NONE = 0U,
    DEPENDENCY_FLAG_HARD = 1U << 0,
    DEPENDENCY_FLAG_BLOCKING = 1U << 1,
    DEPENDENCY_FLAG_PROPAGATE_STOP = 1U << 2
};

struct EquipmentDependency {
    EquipmentId sourceId;
    EquipmentId targetId;
    EquipmentStateValue requiredState;
    uint16_t delayMs;
    DependencyType type;
    uint8_t flags;

    constexpr EquipmentDependency()
        : sourceId(), targetId(), requiredState(), delayMs(0U),
          type(DependencyType::REQUIRES_STATE), flags(DEPENDENCY_FLAG_NONE) {}
};

enum class DependencyValidationError : uint8_t {
    NONE = 0,
    INVALID_SOURCE,
    INVALID_TARGET,
    SELF_REFERENCE,
    UNKNOWN_TYPE,
    INVALID_REQUIRED_STATE,
    ORPHAN_SOURCE,
    ORPHAN_TARGET,
    DUPLICATE_RELATION,
    CYCLE_DETECTED,
    WORKSPACE_TOO_SMALL
};

struct DependencyValidationResult {
    DependencyValidationError error;
    uint16_t dependencyIndex;
    EquipmentId equipmentId;

    constexpr DependencyValidationResult(
        DependencyValidationError value = DependencyValidationError::NONE,
        uint16_t index = 0U,
        EquipmentId equipment = EquipmentId()
    ) : error(value), dependencyIndex(index), equipmentId(equipment) {}

    constexpr bool ok() const { return error == DependencyValidationError::NONE; }
};

struct DependencyGraphWorkspace {
    uint16_t* indegree;
    uint8_t* processed;
    size_t capacity;

    constexpr DependencyGraphWorkspace()
        : indegree(nullptr), processed(nullptr), capacity(0U) {}

    constexpr DependencyGraphWorkspace(uint16_t* degrees, uint8_t* marks, size_t count)
        : indegree(degrees), processed(marks), capacity(count) {}
};

bool isKnownDependencyType(DependencyType type);
bool participatesInCycleCheck(DependencyType type);

DependencyValidationResult validateDependency(
    const EquipmentDependency& dependency,
    const EquipmentId* equipmentIds,
    size_t equipmentCount
);

DependencyValidationResult validateDependencyGraph(
    const EquipmentId* equipmentIds,
    size_t equipmentCount,
    const EquipmentDependency* dependencies,
    size_t dependencyCount,
    DependencyGraphWorkspace workspace
);

static_assert(sizeof(EquipmentDependency) == 16U, "EquipmentDependency layout changed");

}} // namespace AquaLook::Domain
