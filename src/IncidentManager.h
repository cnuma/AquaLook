#pragma once

#include <Arduino.h>

enum class IncidentState : uint8_t {
    NONE = 0,
    ACTIVE,
    RECOVERED_UNACKNOWLEDGED,
    ACKNOWLEDGED
};

enum class IncidentNotification : uint8_t {
    INITIAL = 0x01,
    ESCALATION = 0x02,
    RECOVERY = 0x04
};

struct PersistentIncidentSnapshot {
    IncidentState state = IncidentState::NONE;
    uint32_t occurrences = 0;
    uint32_t firstEpoch = 0;
    uint32_t lastEpoch = 0;
    uint8_t pendingNotifications = 0;
    char lastReason[32] = "";
};

class IncidentManager {
public:
    static void begin();

    static void activateStorageSd(const char* reason);
    static void escalateStorageSdRecoveryFailure();
    static void recoverStorageSd(const char* reason);
    static void acknowledgeStorageSd();

    static PersistentIncidentSnapshot storageSd();
    static bool storageSdNotificationPending(IncidentNotification type);
    static void markStorageSdNotificationDelivered(IncidentNotification type);

    static const char* stateCode(IncidentState state);

private:
    static void load();
    static void save();
    static uint32_t currentEpoch();
};