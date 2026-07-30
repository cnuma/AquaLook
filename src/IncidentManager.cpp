#include "IncidentManager.h"

#include <Preferences.h>
#include <time.h>

#include "EventLog.h"

namespace {
constexpr char INCIDENT_NVS_NAMESPACE[] = "aq_incidents";
constexpr char INCIDENT_NVS_KEY[] = "storage_sd";
constexpr uint8_t INCIDENT_SCHEMA = 1U;

struct StoredIncidentRecord {
    uint8_t schema;
    uint8_t state;
    uint8_t pendingNotifications;
    uint8_t reserved;
    uint32_t occurrences;
    uint32_t firstEpoch;
    uint32_t lastEpoch;
    char lastReason[32];
};

PersistentIncidentSnapshot g_storageSd;
bool g_loaded = false;

void copyReason(char* target, size_t targetSize, const char* reason) {
    if (!target || targetSize == 0U) return;
    strlcpy(target, reason ? reason : "unknown", targetSize);
}

uint8_t notificationMask(IncidentNotification type) {
    return static_cast<uint8_t>(type);
}
}

void IncidentManager::begin() {
    if (g_loaded) return;
    load();
    g_loaded = true;

    if (g_storageSd.state != IncidentState::NONE) {
        EventLog::log(
            LOG_INFO,
            "Incident SD: restaure etat=%s occurrences=%lu notifications=0x%02X",
            stateCode(g_storageSd.state),
            static_cast<unsigned long>(g_storageSd.occurrences),
            static_cast<unsigned>(g_storageSd.pendingNotifications)
        );
    }
}

void IncidentManager::activateStorageSd(const char* reason) {
    begin();

    const bool newOccurrence =
        g_storageSd.state == IncidentState::NONE ||
        g_storageSd.state == IncidentState::ACKNOWLEDGED ||
        g_storageSd.state == IncidentState::RECOVERED_UNACKNOWLEDGED;

    const uint32_t nowEpoch = currentEpoch();

    if (newOccurrence) {
        g_storageSd.occurrences++;
        if (g_storageSd.firstEpoch == 0U && nowEpoch != 0U) {
            g_storageSd.firstEpoch = nowEpoch;
        }
        g_storageSd.pendingNotifications |=
            notificationMask(IncidentNotification::INITIAL);
    }

    g_storageSd.state = IncidentState::ACTIVE;
    if (nowEpoch != 0U) g_storageSd.lastEpoch = nowEpoch;
    copyReason(g_storageSd.lastReason, sizeof(g_storageSd.lastReason), reason);

    save();

    EventLog::log(
        LOG_WARN,
        "Incident SD: actif occurrences=%lu raison=%s",
        static_cast<unsigned long>(g_storageSd.occurrences),
        g_storageSd.lastReason
    );
}

void IncidentManager::escalateStorageSdRecoveryFailure() {
    begin();

    if (g_storageSd.state == IncidentState::NONE) {
        activateStorageSd("recovery_failed");
    }

    g_storageSd.state = IncidentState::ACTIVE;
    g_storageSd.pendingNotifications |=
        notificationMask(IncidentNotification::ESCALATION);

    const uint32_t nowEpoch = currentEpoch();
    if (nowEpoch != 0U) g_storageSd.lastEpoch = nowEpoch;
    copyReason(
        g_storageSd.lastReason,
        sizeof(g_storageSd.lastReason),
        "recovery_failed"
    );

    save();

    EventLog::log(
        LOG_ERROR,
        "Incident SD: echec recuperation persiste en NVS"
    );
}

void IncidentManager::recoverStorageSd(const char* reason) {
    begin();

    if (g_storageSd.state == IncidentState::NONE) return;

    g_storageSd.state = IncidentState::RECOVERED_UNACKNOWLEDGED;
    g_storageSd.pendingNotifications |=
        notificationMask(IncidentNotification::RECOVERY);

    const uint32_t nowEpoch = currentEpoch();
    if (nowEpoch != 0U) g_storageSd.lastEpoch = nowEpoch;
    copyReason(
        g_storageSd.lastReason,
        sizeof(g_storageSd.lastReason),
        reason ? reason : "recovered"
    );

    save();

    EventLog::log(
        LOG_INFO,
        "Incident SD: recupere, acquittement utilisateur requis"
    );
}

