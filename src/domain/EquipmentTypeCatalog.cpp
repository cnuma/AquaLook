#include "domain/EquipmentTypeCatalog.h"

namespace AquaLook { namespace Domain {

namespace {

constexpr EquipmentTypeDescriptor CATALOG[] = {
    {
        EquipmentTypeIds::ZONE_VALVE,
        CAP_BINARY_COMMAND | CAP_TIMED_OPERATION | CAP_SAFE_STATE,
        CAP_BINARY_COMMAND | CAP_TIMED_OPERATION | CAP_STATE_FEEDBACK |
            CAP_FAULT_FEEDBACK | CAP_SAFE_STATE,
        1U,
        0U,
        64U,
        "zone_valve"
    },
    {
        EquipmentTypeIds::PUMP,
        CAP_BINARY_COMMAND | CAP_TIMED_OPERATION | CAP_SAFE_STATE,
        CAP_BINARY_COMMAND | CAP_TIMED_OPERATION | CAP_STATE_FEEDBACK |
            CAP_FAULT_FEEDBACK | CAP_SAFE_STATE | CAP_SHARED_RESOURCE,
        1U,
        0U,
        64U,
        "pump"
    },
    {
        EquipmentTypeIds::AUXILIARY,
        CAP_BINARY_COMMAND,
        CAP_BINARY_COMMAND | CAP_TIMED_OPERATION | CAP_STATE_FEEDBACK |
            CAP_FAULT_FEEDBACK | CAP_SAFE_STATE,
        1U,
        0U,
        64U,
        "auxiliary"
    },
    {
        EquipmentTypeIds::GREENHOUSE_VENT,
        CAP_BIDIRECTIONAL | CAP_SAFE_STATE,
        CAP_BINARY_COMMAND | CAP_PROPORTIONAL_COMMAND | CAP_BIDIRECTIONAL |
            CAP_TIMED_OPERATION | CAP_POSITION_FEEDBACK | CAP_STATE_FEEDBACK |
            CAP_FAULT_FEEDBACK | CAP_SAFE_STATE,
        1U,
        0U,
        96U,
        "greenhouse_vent"
    },
    {
        EquipmentTypeIds::LIGHTING,
        CAP_BINARY_COMMAND,
        CAP_BINARY_COMMAND | CAP_PROPORTIONAL_COMMAND | CAP_TIMED_OPERATION |
            CAP_STATE_FEEDBACK | CAP_FAULT_FEEDBACK | CAP_SAFE_STATE,
        1U,
        0U,
        64U,
        "lighting"
    }
};

constexpr size_t CATALOG_COUNT = sizeof(CATALOG) / sizeof(CATALOG[0]);

} // namespace

const EquipmentTypeDescriptor* equipmentTypeCatalog(size_t& count) {
    count = CATALOG_COUNT;
    return CATALOG;
}

const EquipmentTypeDescriptor* findEquipmentTypeDescriptor(EquipmentTypeId id) {
    for (size_t i = 0U; i < CATALOG_COUNT; ++i) {
        if (CATALOG[i].id == id) return &CATALOG[i];
    }
    return nullptr;
}

}} // namespace AquaLook::Domain
