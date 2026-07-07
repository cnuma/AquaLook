#pragma once

#include <stddef.h>

#include "domain/EquipmentModel.h"

namespace AquaLook { namespace Domain {

namespace EquipmentTypeIds {
constexpr EquipmentTypeId ZONE_VALVE(1U);
constexpr EquipmentTypeId PUMP(2U);
constexpr EquipmentTypeId AUXILIARY(3U);
constexpr EquipmentTypeId GREENHOUSE_VENT(4U);
constexpr EquipmentTypeId LIGHTING(5U);
}

const EquipmentTypeDescriptor* equipmentTypeCatalog(size_t& count);
const EquipmentTypeDescriptor* findEquipmentTypeDescriptor(EquipmentTypeId id);

}} // namespace AquaLook::Domain
