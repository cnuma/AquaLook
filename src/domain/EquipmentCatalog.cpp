#include "domain/EquipmentCatalog.h"

namespace AquaLook { namespace Domain {

namespace {

constexpr CapabilityMask BINARY_BASE_CAPABILITIES =
    CAP_BINARY_COMMAND |
    CAP_STATE_FEEDBACK |
    CAP_SAFE_STATE;

constexpr EquipmentTypeDescriptor AQUALOOK_EQUIPMENT_CATALOG[] = {
    {
        EquipmentTypeIds::ZONE_VALVE,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION | CAP_FAULT_FEEDBACK,
        EquipmentParameterSchemas::ZONE_VALVE_V1,
        sizeof(ZoneValveParameters),
        sizeof(ZoneValveParameters),
        "zone_valve"
    },
    {
        EquipmentTypeIds::PUMP,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION | CAP_SHARED_RESOURCE,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION | CAP_SHARED_RESOURCE |
            CAP_FAULT_FEEDBACK,
        EquipmentParameterSchemas::PUMP_V1,
        sizeof(PumpParameters),
        sizeof(PumpParameters),
        "pump"
    },
    {
        EquipmentTypeIds::AUX_CONTACT,
        BINARY_BASE_CAPABILITIES,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION | CAP_PULSE_COMMAND |
            CAP_FAULT_FEEDBACK,
        EquipmentParameterSchemas::BINARY_OUTPUT_V1,
        sizeof(BinaryOutputParameters),
        sizeof(BinaryOutputParameters),
        "aux_contact"
    },
    {
        EquipmentTypeIds::GREENHOUSE_VENT,
        BINARY_BASE_CAPABILITIES,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION | CAP_POSITION_FEEDBACK |
            CAP_FAULT_FEEDBACK,
        EquipmentParameterSchemas::BINARY_OUTPUT_V1,
        sizeof(BinaryOutputParameters),
        sizeof(BinaryOutputParameters),
        "greenhouse_vent"
    },
    {
        EquipmentTypeIds::LIGHTING,
        BINARY_BASE_CAPABILITIES,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION | CAP_FAULT_FEEDBACK,
        EquipmentParameterSchemas::BINARY_OUTPUT_V1,
        sizeof(BinaryOutputParameters),
        sizeof(BinaryOutputParameters),
        "lighting"
    },
    {
        EquipmentTypeIds::MISTER,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION | CAP_FAULT_FEEDBACK,
        EquipmentParameterSchemas::BINARY_OUTPUT_V1,
        sizeof(BinaryOutputParameters),
        sizeof(BinaryOutputParameters),
        "mister"
    },
    {
        EquipmentTypeIds::FAN,
        BINARY_BASE_CAPABILITIES,
        BINARY_BASE_CAPABILITIES | CAP_TIMED_OPERATION | CAP_FAULT_FEEDBACK,
        EquipmentParameterSchemas::BINARY_OUTPUT_V1,
        sizeof(BinaryOutputParameters),
        sizeof(BinaryOutputParameters),
        "fan"
    }
};

constexpr size_t AQUALOOK_EQUIPMENT_CATALOG_COUNT =
    sizeof(AQUALOOK_EQUIPMENT_CATALOG) / sizeof(AQUALOOK_EQUIPMENT_CATALOG[0]);

bool hasDuplicateEquipmentId(
    const Equipment* equipments,
    size_t equipmentCount,
    size_t index
) {
    (void)equipmentCount;
    for (size_t previous = 0U; previous < index; ++previous) {
        if (equipments[previous].id == equipments[index].id) {
            return true;
        }
    }
    return false;
}

} // namespace

const EquipmentTypeDescriptor* aquaLookEquipmentTypeCatalog(size_t& count) {
    count = AQUALOOK_EQUIPMENT_CATALOG_COUNT;
    return AQUALOOK_EQUIPMENT_CATALOG;
}

const EquipmentTypeDescriptor* findAquaLookEquipmentTypeDescriptor(EquipmentTypeId typeId) {
    for (size_t index = 0U; index < AQUALOOK_EQUIPMENT_CATALOG_COUNT; ++index) {
        if (AQUALOOK_EQUIPMENT_CATALOG[index].id == typeId) {
            return &AQUALOOK_EQUIPMENT_CATALOG[index];
        }
    }
    return nullptr;
}

const char* aquaLookEquipmentTypeName(EquipmentTypeId typeId) {
    const EquipmentTypeDescriptor* descriptor =
        findAquaLookEquipmentTypeDescriptor(typeId);
    return descriptor ? descriptor->technicalName : "unknown";
}

bool isZoneValveType(EquipmentTypeId typeId) {
    return typeId == EquipmentTypeIds::ZONE_VALVE;
}

bool isPumpType(EquipmentTypeId typeId) {
    return typeId == EquipmentTypeIds::PUMP;
}

bool isBinaryOutputEquipmentType(EquipmentTypeId typeId) {
    const EquipmentTypeDescriptor* descriptor =
        findAquaLookEquipmentTypeDescriptor(typeId);
    return descriptor &&
           hasAnyCapability(descriptor->requiredCapabilities, CAP_BINARY_COMMAND);
}

EquipmentValidationResult validateEquipmentAgainstCatalog(
    const Equipment& equipment,
    const BoundedArena& arena
) {
    const EquipmentTypeDescriptor* descriptor =
        findAquaLookEquipmentTypeDescriptor(equipment.typeId);
    if (!descriptor) {
        return EquipmentValidationError::INVALID_TYPE_ID;
    }
    return validateEquipment(equipment, *descriptor, arena);
}

EquipmentInventoryValidationResult validateEquipmentInventory(
    const Equipment* equipments,
    size_t equipmentCount,
    const BoundedArena& arena
) {
    if (equipmentCount != 0U && !equipments) {
        return EquipmentInventoryValidationResult(
            EquipmentInventoryValidationError::NULL_COLLECTION
        );
    }

    for (size_t index = 0U; index < equipmentCount; ++index) {
        const Equipment& equipment = equipments[index];
        const EquipmentTypeDescriptor* descriptor =
            findAquaLookEquipmentTypeDescriptor(equipment.typeId);
        if (!descriptor) {
            return EquipmentInventoryValidationResult(
                EquipmentInventoryValidationError::UNKNOWN_TYPE,
                static_cast<uint16_t>(index),
                equipment.id,
                EquipmentValidationError::INVALID_TYPE_ID
            );
        }

        const EquipmentValidationResult validation =
            validateEquipment(equipment, *descriptor, arena);
        if (!validation.ok()) {
            return EquipmentInventoryValidationResult(
                EquipmentInventoryValidationError::INVALID_EQUIPMENT,
                static_cast<uint16_t>(index),
                equipment.id,
                validation.error
            );
        }

        if (hasDuplicateEquipmentId(equipments, equipmentCount, index)) {
            return EquipmentInventoryValidationResult(
                EquipmentInventoryValidationError::DUPLICATE_ID,
                static_cast<uint16_t>(index),
                equipment.id
            );
        }
    }

    return EquipmentInventoryValidationResult();
}

}} // namespace AquaLook::Domain
