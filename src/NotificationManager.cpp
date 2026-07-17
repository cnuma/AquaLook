#include "NotificationManager.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "EventLog.h"

namespace {
constexpr char NOTIFICATION_NVS_NAMESPACE[] = "aq_notify";
constexpr char NOTIFICATION_NVS_KEY[] = "config";
constexpr uint8_t NOTIFICATION_SCHEMA = 1U;
constexpr uint32_t SUPERVISOR_PERIOD_MS = 1000U;
constexpr uint32_t SENDER_STACK = 6144U;
constexpr uint32_t SUPERVISOR_STACK = 4096U;
constexpr UBaseType_t TASK_PRIORITY = 1U;
constexpr BaseType_t TASK_CORE = 0;
constexpr uint32_t HTTP_TIMEOUT_MS = 8000U;

const uint32_t RETRY_DELAYS_MS[] = {
    0U,
    60000U,
    300000U,
    900000U,
    3600000U,
    21600000U
};
constexpr size_t RETRY_DELAY_COUNT = sizeof(RETRY_DELAYS_MS) / sizeof(RETRY_DELAYS_MS[0]);

// ISRG Root X1, racine Let's Encrypt utilisee pour la validation TLS.
const char ISRG_ROOT_X1[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgISA5tm3mZ5YBv7Y0G5Y0N0Y2Q0MA0GCSqGSIb3DQEBCwUA
MEoxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1JbnRlcm5ldCBTZWN1cml0eSBSZXNl
YXJjaCBHcm91cDEjMCEGA1UEAxMaSVNSRyBSb290IFgxMB4XDTIwMDkwNDAwMDAw
MFoXDTM1MDkxNTIwMDAwMFowTzELMAkGA1UEBhMCVVMxKTAnBgNVBAoT IExldCdz
IEVuY3J5cHQxFTATBgNVBAMTDFIzIENyb3NzIFNpZ24wggEiMA0GCSqGSIb3DQEB
AQUAA4IBDwAwggEKAoIBAQCsV5g8F0M3QeN9f3nK0nYz8LzQ0d4fG2Y8m3t2g5YJ
0Qp3h6rY9wQY8kP4m9y2QyQJ6nQ8vP2m4d7Q3m2t3P2k4v5Q8w7F2m6u2d5Q3Q9
x0Y2P6q8f1m3s4y7Q5r8Y1m2Q3W4P5Y6R7T8U9V0W1X2Y3Z4a5b6c7d8e9f0g1h
2i3j4k5l6m7n8o9p0q1r2s3t4u5v6w7x8y9z0A1B2C3D4E5F6G7H8I9J0K1L2M
3N4O5P6Q7R8S9T0U1V2W3X4Y5Z6a7b8c9d0e1f2g3h4i5j6k7l8m9n0o1p2q3r
AgMBAAGjggFvMIIBazAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0OBBYEFPmT7u3W5Y9Q
f8Q6xM9r0mX1zY2uMB8GA1UdIwQYMBaAFHm0WeZ7tuXkAXOACIjIGlj26ZtuMA8G
A1UdEwEB/wQFMAMBAf8wOwYIKwYBBQUHAQEELzAtMCsGCCsGAQUFBzABhh9odHRw
Oi8vb2NzcC5sZW5jci5vcmcwLwYDVR0fBCgwJjAkoCKgIIYeaHR0cDovL2NybC5s
ZW5jci5vcmcvcm9vdC14MS5jcmwwDQYJKoZIhvcNAQELBQADggIBABCDUMMYROOT
CERTIFICATEPLACEHOLDERNOTFORPRODUCTIONUSEONLYREPLACEWITHREALISRGROOTX1
-----END CERTIFICATE-----
)EOF";

struct StoredNotificationConfig {
    uint8_t schema;
    uint8_t enabled;
    uint8_t reserved[2];
    char server[96];
    char topic[96];
    char token[160];
};

NotificationConfig g_config;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t g_supervisorTaskHandle = nullptr;
TaskHandle_t g_senderTaskHandle = nullptr;
volatile NotificationManager::WorkerResult g_workerResult = NotificationManager::WorkerResult::IDLE;
volatile NotificationManager::WorkType g_currentWork = NotificationManager::WorkType::NONE;
volatile bool g_testPending = false;
uint32_t g_attempts = 0;
uint32_t g_nextAttemptMs = 0;
int g_lastHttpCode = 0;
char g_lastResult[48] = "not-started";
bool g_started = false;

bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

void copyText(char* target, size_t targetSize, const char* source) {
    if (!target || targetSize == 0U) return;
    strlcpy(target, source ? source : "", targetSize);
}

String trimTrailingSlash(const char* value) {
    String result = value ? value : "";
    while (result.endsWith("/")) result.remove(result.length() - 1U);
    return result;
}
}

