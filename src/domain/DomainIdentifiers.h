#pragma once

#include <stdint.h>

namespace AquaLook { namespace Domain {

template <typename Tag>
struct StrongId {
    uint16_t value;

    constexpr StrongId() : value(0) {}
    explicit constexpr StrongId(uint16_t raw) : value(raw) {}

    constexpr bool isValid() const { return value != 0U && value != 0xFFFFU; }
    constexpr explicit operator bool() const { return isValid(); }

    friend constexpr bool operator==(StrongId lhs, StrongId rhs) {
        return lhs.value == rhs.value;
    }
    friend constexpr bool operator!=(StrongId lhs, StrongId rhs) {
        return !(lhs == rhs);
    }
};

struct EquipmentIdTag;
struct EquipmentTypeIdTag;
struct BoardIdTag;
struct SensorIdTag;
struct AutomationIdTag;
struct ExecutionIdTag;
struct IntentIdTag;
struct CorrelationIdTag;
struct ZoneIdTag;

using EquipmentId = StrongId<EquipmentIdTag>;
using EquipmentTypeId = StrongId<EquipmentTypeIdTag>;
using BoardId = StrongId<BoardIdTag>;
using SensorId = StrongId<SensorIdTag>;
using AutomationId = StrongId<AutomationIdTag>;
using ExecutionId = StrongId<ExecutionIdTag>;
using IntentId = StrongId<IntentIdTag>;
using CorrelationId = StrongId<CorrelationIdTag>;
using ZoneId = StrongId<ZoneIdTag>;

static_assert(sizeof(EquipmentId) == sizeof(uint16_t), "Strong IDs must remain compact");
static_assert(sizeof(IntentId) == sizeof(uint16_t), "Intent IDs must remain compact");

}} // namespace AquaLook::Domain
