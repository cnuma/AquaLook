#pragma once

#include <stdint.h>

#include "domain/DomainIdentifiers.h"

namespace AquaLook { namespace Domain {

enum class EquipmentOutputRole : uint8_t {
    UNKNOWN = 0,
    ZONE_VALVE = 1,
    PUMP = 2,
    AUX = 3,
    GREENHOUSE_VENT = 4,
    LIGHTING = 5
};

enum class EquipmentOutputKind : uint8_t {
    UNKNOWN = 0,
    BINARY = 1
};

struct EquipmentOutputRef {
    EquipmentOutputRole role;
    uint8_t targetIndex;

    constexpr EquipmentOutputRef()
        : role(EquipmentOutputRole::UNKNOWN), targetIndex(0U) {}

    constexpr EquipmentOutputRef(EquipmentOutputRole outputRole, uint8_t target)
        : role(outputRole), targetIndex(target) {}
};

struct EquipmentOutputCommand {
    EquipmentOutputRef output;
    EquipmentOutputKind kind;
    bool active;

    constexpr EquipmentOutputCommand()
        : output(), kind(EquipmentOutputKind::UNKNOWN), active(false) {}

    constexpr EquipmentOutputCommand(
        EquipmentOutputRef outputRef,
        EquipmentOutputKind outputKind,
        bool requestedActive
    ) : output(outputRef), kind(outputKind), active(requestedActive) {}
};

constexpr EquipmentOutputRef makeZoneValveOutput(uint8_t zoneIndex) {
    return EquipmentOutputRef(EquipmentOutputRole::ZONE_VALVE, zoneIndex);
}

constexpr EquipmentOutputCommand makeZoneValveCommand(uint8_t zoneIndex, bool active) {
    return EquipmentOutputCommand(
        makeZoneValveOutput(zoneIndex),
        EquipmentOutputKind::BINARY,
        active
    );
}

constexpr bool isKnownEquipmentOutputRole(EquipmentOutputRole role) {
    return role == EquipmentOutputRole::ZONE_VALVE ||
           role == EquipmentOutputRole::PUMP ||
           role == EquipmentOutputRole::AUX ||
           role == EquipmentOutputRole::GREENHOUSE_VENT ||
           role == EquipmentOutputRole::LIGHTING;
}

constexpr EquipmentId equipmentIdForZoneValve(uint8_t zoneIndex) {
    return EquipmentId(static_cast<uint16_t>(zoneIndex) + 1U);
}

static_assert(sizeof(EquipmentOutputRef) == 2U,
              "EquipmentOutputRef must remain compact");
static_assert(sizeof(EquipmentOutputCommand) == 4U,
              "EquipmentOutputCommand must remain compact");

}} // namespace AquaLook::Domain