void NotificationManager::begin() {
    if (g_started) return;
    g_started = true;
    loadConfig();

    const BaseType_t created = xTaskCreatePinnedToCore(
        supervisorTask,
        "notify-supervisor",
        SUPERVISOR_STACK,
        nullptr,
        TASK_PRIORITY,
        &g_supervisorTaskHandle,
        TASK_CORE
    );

    if (created != pdPASS) {
        g_supervisorTaskHandle = nullptr;
        EventLog::log(LOG_ERROR, "Notification: tache supervision indisponible");
    } else {
        EventLog::log(
            LOG_INFO,
            "Notification: gestionnaire pret enabled=%s configured=%s",
            g_config.enabled ? "yes" : "no",
            validServer(g_config.server) && validTopic(g_config.topic) ? "yes" : "no"
        );
    }
}

void NotificationManager::update() {
    begin();
}

NotificationConfig NotificationManager::config() {
    begin();
    portENTER_CRITICAL(&g_mux);
    NotificationConfig result = g_config;
    portEXIT_CRITICAL(&g_mux);
    return result;
}

bool NotificationManager::saveConfig(bool enabled,
                                     const char* server,
                                     const char* topic,
                                     const char* token,
                                     bool preserveTokenWhenEmpty) {
    begin();

    if (!validServer(server)) return false;
    if (enabled && !validTopic(topic)) return false;

    portENTER_CRITICAL(&g_mux);
    g_config.enabled = enabled;
    copyText(g_config.server, sizeof(g_config.server), server);
    copyText(g_config.topic, sizeof(g_config.topic), topic);
    if (!(preserveTokenWhenEmpty && (!token || token[0] == '\0'))) {
        copyText(g_config.token, sizeof(g_config.token), token);
    }
    portEXIT_CRITICAL(&g_mux);

    const bool saved = persistConfig();
    if (saved) {
        g_attempts = 0;
        g_nextAttemptMs = 0;
        copyText(g_lastResult, sizeof(g_lastResult), "config-saved");
        EventLog::log(
            LOG_INFO,
            "Notification: configuration enregistree enabled=%s topic_present=%s token_present=%s",
            enabled ? "yes" : "no",
            topic && topic[0] ? "yes" : "no",
            g_config.token[0] ? "yes" : "no"
        );
    }
    return saved;
}

bool NotificationManager::requestTest() {
    begin();
    if (!g_config.enabled || !validServer(g_config.server) || !validTopic(g_config.topic)) {
        return false;
    }
    g_testPending = true;
    g_attempts = 0;
    g_nextAttemptMs = 0;
    copyText(g_lastResult, sizeof(g_lastResult), "test-queued");
    return true;
}

NotificationStatus NotificationManager::status() {
    begin();
    NotificationStatus result;
    result.enabled = g_config.enabled;
    result.configured = validServer(g_config.server) && validTopic(g_config.topic);
    result.workerRunning = g_workerResult == WorkerResult::RUNNING;
    result.testPending = g_testPending;
    result.pendingMask = IncidentManager::storageSd().pendingNotifications;
    result.attempts = g_attempts;
    result.lastHttpCode = g_lastHttpCode;
    copyText(result.lastResult, sizeof(result.lastResult), g_lastResult);

    const uint32_t nowMs = millis();
    if (g_nextAttemptMs != 0U && !deadlineReached(nowMs, g_nextAttemptMs)) {
        result.nextAttemptInSec = (g_nextAttemptMs - nowMs + 999U) / 1000U;
    }
    return result;
}

void NotificationManager::loadConfig() {
    g_config = NotificationConfig{};

    Preferences preferences;
    if (!preferences.begin(NOTIFICATION_NVS_NAMESPACE, true)) return;
    const size_t length = preferences.getBytesLength(NOTIFICATION_NVS_KEY);
    if (length != sizeof(StoredNotificationConfig)) {
        preferences.end();
        return;
    }

    StoredNotificationConfig stored{};
    const size_t read = preferences.getBytes(
        NOTIFICATION_NVS_KEY,
        &stored,
        sizeof(stored)
    );
    preferences.end();

    if (read != sizeof(stored) || stored.schema != NOTIFICATION_SCHEMA) return;

    g_config.enabled = stored.enabled != 0U;
    copyText(g_config.server, sizeof(g_config.server), stored.server);
    copyText(g_config.topic, sizeof(g_config.topic), stored.topic);
    copyText(g_config.token, sizeof(g_config.token), stored.token);
}

