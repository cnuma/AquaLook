#include "NotificationManager.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "EventLog.h"

namespace {
constexpr char NVS_NAMESPACE[] = "aq_notify";
constexpr char NVS_CONFIG_KEY[] = "config";
constexpr uint8_t CONFIG_SCHEMA = 1U;
constexpr uint32_t SUPERVISOR_PERIOD_MS = 1000U;
constexpr uint32_t HTTP_TIMEOUT_MS = 8000U;
constexpr uint32_t SUPERVISOR_STACK = 4096U;
constexpr uint32_t SENDER_STACK = 6144U;
constexpr UBaseType_t TASK_PRIORITY = 1U;
constexpr BaseType_t TASK_CORE = 0;
constexpr size_t SERVER_SIZE = 96U;
constexpr size_t TOPIC_SIZE = 96U;

const uint32_t RETRY_DELAYS_MS[] = {
    0U,
    60000U,
    300000U,
    900000U,
    3600000U,
    21600000U
};
constexpr size_t RETRY_DELAY_COUNT =
    sizeof(RETRY_DELAYS_MS) / sizeof(RETRY_DELAYS_MS[0]);

// Racine officielle ISRG Root X1. La validation TLS reste obligatoire.
const char ISRG_ROOT_X1[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

struct StoredConfig {
    uint8_t schema;
    uint8_t enabled;
    uint8_t reserved[2];
    char server[96];
    char topic[96];
    char token[160];
};

NotificationConfig g_config;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t g_supervisorHandle = nullptr;
TaskHandle_t g_senderHandle = nullptr;
volatile NotificationManager::WorkerResult g_result =
    NotificationManager::WorkerResult::IDLE;
volatile NotificationManager::WorkType g_work =
    NotificationManager::WorkType::NONE;
volatile bool g_testPending = false;
uint32_t g_attempts = 0U;
uint32_t g_nextAttemptMs = 0U;
int g_lastHttpCode = 0;
char g_lastResult[48] = "not-started";
bool g_started = false;

bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

void copyText(char* target, size_t size, const char* source) {
    if (!target || size == 0U) return;
    strlcpy(target, source ? source : "", size);
}

String withoutTrailingSlash(const char* value) {
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
        &g_supervisorHandle,
        TASK_CORE
    );

    if (created != pdPASS) {
        g_supervisorHandle = nullptr;
        EventLog::log(LOG_ERROR, "Notification: supervision indisponible");
        return;
    }

    EventLog::log(
        LOG_INFO,
        "Notification: pret enabled=%s configured=%s",
        g_config.enabled ? "yes" : "no",
        validServer(g_config.server) && validTopic(g_config.topic) ? "yes" : "no"
    );
}

NotificationConfig NotificationManager::config() {
    begin();
    portENTER_CRITICAL(&g_mux);
    const NotificationConfig result = g_config;
    portEXIT_CRITICAL(&g_mux);
    return result;
}

NotificationStatus NotificationManager::status() {
    begin();
    NotificationStatus value;
    value.configured = validServer(g_config.server) && validTopic(g_config.topic);
    value.enabled = g_config.enabled;
    value.workerRunning = g_result == WorkerResult::RUNNING;
    value.testPending = g_testPending;
    value.pendingMask = IncidentManager::storageSd().pendingNotifications;
    value.attempts = g_attempts;
    value.lastHttpCode = g_lastHttpCode;
    copyText(value.lastResult, sizeof(value.lastResult), g_lastResult);

    const uint32_t nowMs = millis();
    if (g_nextAttemptMs != 0U && !deadlineReached(nowMs, g_nextAttemptMs)) {
        value.nextAttemptInSec = (g_nextAttemptMs - nowMs + 999U) / 1000U;
    }
    return value;
}

bool NotificationManager::saveConfig(bool enabled,
                                     const char* server,
                                     const char* topic,
                                     const char* token,
                                     bool preserveTokenWhenEmpty) {
    begin();
    if (!validServer(server) || (enabled && !validTopic(topic))) return false;

    portENTER_CRITICAL(&g_mux);
    g_config.enabled = enabled;
    copyText(g_config.server, sizeof(g_config.server), server);
    copyText(g_config.topic, sizeof(g_config.topic), topic);
    if (!(preserveTokenWhenEmpty && (!token || token[0] == '\0'))) {
        copyText(g_config.token, sizeof(g_config.token), token);
    }
    portEXIT_CRITICAL(&g_mux);

    if (!persistConfig()) return false;

    g_attempts = 0U;
    g_nextAttemptMs = 0U;
    copyText(g_lastResult, sizeof(g_lastResult), "config-saved");
    EventLog::log(
        LOG_INFO,
        "Notification: configuration sauvee enabled=%s topic_present=%s token_present=%s",
        enabled ? "yes" : "no",
        topic && topic[0] ? "yes" : "no",
        g_config.token[0] ? "yes" : "no"
    );
    return true;
}

bool NotificationManager::requestTest() {
    begin();
    if (!g_config.enabled ||
        !validServer(g_config.server) ||
        !validTopic(g_config.topic)) {
        return false;
    }

    g_testPending = true;
    g_attempts = 0U;
    g_nextAttemptMs = 0U;
    copyText(g_lastResult, sizeof(g_lastResult), "test-queued");
    return true;
}

void NotificationManager::loadConfig() {
    g_config = NotificationConfig{};

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return;
    if (prefs.getBytesLength(NVS_CONFIG_KEY) != sizeof(StoredConfig)) {
        prefs.end();
        return;
    }

    StoredConfig stored{};
    const size_t read = prefs.getBytes(NVS_CONFIG_KEY, &stored, sizeof(stored));
    prefs.end();
    if (read != sizeof(stored) || stored.schema != CONFIG_SCHEMA) return;

    g_config.enabled = stored.enabled != 0U;
    copyText(g_config.server, sizeof(g_config.server), stored.server);
    copyText(g_config.topic, sizeof(g_config.topic), stored.topic);
    copyText(g_config.token, sizeof(g_config.token), stored.token);
}

bool NotificationManager::persistConfig() {
    StoredConfig stored{};
    stored.schema = CONFIG_SCHEMA;
    stored.enabled = g_config.enabled ? 1U : 0U;
    copyText(stored.server, sizeof(stored.server), g_config.server);
    copyText(stored.topic, sizeof(stored.topic), g_config.topic);
    copyText(stored.token, sizeof(stored.token), g_config.token);

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    const size_t written = prefs.putBytes(NVS_CONFIG_KEY, &stored, sizeof(stored));
    prefs.end();
    return written == sizeof(stored);
}

void NotificationManager::supervisorTask(void*) {
    for (;;) {
        const uint32_t nowMs = millis();

        if (g_result != WorkerResult::IDLE &&
            g_result != WorkerResult::RUNNING) {
            processWorkerResult(nowMs);
        }

        if (g_result == WorkerResult::IDLE &&
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
    g_result = sendCurrentWork() ? WorkerResult::SUCCESS : WorkerResult::FAILED;
    vTaskDelete(nullptr);
}

void NotificationManager::startSender(WorkType type) {
    if (g_result == WorkerResult::RUNNING) return;

    g_work = type;
    g_result = WorkerResult::RUNNING;
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
        &g_senderHandle,
        TASK_CORE
    );

    if (created != pdPASS) {
        g_senderHandle = nullptr;
        g_result = WorkerResult::START_FAILED;
    }
}

void NotificationManager::processWorkerResult(uint32_t nowMs) {
    const WorkerResult result = g_result;
    g_result = WorkerResult::IDLE;
    g_senderHandle = nullptr;

    if (result == WorkerResult::SUCCESS) {
        if (g_work == WorkType::MANUAL_TEST) {
            g_testPending = false;
        } else {
            IncidentManager::markStorageSdNotificationDelivered(
                incidentNotificationFor(g_work)
            );
        }

        copyText(g_lastResult, sizeof(g_lastResult), "delivered");
        EventLog::log(
            LOG_INFO,
            "Notification: livree type=%s http=%d",
            workCode(g_work),
            g_lastHttpCode
        );
        g_attempts = 0U;
        g_nextAttemptMs = 0U;
    } else {
        copyText(
            g_lastResult,
            sizeof(g_lastResult),
            result == WorkerResult::START_FAILED
                ? "task-start-failed"
                : "delivery-failed"
        );
        scheduleNextAttempt(nowMs);
        EventLog::log(
            LOG_WARN,
            "Notification: echec type=%s http=%d prochain=%lus",
            workCode(g_work),
            g_lastHttpCode,
            static_cast<unsigned long>(
                g_nextAttemptMs > nowMs ? (g_nextAttemptMs - nowMs) / 1000U : 0U
            )
        );
    }

    g_work = WorkType::NONE;
}

void NotificationManager::scheduleNextAttempt(uint32_t nowMs) {
    const size_t index = g_attempts < RETRY_DELAY_COUNT
        ? static_cast<size_t>(g_attempts)
        : RETRY_DELAY_COUNT - 1U;
    g_nextAttemptMs = nowMs + RETRY_DELAYS_MS[index];
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

bool NotificationManager::sendCurrentWork() {
    NotificationConfig configCopy;
    portENTER_CRITICAL(&g_mux);
    configCopy = g_config;
    portEXIT_CRITICAL(&g_mux);

    const PersistentIncidentSnapshot incident = IncidentManager::storageSd();
    String title;
    String message;
    const char* priority = "default";
    const char* tags = "droplet";

    switch (g_work) {
        case WorkType::INCIDENT_INITIAL:
            title = "AquaLook - carte SD indisponible";
            message = "La perte de la carte SD a ete confirmee. L'interface LittleFS de secours reste active.";
            priority = "high";
            tags = "warning,floppy_disk";
            break;
        case WorkType::INCIDENT_ESCALATION:
            title = "AquaLook - recuperation SD en echec";
            message = "Les cinq tentatives rapides ont echoue. Une reprise est tentee toutes les 10 minutes.";
            priority = "urgent";
            tags = "rotating_light,floppy_disk";
            break;
        case WorkType::INCIDENT_RECOVERY:
            title = "AquaLook - carte SD recuperee";
            message = "La carte SD est de nouveau operationnelle. L'incident reste a acquitter localement.";
            tags = "white_check_mark,floppy_disk";
            break;
        case WorkType::MANUAL_TEST:
            title = "AquaLook - test notification";
            message = "Le canal ntfy est correctement configure et joignable.";
            tags = "test_tube,droplet";
            break;
        default:
            return false;
    }

    if (g_work != WorkType::MANUAL_TEST) {
        message += " Occurrences: ";
        message += incident.occurrences;
        message += ". Cause: ";
        message += incident.lastReason;
        message += ".";
    }

    const String endpoint =
        withoutTrailingSlash(configCopy.server) + "/" + configCopy.topic;

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
    if (configCopy.token[0] != '\0') {
        String auth = "Bearer ";
        auth += configCopy.token;
        http.addHeader("Authorization", auth);
    }

    const int code = http.POST(
        reinterpret_cast<uint8_t*>(const_cast<char*>(message.c_str())),
        message.length()
    );
    g_lastHttpCode = code;
    http.end();
    return code >= 200 && code < 300;
}

bool NotificationManager::validServer(const char* server) {
    if (!server) return false;
    const String value = server;
    return value.startsWith("https://") &&
           value.length() >= 12U &&
           value.length() < SERVER_SIZE;
}

bool NotificationManager::validTopic(const char* topic) {
    if (!topic) return false;
    const size_t length = strlen(topic);
    if (length < 8U || length >= TOPIC_SIZE) return false;

    for (size_t index = 0U; index < length; ++index) {
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
