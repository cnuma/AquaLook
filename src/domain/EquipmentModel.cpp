#include "domain/EquipmentModel.h"

#include <string.h>

namespace AquaLook { namespace Domain {

bool hasAllCapabilities(CapabilityMask actual, CapabilityMask required) {
    return (actual & required) == required;
}

bool hasAnyCapability(CapabilityMask actual, CapabilityMask requested) {
    return (actual & requested) != 0U;
}

static bool isValidMode(EquipmentMode mode) {
    return mode == EquipmentMode::DISABLED ||
           mode == EquipmentMode::AUTOMATIC ||
           mode == EquipmentMode::MANUAL ||
           mode == EquipmentMode::MAINTENANCE;
}

static bool isValidSafeState(SafeState state) {
    return state == SafeState::UNSPECIFIED ||
           state == SafeState::INACTIVE ||
           state == SafeState::ACTIVE ||
           state == SafeState::HOLD_LAST;
}

EquipmentValidationResult validateEquipment(
    const Equipment& equipment,
    const EquipmentTypeDescriptor& descriptor,
    const BoundedArena& arena
) {
    if (!equipment.id.isValid()) {
        return EquipmentValidationError::INVALID_ID;
    }
    if (!equipment.typeId.isValid() || equipment.typeId != descriptor.id) {
        return EquipmentValidationError::INVALID_TYPE_ID;
    }
    if ((equipment.capabilities & ~KNOWN_EQUIPMENT_CAPABILITIES) != 0U) {
        return EquipmentValidationError::UNKNOWN_CAPABILITY;
    }
    if (!hasAllCapabilities(equipment.capabilities, descriptor.requiredCapabilities)) {
        return EquipmentValidationError::MISSING_REQUIRED_CAPABILITY;
    }
    if ((equipment.capabilities & ~descriptor.supportedCapabilities) != 0U) {
        return EquipmentValidationError::UNSUPPORTED_CAPABILITY;
    }
    if (!isValidMode(equipment.mode)) {
        return EquipmentValidationError::INVALID_MODE;
    }
    if (!isValidSafeState(equipment.safeState)) {
        return EquipmentValidationError::INVALID_SAFE_STATE;
    }

    if (equipment.parameters.isPresent()) {
        if (!arena.pointerAt(equipment.parameters.offset, equipment.parameters.size)) {
            return EquipmentValidationError::INVALID_PARAMETER_REFERENCE;
        }
        if (equipment.parameters.size < descriptor.minimumParameterBytes ||
            equipment.parameters.size > descriptor.maximumParameterBytes) {
            return EquipmentValidationError::INVALID_PARAMETER_SIZE;
        }
        if (equipment.parameters.schemaVersion != descriptor.parameterSchemaVersion) {
            return EquipmentValidationError::INVALID_PARAMETER_SCHEMA;
        }
    } else if (descriptor.minimumParameterBytes != 0U) {
        return EquipmentValidationError::INVALID_PARAMETER_SIZE;
    }

    if (equipment.name.isPresent()) {
        const char* name = static_cast<const char*>(
            arena.pointerAt(equipment.name.offset, static_cast<size_t>(equipment.name.size) + 1U)
        );
        if (!name || name[equipment.name.size] != '\0') {
            return EquipmentValidationError::INVALID_NAME_REFERENCE;
        }
    }

    return EquipmentValidationError::NONE;
}

ParameterBlockRef storeParameterBlock(
    BoundedArena& arena,
    const void* data,
    uint16_t size,
    uint16_t schemaVersion,
    size_t alignment
) {
    ParameterBlockRef result;
    if (!data || size == 0U || schemaVersion == 0U) return result;

    const uint32_t offset = arena.appendBytes(data, size, alignment);
    if (offset == BoundedArena::INVALID_OFFSET) return result;

    result.offset = offset;
    result.size = size;
    result.schemaVersion = schemaVersion;
    return result;
}

TextRef storeText(BoundedArena& arena, const char* text, uint16_t length) {
    TextRef result;
    if (!text || length == 0U) return result;

    void* destination = arena.allocate(static_cast<size_t>(length) + 1U, 1U);
    if (!destination) return result;

    memcpy(destination, text, length);
    static_cast<char*>(destination)[length] = '\0';

    result.offset = arena.offsetOf(destination);
    result.size = length;
    return result;
}

const void* resolveParameterBlock(const Equipment& equipment, const BoundedArena& arena) {
    if (!equipment.parameters.isPresent()) return nullptr;
    return arena.pointerAt(equipment.parameters.offset, equipment.parameters.size);
}

const char* resolveEquipmentName(const Equipment& equipment, const BoundedArena& arena) {
    if (!equipment.name.isPresent()) return nullptr;
    return static_cast<const char*>(
        arena.pointerAt(equipment.name.offset, static_cast<size_t>(equipment.name.size) + 1U)
    );
}

}} // namespace AquaLook::Domain