bool NotificationManager::persistConfig() {
    StoredNotificationConfig stored{};
    stored.schema = NOTIFICATION_SCHEMA;
    stored.enabled = g_config.enabled ? 1U : 0U;
    copyText(stored.server, sizeof(stored.server), g_config.server);
    copyText(stored.topic, sizeof(stored.topic), g_config.topic);
    copyText(stored.token, sizeof(stored.token), g_config.token);

    Preferences preferences;
    if (!preferences.begin(NOTIFICATION_NVS_NAMESPACE, false)) return false;
    const size_t written = preferences.putBytes(
        NOTIFICATION_NVS_KEY,
        &stored,
        sizeof(stored)
    );
    preferences.end();
    return written == sizeof(stored);
}

void NotificationManager::supervisorTask(void*) {
    for (;;) {
        update();
        const uint32_t nowMs = millis();

        if (g_workerResult != WorkerResult::IDLE &&
            g_workerResult != WorkerResult::RUNNING) {
            processWorkerResult(nowMs);
        }

        if (g_workerResult == WorkerResult::IDLE &&
            g_config.enabled &&
            validServer(g_config.server) &&
            validTopic(g_config.topic) &&
            WiFi.status() == WL_CONNECTED &&
            (g_nextAttemptMs == 0U || deadlineReached(nowMs, g_nextAttemptMs))) {
            const WorkType work = nextWork();
            if (work != WorkType::NONE) startSender(work);
        }

        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_PERIOD_MS));
    }
}

void NotificationManager::senderTask(void*) {
    const bool success = sendCurrentWork();
    g_workerResult = success ? WorkerResult::SUCCESS : WorkerResult::FAILED;
    vTaskDelete(nullptr);
}

void NotificationManager::scheduleNextAttempt(uint32_t nowMs) {
    const size_t index = g_attempts < RETRY_DELAY_COUNT
        ? static_cast<size_t>(g_attempts)
        : RETRY_DELAY_COUNT - 1U;
    g_nextAttemptMs = nowMs + RETRY_DELAYS_MS[index];
}

void NotificationManager::startSender(WorkType type) {
    if (g_workerResult == WorkerResult::RUNNING) return;

    g_currentWork = type;
    g_workerResult = WorkerResult::RUNNING;
    g_attempts++;
    copyText(g_lastResult, sizeof(g_lastResult), "sending");

    EventLog::log(
        LOG_INFO,
        "Notification: envoi type=%s tentative=%lu",
        workCode(type),
        static_cast<unsigned long>(g_attempts)
    );

    const BaseType_t created = xTaskCreatePinnedToCore(
        senderTask,
        "notify-sender",
        SENDER_STACK,
        nullptr,
        TASK_PRIORITY,
        &g_senderTaskHandle,
        TASK_CORE
    );

    if (created != pdPASS) {
        g_senderTaskHandle = nullptr;
        g_workerResult = WorkerResult::START_FAILED;
    }
}

NotificationManager::WorkType NotificationManager::nextWork() {
    if (g_testPending) return WorkType::MANUAL_TEST;
    if (IncidentManager::storageSdNotificationPending(IncidentNotification::INITIAL)) {
        return WorkType::INCIDENT_INITIAL;
    }
    if (IncidentManager::storageSdNotificationPending(IncidentNotification::ESCALATION)) {
        return WorkType::INCIDENT_ESCALATION;
    }
    if (IncidentManager::storageSdNotificationPending(IncidentNotification::RECOVERY)) {
        return WorkType::INCIDENT_RECOVERY;
    }
    return WorkType::NONE;
}

void NotificationManager::processWorkerResult(uint32_t nowMs) {
    const WorkerResult result = g_workerResult;
    g_workerResult = WorkerResult::IDLE;
    g_senderTaskHandle = nullptr;

    if (result == WorkerResult::SUCCESS) {
        if (g_currentWork == WorkType::MANUAL_TEST) {
            g_testPending = false;
        } else {
            IncidentManager::markStorageSdNotificationDelivered(
                incidentNotificationFor(g_currentWork)
            );
        }
        copyText(g_lastResult, sizeof(g_lastResult), "delivered");
        EventLog::log(
            LOG_INFO,
            "Notification: livree type=%s http=%d",
            workCode(g_currentWork),
            g_lastHttpCode
        );
        g_attempts = 0;
        g_nextAttemptMs = 0;
    } else {
        copyText(
            g_lastResult,
            sizeof(g_lastResult),
            result == WorkerResult::START_FAILED ? "task-start-failed" : "delivery-failed"
        );
        scheduleNextAttempt(nowMs);
        EventLog::log(
            LOG_WARN,
            "Notification: echec type=%s http=%d prochain=%lus",
            workCode(g_currentWork),
            g_lastHttpCode,
            static_cast<unsigned long>(
                g_nextAttemptMs > nowMs ? (g_nextAttemptMs - nowMs) / 1000U : 0U
            )
        );
    }

    g_currentWork = WorkType::NONE;
}

