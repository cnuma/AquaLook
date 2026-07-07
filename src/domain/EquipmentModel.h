#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/BoundedArena.h"
#include "domain/DomainIdentifiers.h"

namespace AquaLook { namespace Domain {

using CapabilityMask = uint32_t;

enum EquipmentCapability : CapabilityMask {
    CAP_NONE                 = 0U,
    CAP_BINARY_COMMAND       = 1UL << 0,
    CAP_PROPORTIONAL_COMMAND = 1UL << 1,
    CAP_BIDIRECTIONAL        = 1UL << 2,
    CAP_TIMED_OPERATION      = 1UL << 3,
    CAP_PULSE_COMMAND        = 1UL << 4,
    CAP_POSITION_FEEDBACK    = 1UL << 5,
    CAP_STATE_FEEDBACK       = 1UL << 6,
    CAP_FAULT_FEEDBACK       = 1UL << 7,
    CAP_SAFE_STATE           = 1UL << 8,
    CAP_SHARED_RESOURCE      = 1UL << 9
};

enum class EquipmentMode : uint8_t {
    DISABLED = 0,
    AUTOMATIC = 1,
    MANUAL = 2,
    MAINTENANCE = 3
};

enum class SafeState : uint8_t {
    UNSPECIFIED = 0,
    INACTIVE = 1,
    ACTIVE = 2,
    HOLD_LAST = 3
};

enum EquipmentFlags : uint8_t {
    EQUIPMENT_FLAG_NONE = 0U,
    EQUIPMENT_FLAG_ENABLED = 1U << 0,
    EQUIPMENT_FLAG_CRITICAL = 1U << 1
};

struct ParameterBlockRef {
    uint32_t offset;
    uint16_t size;
    uint16_t schemaVersion;

    constexpr ParameterBlockRef()
        : offset(BoundedArena::INVALID_OFFSET), size(0U), schemaVersion(0U) {}

    constexpr bool isPresent() const {
        return offset != BoundedArena::INVALID_OFFSET && size != 0U;
    }
};

struct TextRef {
    uint32_t offset;
    uint16_t size;

    constexpr TextRef() : offset(BoundedArena::INVALID_OFFSET), size(0U) {}
    constexpr bool isPresent() const {
        return offset != BoundedArena::INVALID_OFFSET && size != 0U;
    }
};

struct Equipment {
    EquipmentId id;
    EquipmentTypeId typeId;
    CapabilityMask capabilities;
    ParameterBlockRef parameters;
    TextRef name;
    EquipmentMode mode;
    SafeState safeState;
    uint8_t flags;
    uint8_t reserved;

    constexpr Equipment()
        : id(), typeId(), capabilities(CAP_NONE), parameters(), name(),
          mode(EquipmentMode::DISABLED), safeState(SafeState::UNSPECIFIED),
          flags(EQUIPMENT_FLAG_NONE), reserved(0U) {}
};

struct EquipmentTypeDescriptor {
    EquipmentTypeId id;
    CapabilityMask requiredCapabilities;
    CapabilityMask supportedCapabilities;
    uint16_t parameterSchemaVersion;
    uint16_t minimumParameterBytes;
    uint16_t maximumParameterBytes;
    const char* technicalName;
};

enum class EquipmentValidationError : uint8_t {
    NONE = 0,
    INVALID_ID,
    INVALID_TYPE_ID,
    UNKNOWN_CAPABILITY,
    MISSING_REQUIRED_CAPABILITY,
    UNSUPPORTED_CAPABILITY,
    INVALID_PARAMETER_REFERENCE,
    INVALID_PARAMETER_SIZE,
    INVALID_PARAMETER_SCHEMA,
    INVALID_NAME_REFERENCE,
    INVALID_MODE,
    INVALID_SAFE_STATE
};

struct EquipmentValidationResult {
    EquipmentValidationError error;

    constexpr EquipmentValidationResult(EquipmentValidationError value = EquipmentValidationError::NONE)
        : error(value) {}

    constexpr bool ok() const { return error == EquipmentValidationError::NONE; }
};

constexpr CapabilityMask KNOWN_EQUIPMENT_CAPABILITIES =
    CAP_BINARY_COMMAND |
    CAP_PROPORTIONAL_COMMAND |
    CAP_BIDIRECTIONAL |
    CAP_TIMED_OPERATION |
    CAP_PULSE_COMMAND |
    CAP_POSITION_FEEDBACK |
    CAP_STATE_FEEDBACK |
    CAP_FAULT_FEEDBACK |
    CAP_SAFE_STATE |
    CAP_SHARED_RESOURCE;

bool hasAllCapabilities(CapabilityMask actual, CapabilityMask required);
bool hasAnyCapability(CapabilityMask actual, CapabilityMask requested);

EquipmentValidationResult validateEquipment(
    const Equipment& equipment,
    const EquipmentTypeDescriptor& descriptor,
    const BoundedArena& arena
);

ParameterBlockRef storeParameterBlock(
    BoundedArena& arena,
    const void* data,
    uint16_t size,
    uint16_t schemaVersion,
    size_t alignment = 1U
);

TextRef storeText(BoundedArena& arena, const char* text, uint16_t length);

const void* resolveParameterBlock(const Equipment& equipment, const BoundedArena& arena);
const char* resolveEquipmentName(const Equipment& equipment, const BoundedArena& arena);

static_assert(sizeof(ParameterBlockRef) == 8U, "ParameterBlockRef layout changed");
static_assert(sizeof(TextRef) == 8U, "TextRef layout changed");
static_assert(sizeof(Equipment) <= 32U, "Equipment must remain compact");

}} // namespace AquaLook::Domain
