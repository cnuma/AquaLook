#include "domain/IntentModel.h"

namespace AquaLook { namespace Domain {

static bool isKnownOrigin(IntentOrigin origin) {
    return origin == IntentOrigin::AUTOMATION ||
           origin == IntentOrigin::MANUAL ||
           origin == IntentOrigin::SAFETY ||
           origin == IntentOrigin::RECOVERY ||
           origin == IntentOrigin::API ||
           origin == IntentOrigin::SYSTEM;
}

static bool isKnownStatus(IntentStatus status) {
    return status == IntentStatus::PENDING ||
           status == IntentStatus::ACCEPTED ||
           status == IntentStatus::REJECTED ||
           status == IntentStatus::SUPERSEDED ||
           status == IntentStatus::EXPIRED ||
           status == IntentStatus::CANCELLED;
}

static void incrementRevision(EquipmentIntent& intent) {
    intent.revision = static_cast<uint8_t>(intent.revision + 1U);
    if (intent.revision == 0U) intent.revision = 1U;
}

IntentValidationResult validateIntent(const EquipmentIntent& intent) {
    if (!intent.id.isValid()) {
        return IntentValidationError::INVALID_ID;
    }
    if (!intent.targetId.isValid()) {
        return IntentValidationError::INVALID_TARGET;
    }
    if (!isKnownOrigin(intent.source.origin)) {
        return IntentValidationError::INVALID_ORIGIN;
    }
    if (intent.requestedState.kind == StateValueKind::UNKNOWN ||
        intent.requestedState.validity != StateValidity::VALID) {
        return IntentValidationError::INVALID_STATE;
    }
    if (!isKnownStatus(intent.status)) {
        return IntentValidationError::INVALID_STATUS;
    }
    if (intent.validUntilMs != 0U &&
        static_cast<int32_t>(intent.validUntilMs - intent.createdAtMs) <= 0) {
        return IntentValidationError::INVALID_VALIDITY_WINDOW;
    }
    return IntentValidationError::NONE;
}

bool isIntentExpired(const EquipmentIntent& intent, uint32_t nowMs) {
    if (intent.validUntilMs == 0U) return false;
    return static_cast<int32_t>(nowMs - intent.validUntilMs) >= 0;
}

bool outranks(const EquipmentIntent& candidate, const EquipmentIntent& current) {
    const uint8_t candidatePriority = static_cast<uint8_t>(candidate.priority);
    const uint8_t currentPriority = static_cast<uint8_t>(current.priority);
    if (candidatePriority != currentPriority) {
        return candidatePriority > currentPriority;
    }

    const int32_t ageDifference = static_cast<int32_t>(candidate.createdAtMs - current.createdAtMs);
    if (ageDifference != 0) {
        return ageDifference > 0;
    }

    return candidate.id.value > current.id.value;
}

void acceptIntent(EquipmentIntent& intent) {
    intent.status = IntentStatus::ACCEPTED;
    intent.rejectionReason = IntentRejectionReason::NONE;
    incrementRevision(intent);
}

void rejectIntent(EquipmentIntent& intent, IntentRejectionReason reason) {
    intent.status = IntentStatus::REJECTED;
    intent.rejectionReason = reason == IntentRejectionReason::NONE
        ? IntentRejectionReason::INTERNAL_ERROR
        : reason;
    incrementRevision(intent);
}

void supersedeIntent(EquipmentIntent& intent) {
    intent.status = IntentStatus::SUPERSEDED;
    intent.rejectionReason = IntentRejectionReason::LOWER_PRIORITY;
    incrementRevision(intent);
}

void expireIntent(EquipmentIntent& intent) {
    intent.status = IntentStatus::EXPIRED;
    intent.rejectionReason = IntentRejectionReason::EXPIRED;
    incrementRevision(intent);
}

void cancelIntent(EquipmentIntent& intent) {
    intent.status = IntentStatus::CANCELLED;
    intent.rejectionReason = IntentRejectionReason::NONE;
    incrementRevision(intent);
}

}} // namespace AquaLook::Domain