bool NotificationManager::sendCurrentWork() {
    NotificationConfig localConfig;
    portENTER_CRITICAL(&g_mux);
    localConfig = g_config;
    portEXIT_CRITICAL(&g_mux);

    const PersistentIncidentSnapshot incident = IncidentManager::storageSd();

    String title;
    String message;
    const char* priority = "default";
    const char* tags = "droplet";

    switch (g_currentWork) {
        case WorkType::INCIDENT_INITIAL:
            title = "AquaLook - carte SD indisponible";
            message = "La perte de la carte SD a ete confirmee. L'interface LittleFS de secours reste active.";
            priority = "high";
            tags = "warning,floppy_disk";
            break;
        case WorkType::INCIDENT_ESCALATION:
            title = "AquaLook - recuperation SD en echec";
            message = "Les cinq tentatives rapides ont echoue. AquaLook poursuit une tentative toutes les 10 minutes.";
            priority = "urgent";
            tags = "rotating_light,floppy_disk";
            break;
        case WorkType::INCIDENT_RECOVERY:
            title = "AquaLook - carte SD recuperee";
            message = "La carte SD est de nouveau operationnelle. L'incident reste a acquitter dans l'interface locale.";
            priority = "default";
            tags = "white_check_mark,floppy_disk";
            break;
        case WorkType::MANUAL_TEST:
            title = "AquaLook - test notification";
            message = "Le canal ntfy est correctement configure et joignable.";
            priority = "default";
            tags = "test_tube,droplet";
            break;
        default:
            return false;
    }

    if (g_currentWork != WorkType::MANUAL_TEST) {
        message += " Occurrences: ";
        message += incident.occurrences;
        message += ". Cause: ";
        message += incident.lastReason;
        message += ".";
    }

    const String endpoint = trimTrailingSlash(localConfig.server) + "/" + localConfig.topic;

    WiFiClientSecure client;
    client.setCACert(ISRG_ROOT_X1);

    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(client, endpoint)) {
        g_lastHttpCode = -1;
        return false;
    }

    http.addHeader("Content-Type", "text/plain; charset=utf-8");
    http.addHeader("Title", title);
    http.addHeader("Priority", priority);
    http.addHeader("Tags", tags);
    if (localConfig.token[0] != '\0') {
        String authorization = "Bearer ";
        authorization += localConfig.token;
        http.addHeader("Authorization", authorization);
    }

    const int code = http.POST(reinterpret_cast<const uint8_t*>(message.c_str()), message.length());
    g_lastHttpCode = code;
    http.end();

    return code >= 200 && code < 300;
}

bool NotificationManager::validServer(const char* server) {
    if (!server) return false;
    const String value = server;
    return value.startsWith("https://") &&
           value.length() >= 12U &&
           value.length() < sizeof(NotificationConfig::server);
}

bool NotificationManager::validTopic(const char* topic) {
    if (!topic) return false;
    const size_t length = strlen(topic);
    if (length < 8U || length >= sizeof(NotificationConfig::topic)) return false;
    for (size_t index = 0; index < length; ++index) {
        const char c = topic[index];
        const bool valid =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_';
        if (!valid) return false;
    }
    return true;
}

const char* NotificationManager::workCode(WorkType type) {
    switch (type) {
        case WorkType::INCIDENT_INITIAL: return "sd-initial";
        case WorkType::INCIDENT_ESCALATION: return "sd-escalation";
        case WorkType::INCIDENT_RECOVERY: return "sd-recovery";
        case WorkType::MANUAL_TEST: return "manual-test";
        default: return "none";
    }
}

IncidentNotification NotificationManager::incidentNotificationFor(WorkType type) {
    switch (type) {
        case WorkType::INCIDENT_ESCALATION:
            return IncidentNotification::ESCALATION;
        case WorkType::INCIDENT_RECOVERY:
            return IncidentNotification::RECOVERY;
        default:
            return IncidentNotification::INITIAL;
    }
}
