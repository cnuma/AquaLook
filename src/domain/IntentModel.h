#pragma once

#include <stdint.h>

#include "domain/DomainIdentifiers.h"
#include "domain/EquipmentRuntimeState.h"

namespace AquaLook { namespace Domain {

enum class IntentOrigin : uint8_t {
    UNKNOWN = 0,
    AUTOMATION = 1,
    MANUAL = 2,
    SAFETY = 3,
    RECOVERY = 4,
    API = 5,
    SYSTEM = 6
};

enum class IntentPriority : uint8_t {
    BACKGROUND = 0,
    NORMAL = 64,
    HIGH = 128,
    SAFETY = 192,
    EMERGENCY = 255
};

enum class IntentStatus : uint8_t {
    PENDING = 0,
    ACCEPTED = 1,
    REJECTED = 2,
    SUPERSEDED = 3,
    EXPIRED = 4,
    CANCELLED = 5
};

enum class IntentRejectionReason : uint16_t {
    NONE = 0,
    INVALID_ID = 1,
    INVALID_TARGET = 2,
    INVALID_STATE = 3,
    INVALID_ORIGIN = 4,
    EXPIRED = 5,
    EQUIPMENT_DISABLED = 6,
    LOWER_PRIORITY = 7,
    INTERLOCKED = 8,
    DEPENDENCY_UNAVAILABLE = 9,
    BLOCKING_FAULT = 10,
    CAPABILITY_NOT_SUPPORTED = 11,
    CONFLICT = 12,
    INTERNAL_ERROR = 13
};

enum IntentFlags : uint8_t {
    INTENT_FLAG_NONE = 0U,
    INTENT_FLAG_REPLACEABLE = 1U << 0,
    INTENT_FLAG_REQUIRES_OBSERVATION = 1U << 1,
    INTENT_FLAG_STICKY = 1U << 2
};

struct IntentSourceRef {
    IntentOrigin origin;
    uint8_t reserved;
    uint16_t sourceId;

    constexpr IntentSourceRef()
        : origin(IntentOrigin::UNKNOWN), reserved(0U), sourceId(0U) {}
};

struct EquipmentIntent {
    IntentId id;
    EquipmentId targetId;
    CorrelationId correlationId;
    IntentSourceRef source;
    EquipmentStateValue requestedState;
    uint32_t createdAtMs;
    uint32_t validUntilMs;
    IntentPriority priority;
    IntentStatus status;
    uint8_t flags;
    uint8_t revision;
    IntentRejectionReason rejectionReason;

    constexpr EquipmentIntent()
        : id(), targetId(), correlationId(), source(), requestedState(),
          createdAtMs(0U), validUntilMs(0U), priority(IntentPriority::NORMAL),
          status(IntentStatus::PENDING), flags(INTENT_FLAG_NONE), revision(0U),
          rejectionReason(IntentRejectionReason::NONE) {}
};

enum class IntentValidationError : uint8_t {
    NONE = 0,
    INVALID_ID,
    INVALID_TARGET,
    INVALID_ORIGIN,
    INVALID_STATE,
    INVALID_STATUS,
    INVALID_VALIDITY_WINDOW
};

struct IntentValidationResult {
    IntentValidationError error;

    constexpr IntentValidationResult(IntentValidationError value = IntentValidationError::NONE)
        : error(value) {}

    constexpr bool ok() const { return error == IntentValidationError::NONE; }
};

IntentValidationResult validateIntent(const EquipmentIntent& intent);
bool isIntentExpired(const EquipmentIntent& intent, uint32_t nowMs);
bool outranks(const EquipmentIntent& candidate, const EquipmentIntent& current);

void acceptIntent(EquipmentIntent& intent);
void rejectIntent(EquipmentIntent& intent, IntentRejectionReason reason);
void supersedeIntent(EquipmentIntent& intent);
void expireIntent(EquipmentIntent& intent);
void cancelIntent(EquipmentIntent& intent);

static_assert(sizeof(IntentSourceRef) == 4U, "IntentSourceRef layout changed");
static_assert(sizeof(EquipmentIntent) <= 32U, "EquipmentIntent must remain compact");

}} // namespace AquaLook::Domain