void IncidentManager::acknowledgeStorageSd() {
    begin();

    if (g_storageSd.state == IncidentState::NONE) return;

    g_storageSd.state = IncidentState::ACKNOWLEDGED;
    save();

    EventLog::log(LOG_INFO, "Incident SD: acquitte");
}

PersistentIncidentSnapshot IncidentManager::storageSd() {
    begin();
    return g_storageSd;
}

bool IncidentManager::storageSdNotificationPending(IncidentNotification type) {
    begin();
    return (g_storageSd.pendingNotifications & notificationMask(type)) != 0U;
}

void IncidentManager::markStorageSdNotificationDelivered(
    IncidentNotification type
) {
    begin();

    const uint8_t mask = notificationMask(type);
    if ((g_storageSd.pendingNotifications & mask) == 0U) return;

    g_storageSd.pendingNotifications &= static_cast<uint8_t>(~mask);
    save();
}

const char* IncidentManager::stateCode(IncidentState state) {
    switch (state) {
        case IncidentState::ACTIVE:
            return "active";
        case IncidentState::RECOVERED_UNACKNOWLEDGED:
            return "recovered-unacknowledged";
        case IncidentState::ACKNOWLEDGED:
            return "acknowledged";
        default:
            return "none";
    }
}

void IncidentManager::load() {
    g_storageSd = PersistentIncidentSnapshot{};

    Preferences preferences;
    if (!preferences.begin(INCIDENT_NVS_NAMESPACE, true)) {
        EventLog::log(LOG_WARN, "Incident SD: ouverture NVS lecture impossible");
        return;
    }

    const size_t storedSize = preferences.getBytesLength(INCIDENT_NVS_KEY);
    if (storedSize != sizeof(StoredIncidentRecord)) {
        preferences.end();
        return;
    }

    StoredIncidentRecord stored{};
    const size_t readSize = preferences.getBytes(
        INCIDENT_NVS_KEY,
        &stored,
        sizeof(stored)
    );
    preferences.end();

    if (readSize != sizeof(stored) || stored.schema != INCIDENT_SCHEMA) {
        EventLog::log(LOG_WARN, "Incident SD: enregistrement NVS invalide");
        return;
    }

    g_storageSd.state = static_cast<IncidentState>(stored.state);
    g_storageSd.occurrences = stored.occurrences;
    g_storageSd.firstEpoch = stored.firstEpoch;
    g_storageSd.lastEpoch = stored.lastEpoch;
    g_storageSd.pendingNotifications = stored.pendingNotifications;
    copyReason(
        g_storageSd.lastReason,
        sizeof(g_storageSd.lastReason),
        stored.lastReason
    );
}

void IncidentManager::save() {
    StoredIncidentRecord stored{};
    stored.schema = INCIDENT_SCHEMA;
    stored.state = static_cast<uint8_t>(g_storageSd.state);
    stored.pendingNotifications = g_storageSd.pendingNotifications;
    stored.occurrences = g_storageSd.occurrences;
    stored.firstEpoch = g_storageSd.firstEpoch;
    stored.lastEpoch = g_storageSd.lastEpoch;
    copyReason(stored.lastReason, sizeof(stored.lastReason), g_storageSd.lastReason);

    Preferences preferences;
    if (!preferences.begin(INCIDENT_NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "Incident SD: ouverture NVS ecriture impossible");
        return;
    }

    const size_t written = preferences.putBytes(
        INCIDENT_NVS_KEY,
        &stored,
        sizeof(stored)
    );
    preferences.end();

    if (written != sizeof(stored)) {
        EventLog::log(LOG_ERROR, "Incident SD: ecriture NVS incomplete");
    }
}

uint32_t IncidentManager::currentEpoch() {
    const time_t now = time(nullptr);
    if (now < 1700000000) return 0U;
    return static_cast<uint32_t>(now);
}
