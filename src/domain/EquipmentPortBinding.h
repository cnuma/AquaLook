#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/BoardPortModel.h"
#include "domain/EquipmentModel.h"

namespace AquaLook { namespace Domain {

enum class BindingKind : uint8_t {
    PRIMARY_ACTUATOR = 0,
    SECONDARY_ACTUATOR = 1,
    OBSERVER = 2,
    SAFETY_INPUT = 3
};

enum BindingFlags : uint8_t {
    BINDING_FLAG_NONE = 0U,
    BINDING_FLAG_ENABLED = 1U << 0,
    BINDING_FLAG_INVERTED = 1U << 1,
    BINDING_FLAG_SHARED_PORT = 1U << 2,
    BINDING_FLAG_REQUIRED = 1U << 3
};

struct EquipmentPortBinding {
    PortCapabilityMask requiredPortCapabilities;
    EquipmentId equipmentId;
    PortId portId;
    uint16_t revision;
    BindingKind kind;
    uint8_t flags;
    uint16_t reserved;

    constexpr EquipmentPortBinding()
        : requiredPortCapabilities(PORT_CAP_NONE), equipmentId(), portId(),
          revision(0U), kind(BindingKind::PRIMARY_ACTUATOR),
          flags(BINDING_FLAG_NONE), reserved(0U) {}
};

enum class BindingValidationError : uint8_t {
    NONE = 0,
    INVALID_EQUIPMENT_ID,
    INVALID_PORT_ID,
    ORPHAN_EQUIPMENT,
    ORPHAN_PORT,
    INVALID_KIND,
    EMPTY_CAPABILITY_REQUIREMENT,
    UNSUPPORTED_PORT_CAPABILITY,
    INCOMPATIBLE_EQUIPMENT_CAPABILITY,
    INVALID_PORT_DIRECTION,
    DUPLICATE_BINDING,
    MULTIPLE_PRIMARY_ACTUATORS,
    PORT_COLLISION
};

struct BindingValidationResult {
    BindingValidationError error;
    uint16_t index;
    EquipmentId equipmentId;
    PortId portId;

    constexpr BindingValidationResult(
        BindingValidationError value = BindingValidationError::NONE,
        uint16_t itemIndex = 0U,
        EquipmentId equipment = EquipmentId(),
        PortId port = PortId()
    ) : error(value), index(itemIndex), equipmentId(equipment), portId(port) {}

    constexpr bool ok() const { return error == BindingValidationError::NONE; }
};

bool isKnownBindingKind(BindingKind kind);

BindingValidationResult validateEquipmentPortBindings(
    const Equipment* equipment,
    size_t equipmentCount,
    const PortDefinition* ports,
    size_t portCount,
    const EquipmentPortBinding* bindings,
    size_t bindingCount
);

// Vue neutre des champs de RelayTopology::RelayAssignment.
// Les valeurs de role restent alignées sur l'historique :
// 1=zone valve, 2=pump, 3=aux, 4=greenhouse vent, 5=lighting.
struct LegacyRelayReference {
    uint8_t enabled;
    uint8_t role;
    uint8_t targetIndex;
    uint8_t boardIndex;
    uint8_t channelIndex;
    uint8_t reserved;

    constexpr LegacyRelayReference()
        : enabled(0U), role(0U), targetIndex(0U), boardIndex(0U),
          channelIndex(0U), reserved(0U) {}
};

struct LegacyEquipmentKey {
    EquipmentId equipmentId;
    uint8_t role;
    uint8_t targetIndex;

    constexpr LegacyEquipmentKey()
        : equipmentId(), role(0U), targetIndex(0U) {}
};

struct LegacyPortKey {
    PortId portId;
    uint8_t boardIndex;
    uint8_t channelIndex;

    constexpr LegacyPortKey()
        : portId(), boardIndex(0U), channelIndex(0U) {}
};

enum class LegacyBindingResolutionError : uint8_t {
    NONE = 0,
    DISABLED_ASSIGNMENT,
    UNSUPPORTED_ROLE,
    EQUIPMENT_NOT_FOUND,
    PORT_NOT_FOUND
};

struct LegacyBindingResolutionResult {
    LegacyBindingResolutionError error;
    EquipmentPortBinding binding;

    constexpr LegacyBindingResolutionResult()
        : error(LegacyBindingResolutionError::NONE), binding() {}

    constexpr bool ok() const { return error == LegacyBindingResolutionError::NONE; }
};

LegacyBindingResolutionResult resolveLegacyRelayBinding(
    const LegacyRelayReference& legacy,
    const LegacyEquipmentKey* equipmentKeys,
    size_t equipmentKeyCount,
    const LegacyPortKey* portKeys,
    size_t portKeyCount
);

static_assert(sizeof(EquipmentPortBinding) == 16U,
              "EquipmentPortBinding layout changed");
static_assert(sizeof(LegacyRelayReference) == 6U,
              "LegacyRelayReference layout changed");
static_assert(sizeof(LegacyEquipmentKey) == 4U,
              "LegacyEquipmentKey layout changed");
static_assert(sizeof(LegacyPortKey) == 4U,
              "LegacyPortKey layout changed");

}} // namespace AquaLook::Domain
