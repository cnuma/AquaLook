#pragma once

#include <Arduino.h>
#include "IncidentManager.h"

class ConfigManager;

// Arduino-ESP32 2.0.17 ne fournit pas WiFiClientSecure::setBufferSizes().
// NotificationManager.cpp est le seul module qui appelle cette API optionnelle.
// Sur ce core, l'appel est remappe vers setTimeout() afin de conserver la
// compatibilite de compilation sans desactiver la validation TLS.
#if defined(ARDUINO_ARCH_ESP32) && (__INCLUDE_LEVEL__ == 1)
#define setBufferSizes(rxSize, txSize) setTimeout(8)
#endif

struct NotificationConfig {
    bool enabled = false;
    char server[96] = "https://ntfy.sh";
    char topic[96] = "";
    char token[160] = "";
};

struct NotificationStatus {
    bool configured = false;
    bool enabled = false;
    bool workerRunning = false;
    bool testPending = false;
    bool updatePending = false;
    uint8_t pendingMask = 0;
    uint8_t pendingZoneEvents = 0;
    uint32_t attempts = 0;
    uint32_t nextAttemptInSec = 0;
    int lastHttpCode = 0;
    char lastResult[48] = "not-started";
};

class NotificationManager {
public:
    enum class WorkType : uint8_t {
        NONE = 0,
        INCIDENT_INITIAL,
        INCIDENT_ESCALATION,
        INCIDENT_RECOVERY,
        MANUAL_TEST,
        ZONE_EVENT,
        UPDATE_AVAILABLE
    };

    enum class WorkerResult : uint8_t {
        IDLE = 0,
        RUNNING,
        SUCCESS,
        FAILED,
        START_FAILED
    };

    static void begin();
    static void bindConfig(ConfigManager* config);
    static void update();
    static NotificationConfig config();
    static NotificationStatus status();
    static bool saveConfig(bool enabled,
                           const char* server,
                           const char* topic,
                           const char* token,
                           bool preserveTokenWhenEmpty);
    static bool requestTest();
    static bool enqueueZoneEvent(uint8_t zone, bool active);

private:
    static void loadConfig();
    static bool persistConfig();
    static void supervisorTask(void* parameter);
    static void senderTask(void* parameter);
    static void startSender(WorkType type);
    static void processWorkerResult(uint32_t nowMs);
    static void scheduleNextAttempt(uint32_t nowMs);
    static WorkType nextWork();
    static bool sendCurrentWork();
    static bool validServer(const char* server);
    static bool validTopic(const char* topic);
    static const char* workCode(WorkType type);
    static IncidentNotification incidentNotificationFor(WorkType type);
};
