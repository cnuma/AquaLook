#include "domain/EquipmentPortBinding.h"

namespace AquaLook { namespace Domain {

static int findEquipmentIndex(
    EquipmentId id,
    const Equipment* equipment,
    size_t equipmentCount
) {
    if (!equipment) return -1;
    for (size_t i = 0U; i < equipmentCount; ++i) {
        if (equipment[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

static int findPortIndex(PortId id, const PortDefinition* ports, size_t portCount) {
    if (!ports) return -1;
    for (size_t i = 0U; i < portCount; ++i) {
        if (ports[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

static bool hasOutputCapability(PortCapabilityMask capabilities) {
    return (capabilities &
        (PORT_CAP_DIGITAL_OUTPUT | PORT_CAP_PWM_OUTPUT | PORT_CAP_RELAY_OUTPUT)) != 0U;
}

static bool hasInputCapability(PortCapabilityMask capabilities) {
    return (capabilities &
        (PORT_CAP_DIGITAL_INPUT | PORT_CAP_COUNTER_INPUT | PORT_CAP_ANALOG_INPUT |
         PORT_CAP_INTERRUPT_INPUT | PORT_CAP_SENSOR_INPUT)) != 0U;
}

static bool equipmentSupportsBinding(
    const Equipment& equipment,
    const EquipmentPortBinding& binding
) {
    switch (binding.kind) {
        case BindingKind::PRIMARY_ACTUATOR:
        case BindingKind::SECONDARY_ACTUATOR:
            return (equipment.capabilities &
                (CAP_BINARY_COMMAND | CAP_PROPORTIONAL_COMMAND | CAP_BIDIRECTIONAL |
                 CAP_PULSE_COMMAND)) != 0U;
        case BindingKind::OBSERVER:
            return (equipment.capabilities &
                (CAP_POSITION_FEEDBACK | CAP_STATE_FEEDBACK | CAP_FAULT_FEEDBACK)) != 0U;
        case BindingKind::SAFETY_INPUT:
            return (equipment.capabilities &
                (CAP_STATE_FEEDBACK | CAP_FAULT_FEEDBACK | CAP_SAFE_STATE)) != 0U;
        default:
            return false;
    }
}

bool isKnownBindingKind(BindingKind kind) {
    return kind == BindingKind::PRIMARY_ACTUATOR ||
           kind == BindingKind::SECONDARY_ACTUATOR ||
           kind == BindingKind::OBSERVER ||
           kind == BindingKind::SAFETY_INPUT;
}

BindingValidationResult validateEquipmentPortBindings(
    const Equipment* equipment,
    size_t equipmentCount,
    const PortDefinition* ports,
    size_t portCount,
    const EquipmentPortBinding* bindings,
    size_t bindingCount
) {
    if ((equipmentCount != 0U && !equipment) ||
        (portCount != 0U && !ports) ||
        (bindingCount != 0U && !bindings)) {
        return BindingValidationError::ORPHAN_EQUIPMENT;
    }

    for (size_t i = 0U; i < bindingCount; ++i) {
        const EquipmentPortBinding& binding = bindings[i];

        if (!binding.equipmentId.isValid()) {
            return BindingValidationResult(
                BindingValidationError::INVALID_EQUIPMENT_ID,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }
        if (!binding.portId.isValid()) {
            return BindingValidationResult(
                BindingValidationError::INVALID_PORT_ID,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }

        const int equipmentIndex = findEquipmentIndex(
            binding.equipmentId, equipment, equipmentCount
        );
        if (equipmentIndex < 0) {
            return BindingValidationResult(
                BindingValidationError::ORPHAN_EQUIPMENT,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }

        const int portIndex = findPortIndex(binding.portId, ports, portCount);
        if (portIndex < 0) {
            return BindingValidationResult(
                BindingValidationError::ORPHAN_PORT,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }

        if (!isKnownBindingKind(binding.kind)) {
            return BindingValidationResult(
                BindingValidationError::INVALID_KIND,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }
        if (binding.requiredPortCapabilities == PORT_CAP_NONE) {
            return BindingValidationResult(
                BindingValidationError::EMPTY_CAPABILITY_REQUIREMENT,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }

        const Equipment& targetEquipment = equipment[equipmentIndex];
        const PortDefinition& targetPort = ports[portIndex];

        if ((binding.requiredPortCapabilities & ~targetPort.capabilities) != 0U) {
            return BindingValidationResult(
                BindingValidationError::UNSUPPORTED_PORT_CAPABILITY,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }
        if (!equipmentSupportsBinding(targetEquipment, binding)) {
            return BindingValidationResult(
                BindingValidationError::INCOMPATIBLE_EQUIPMENT_CAPABILITY,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }

        const bool actuator = binding.kind == BindingKind::PRIMARY_ACTUATOR ||
                              binding.kind == BindingKind::SECONDARY_ACTUATOR;
        const bool observer = binding.kind == BindingKind::OBSERVER ||
                              binding.kind == BindingKind::SAFETY_INPUT;

        if ((actuator &&
             (targetPort.direction == PortDirection::INPUT ||
              !hasOutputCapability(binding.requiredPortCapabilities))) ||
            (observer &&
             (targetPort.direction == PortDirection::OUTPUT ||
              !hasInputCapability(binding.requiredPortCapabilities)))) {
            return BindingValidationResult(
                BindingValidationError::INVALID_PORT_DIRECTION,
                static_cast<uint16_t>(i), binding.equipmentId, binding.portId
            );
        }

        for (size_t j = 0U; j < i; ++j) {
            const EquipmentPortBinding& previous = bindings[j];
            if (previous.equipmentId == binding.equipmentId &&
                previous.portId == binding.portId &&
                previous.kind == binding.kind) {
                return BindingValidationResult(
                    BindingValidationError::DUPLICATE_BINDING,
                    static_cast<uint16_t>(i), binding.equipmentId, binding.portId
                );
            }
            if (binding.kind == BindingKind::PRIMARY_ACTUATOR &&
                previous.kind == BindingKind::PRIMARY_ACTUATOR &&
                previous.equipmentId == binding.equipmentId) {
                return BindingValidationResult(
                    BindingValidationError::MULTIPLE_PRIMARY_ACTUATORS,
                    static_cast<uint16_t>(i), binding.equipmentId, binding.portId
                );
            }
            if (previous.portId == binding.portId &&
                (((previous.flags | binding.flags) & BINDING_FLAG_SHARED_PORT) == 0U)) {
                return BindingValidationResult(
                    BindingValidationError::PORT_COLLISION,
                    static_cast<uint16_t>(i), binding.equipmentId, binding.portId
                );
            }
        }
    }

    return BindingValidationError::NONE;
}

LegacyBindingResolutionResult resolveLegacyRelayBinding(
    const LegacyRelayReference& legacy,
    const LegacyEquipmentKey* equipmentKeys,
    size_t equipmentKeyCount,
    const LegacyPortKey* portKeys,
    size_t portKeyCount
) {
    LegacyBindingResolutionResult result;

    if (legacy.enabled == 0U) {
        result.error = LegacyBindingResolutionError::DISABLED_ASSIGNMENT;
        return result;
    }
    if (legacy.role < 1U || legacy.role > 5U) {
        result.error = LegacyBindingResolutionError::UNSUPPORTED_ROLE;
        return result;
    }

    EquipmentId equipmentId;
    for (size_t i = 0U; i < equipmentKeyCount; ++i) {
        if (equipmentKeys && equipmentKeys[i].role == legacy.role &&
            equipmentKeys[i].targetIndex == legacy.targetIndex) {
            equipmentId = equipmentKeys[i].equipmentId;
            break;
        }
    }
    if (!equipmentId.isValid()) {
        result.error = LegacyBindingResolutionError::EQUIPMENT_NOT_FOUND;
        return result;
    }

    PortId portId;
    for (size_t i = 0U; i < portKeyCount; ++i) {
        if (portKeys && portKeys[i].boardIndex == legacy.boardIndex &&
            portKeys[i].channelIndex == legacy.channelIndex) {
            portId = portKeys[i].portId;
            break;
        }
    }
    if (!portId.isValid()) {
        result.error = LegacyBindingResolutionError::PORT_NOT_FOUND;
        return result;
    }

    result.binding.equipmentId = equipmentId;
    result.binding.portId = portId;
    result.binding.requiredPortCapabilities = PORT_CAP_RELAY_OUTPUT;
    result.binding.kind = BindingKind::PRIMARY_ACTUATOR;
    result.binding.flags = static_cast<uint8_t>(
        BINDING_FLAG_ENABLED | BINDING_FLAG_REQUIRED
    );
    result.binding.revision = 1U;
    return result;
}

}} // namespace AquaLook::Domain
