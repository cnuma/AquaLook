#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/EquipmentModel.h"
#include "domain/EquipmentOutputTypes.h"

namespace AquaLook { namespace Domain {

namespace EquipmentTypeIds {
constexpr EquipmentTypeId ZONE_VALVE(1U);
constexpr EquipmentTypeId PUMP(2U);
constexpr EquipmentTypeId AUX_CONTACT(3U);
constexpr EquipmentTypeId GREENHOUSE_VENT(4U);
constexpr EquipmentTypeId LIGHTING(5U);
constexpr EquipmentTypeId MISTER(6U);
constexpr EquipmentTypeId FAN(7U);
}

namespace EquipmentParameterSchemas {
constexpr uint16_t BINARY_OUTPUT_V1 = 1U;
constexpr uint16_t ZONE_VALVE_V1 = 1U;
constexpr uint16_t PUMP_V1 = 1U;
}

enum ZoneValveParameterFlags : uint8_t {
    ZONE_VALVE_PARAM_NONE = 0U,
    ZONE_VALVE_PARAM_REQUIRES_PUMP = 1U << 0
};

struct BinaryOutputParameters {
    EquipmentOutputRef output;
    uint16_t minimumOnSec;
    uint16_t minimumOffSec;

    constexpr BinaryOutputParameters()
        : output(), minimumOnSec(0U), minimumOffSec(0U) {}
};

struct ZoneValveParameters {
    EquipmentOutputRef output;
    uint8_t pumpIndex;
    uint8_t flags;

    constexpr ZoneValveParameters()
        : output(), pumpIndex(0U), flags(ZONE_VALVE_PARAM_NONE) {}
};

struct PumpParameters {
    EquipmentOutputRef output;
    uint16_t startupDelayMs;
    uint16_t shutdownDelayMs;
    uint16_t minimumOnSec;
    uint16_t minimumOffSec;

    constexpr PumpParameters()
        : output(), startupDelayMs(0U), shutdownDelayMs(0U),
          minimumOnSec(0U), minimumOffSec(0U) {}
};

enum class EquipmentInventoryValidationError : uint8_t {
    NONE = 0,
    NULL_COLLECTION,
    UNKNOWN_TYPE,
    INVALID_EQUIPMENT,
    DUPLICATE_ID
};

struct EquipmentInventoryValidationResult {
    EquipmentInventoryValidationError error;
    uint16_t index;
    EquipmentId equipmentId;
    EquipmentValidationError equipmentError;

    constexpr EquipmentInventoryValidationResult(
        EquipmentInventoryValidationError value = EquipmentInventoryValidationError::NONE,
        uint16_t itemIndex = 0U,
        EquipmentId id = EquipmentId(),
        EquipmentValidationError detail = EquipmentValidationError::NONE
    ) : error(value), index(itemIndex), equipmentId(id), equipmentError(detail) {}

    constexpr bool ok() const {
        return error == EquipmentInventoryValidationError::NONE;
    }
};

const EquipmentTypeDescriptor* equipmentTypeCatalog(size_t& count);
const EquipmentTypeDescriptor* findEquipmentTypeDescriptor(EquipmentTypeId typeId);
const char* equipmentTypeName(EquipmentTypeId typeId);

bool isZoneValveType(EquipmentTypeId typeId);
bool isPumpType(EquipmentTypeId typeId);
bool isBinaryOutputEquipmentType(EquipmentTypeId typeId);

EquipmentValidationResult validateEquipmentAgainstCatalog(
    const Equipment& equipment,
    const BoundedArena& arena
);

EquipmentInventoryValidationResult validateEquipmentInventory(
    const Equipment* equipments,
    size_t equipmentCount,
    const BoundedArena& arena
);

static_assert(sizeof(BinaryOutputParameters) == 6U,
              "BinaryOutputParameters layout changed");
static_assert(sizeof(ZoneValveParameters) == 4U,
              "ZoneValveParameters layout changed");
static_assert(sizeof(PumpParameters) == 10U,
              "PumpParameters layout changed");

}} // namespace AquaLook::Domain
