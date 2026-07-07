#include "domain/DependencyModel.h"

namespace AquaLook { namespace Domain {

static int findEquipmentIndex(
    EquipmentId id,
    const EquipmentId* equipmentIds,
    size_t equipmentCount
) {
    if (!equipmentIds) return -1;
    for (size_t i = 0; i < equipmentCount; ++i) {
        if (equipmentIds[i] == id) return static_cast<int>(i);
    }
    return -1;
}

static bool sameDependency(
    const EquipmentDependency& lhs,
    const EquipmentDependency& rhs
) {
    return lhs.sourceId == rhs.sourceId &&
           lhs.targetId == rhs.targetId &&
           lhs.type == rhs.type &&
           lhs.requiredState.kind == rhs.requiredState.kind &&
           lhs.requiredState.validity == rhs.requiredState.validity &&
           lhs.requiredState.value == rhs.requiredState.value;
}

bool isKnownDependencyType(DependencyType type) {
    return type == DependencyType::REQUIRES_STATE ||
           type == DependencyType::START_AFTER ||
           type == DependencyType::STOP_BEFORE ||
           type == DependencyType::MUTUALLY_EXCLUSIVE ||
           type == DependencyType::INHIBITS;
}

bool participatesInCycleCheck(DependencyType type) {
    return type == DependencyType::REQUIRES_STATE ||
           type == DependencyType::START_AFTER ||
           type == DependencyType::STOP_BEFORE;
}

DependencyValidationResult validateDependency(
    const EquipmentDependency& dependency,
    const EquipmentId* equipmentIds,
    size_t equipmentCount
) {
    if (!dependency.sourceId.isValid()) {
        return DependencyValidationResult(
            DependencyValidationError::INVALID_SOURCE,
            0U,
            dependency.sourceId
        );
    }
    if (!dependency.targetId.isValid()) {
        return DependencyValidationResult(
            DependencyValidationError::INVALID_TARGET,
            0U,
            dependency.targetId
        );
    }
    if (dependency.sourceId == dependency.targetId) {
        return DependencyValidationResult(
            DependencyValidationError::SELF_REFERENCE,
            0U,
            dependency.sourceId
        );
    }
    if (!isKnownDependencyType(dependency.type)) {
        return DependencyValidationError::UNKNOWN_TYPE;
    }
    if (dependency.type == DependencyType::REQUIRES_STATE &&
        (dependency.requiredState.kind == StateValueKind::UNKNOWN ||
         dependency.requiredState.validity != StateValidity::VALID)) {
        return DependencyValidationError::INVALID_REQUIRED_STATE;
    }
    if (findEquipmentIndex(dependency.sourceId, equipmentIds, equipmentCount) < 0) {
        return DependencyValidationResult(
            DependencyValidationError::ORPHAN_SOURCE,
            0U,
            dependency.sourceId
        );
    }
    if (findEquipmentIndex(dependency.targetId, equipmentIds, equipmentCount) < 0) {
        return DependencyValidationResult(
            DependencyValidationError::ORPHAN_TARGET,
            0U,
            dependency.targetId
        );
    }
    return DependencyValidationError::NONE;
}

DependencyValidationResult validateDependencyGraph(
    const EquipmentId* equipmentIds,
    size_t equipmentCount,
    const EquipmentDependency* dependencies,
    size_t dependencyCount,
    DependencyGraphWorkspace workspace
) {
    if ((equipmentCount != 0U && !equipmentIds) ||
        (dependencyCount != 0U && !dependencies) ||
        !workspace.indegree || !workspace.processed ||
        workspace.capacity < equipmentCount) {
        return DependencyValidationError::WORKSPACE_TOO_SMALL;
    }

    for (size_t i = 0; i < equipmentCount; ++i) {
        if (!equipmentIds[i].isValid()) {
            return DependencyValidationResult(
                DependencyValidationError::INVALID_SOURCE,
                0U,
                equipmentIds[i]
            );
        }
        workspace.indegree[i] = 0U;
        workspace.processed[i] = 0U;
    }

    for (size_t i = 0; i < dependencyCount; ++i) {
        DependencyValidationResult result = validateDependency(
            dependencies[i], equipmentIds, equipmentCount
        );
        if (!result.ok()) {
            result.dependencyIndex = static_cast<uint16_t>(i);
            return result;
        }

        for (size_t j = 0; j < i; ++j) {
            if (sameDependency(dependencies[i], dependencies[j])) {
                return DependencyValidationResult(
                    DependencyValidationError::DUPLICATE_RELATION,
                    static_cast<uint16_t>(i),
                    dependencies[i].sourceId
                );
            }
        }

        if (participatesInCycleCheck(dependencies[i].type)) {
            const int targetIndex = findEquipmentIndex(
                dependencies[i].targetId, equipmentIds, equipmentCount
            );
            if (targetIndex >= 0 && workspace.indegree[targetIndex] != 0xFFFFU) {
                workspace.indegree[targetIndex]++;
            }
        }
    }

    size_t processedCount = 0U;
    bool progressed = true;

    while (progressed) {
        progressed = false;

        for (size_t node = 0; node < equipmentCount; ++node) {
            if (workspace.processed[node] != 0U || workspace.indegree[node] != 0U) {
                continue;
            }

            workspace.processed[node] = 1U;
            processedCount++;
            progressed = true;

            for (size_t edge = 0; edge < dependencyCount; ++edge) {
                const EquipmentDependency& dependency = dependencies[edge];
                if (!participatesInCycleCheck(dependency.type) ||
                    dependency.sourceId != equipmentIds[node]) {
                    continue;
                }

                const int targetIndex = findEquipmentIndex(
                    dependency.targetId, equipmentIds, equipmentCount
                );
                if (targetIndex >= 0 && workspace.indegree[targetIndex] > 0U) {
                    workspace.indegree[targetIndex]--;
                }
            }
        }
    }

    if (processedCount != equipmentCount) {
        for (size_t i = 0; i < equipmentCount; ++i) {
            if (workspace.processed[i] == 0U && workspace.indegree[i] > 0U) {
                return DependencyValidationResult(
                    DependencyValidationError::CYCLE_DETECTED,
                    0U,
                    equipmentIds[i]
                );
            }
        }
        return DependencyValidationError::CYCLE_DETECTED;
    }

    return DependencyValidationError::NONE;
}

}} // namespace AquaLook::Domain
