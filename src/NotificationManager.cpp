#include "NotificationManager.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <time.h>

#include "EventLog.h"
#include "ConfigManager.h"

namespace {
constexpr char NVS_NAMESPACE[] = "aq_notify";
constexpr char NVS_CONFIG_KEY[] = "config";
constexpr uint8_t CONFIG_SCHEMA = 1U;
constexpr uint32_t SUPERVISOR_PERIOD_MS = 1000U;
constexpr uint32_t NETWORK_TIMEOUT_MS = 8000U;
constexpr uint32_t SUPERVISOR_STACK = 4096U;
constexpr uint32_t SENDER_STACK = 4096U;
constexpr UBaseType_t TASK_PRIORITY = 1U;
constexpr BaseType_t TASK_CORE = 0;
constexpr size_t SERVER_SIZE = 96U;
constexpr size_t TOPIC_SIZE = 96U;
constexpr int ERROR_DNS = -1001;
constexpr int ERROR_TCP = -1003;
constexpr int ERROR_RESPONSE_TIMEOUT = -1004;
constexpr int ERROR_INVALID_RESPONSE = -1005;
constexpr uint8_t ZONE_EVENT_QUEUE_CAPACITY = 8U;

const uint32_t RETRY_DELAYS_MS[] = {
    0U, 60000U, 300000U, 900000U, 3600000U, 21600000U
};
constexpr size_t RETRY_DELAY_COUNT =
    sizeof(RETRY_DELAYS_MS) / sizeof(RETRY_DELAYS_MS[0]);

struct StoredConfig {
    uint8_t schema;
    uint8_t enabled;
    uint8_t reserved[2];
    char server[96];
    char topic[96];
    char token[160];
};

NotificationConfig g_config;
struct ZoneEvent {
    uint8_t zone = 0U;
    bool active = false;
    char name[24] = "";
};
ZoneEvent g_zoneEvents[ZONE_EVENT_QUEUE_CAPACITY];
uint8_t g_zoneEventHead = 0U;
uint8_t g_zoneEventCount = 0U;
ConfigManager* g_zoneConfig = nullptr;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t g_supervisorHandle = nullptr;
TaskHandle_t g_senderHandle = nullptr;
volatile NotificationManager::WorkerResult g_result =
    NotificationManager::WorkerResult::IDLE;
volatile NotificationManager::WorkType g_work =
    NotificationManager::WorkType::NONE;
NotificationManager::WorkType g_retryWork = NotificationManager::WorkType::NONE;
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

String extractHost(const char* server) {
    String value = server ? server : "";
    if (value.startsWith("http://")) value.remove(0, 7);
    const int slash = value.indexOf('/');
    if (slash >= 0) value.remove(slash);
    const int colon = value.indexOf(':');
    if (colon >= 0) value.remove(colon);
    return value;
}

String extractBasePath(const char* server) {
    String value = server ? server : "";
    if (value.startsWith("http://")) value.remove(0, 7);
    const int slash = value.indexOf('/');
    if (slash < 0) return "";
    String path = value.substring(slash);
    while (path.endsWith("/")) path.remove(path.length() - 1U);
    return path;
}

uint32_t currentEpoch() {
    const time_t now = time(nullptr);
    return now > 0 ? static_cast<uint32_t>(now) : 0U;
}

const char* transportReason(int code) {
    switch (code) {
        case ERROR_DNS: return "dns-failed";
        case ERROR_TCP: return "tcp-failed";
        case ERROR_RESPONSE_TIMEOUT: return "response-timeout";
        case ERROR_INVALID_RESPONSE: return "invalid-response";
        default: return code >= 200 ? "http-response" : "delivery-failed";
    }
}

int parseHttpStatus(const String& statusLine) {
    if (!statusLine.startsWith("HTTP/1.")) return ERROR_INVALID_RESPONSE;
    const int firstSpace = statusLine.indexOf(' ');
    if (firstSpace < 0 || statusLine.length() < static_cast<size_t>(firstSpace + 4)) {
        return ERROR_INVALID_RESPONSE;
    }
    return statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
}
}

void NotificationManager::begin() {
    if (g_started) return;
    g_started = true;
    loadConfig();

    const BaseType_t created = xTaskCreatePinnedToCore(
        supervisorTask, "notify-supervisor", SUPERVISOR_STACK, nullptr,
        TASK_PRIORITY, &g_supervisorHandle, TASK_CORE
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

void NotificationManager::bindConfig(ConfigManager* config) {
    g_zoneConfig = config;
}

void NotificationManager::update() {
    begin();
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
    value.pendingZoneEvents = g_zoneEventCount;
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

bool NotificationManager::enqueueZoneEvent(uint8_t zone, bool active) {
    if (!g_zoneConfig || zone >= g_zoneConfig->nbZones()) return false;
    const uint8_t required = active ? ZONE_NOTIFY_START : ZONE_NOTIFY_STOP;
    if ((g_zoneConfig->zoneNotificationMask(zone) & required) == 0U) return false;

    portENTER_CRITICAL(&g_mux);
    if (g_zoneEventCount >= ZONE_EVENT_QUEUE_CAPACITY) {
        portEXIT_CRITICAL(&g_mux);
        EventLog::log(LOG_WARN, "Notification: file zones pleine zone=%u", zone + 1U);
        return false;
    }
    const uint8_t index = (g_zoneEventHead + g_zoneEventCount) % ZONE_EVENT_QUEUE_CAPACITY;
    g_zoneEvents[index].zone = zone;
    g_zoneEvents[index].active = active;
    copyText(g_zoneEvents[index].name, sizeof(g_zoneEvents[index].name),
             g_zoneConfig->zone(zone).name);
    g_zoneEventCount++;
    portEXIT_CRITICAL(&g_mux);
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
            WiFi.status() == WL_CONNECTED) {
            const bool sdPending =
                IncidentManager::storageSdNotificationPending(IncidentNotification::INITIAL) ||
                IncidentManager::storageSdNotificationPending(IncidentNotification::ESCALATION) ||
                IncidentManager::storageSdNotificationPending(IncidentNotification::RECOVERY);
            const bool manualCanPreemptZoneRetry =
                g_testPending && g_retryWork == WorkType::ZONE_EVENT;
            if (!sdPending && !manualCanPreemptZoneRetry && g_nextAttemptMs != 0U &&
                !deadlineReached(nowMs, g_nextAttemptMs)) {
                vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_PERIOD_MS));
                continue;
            }
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
        senderTask, "notify-sender", SENDER_STACK, nullptr,
        TASK_PRIORITY, &g_senderHandle, TASK_CORE
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
        } else if (g_work == WorkType::ZONE_EVENT) {
            portENTER_CRITICAL(&g_mux);
            if (g_zoneEventCount > 0U) {
                g_zoneEventHead = (g_zoneEventHead + 1U) % ZONE_EVENT_QUEUE_CAPACITY;
                g_zoneEventCount--;
            }
            portEXIT_CRITICAL(&g_mux);
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
        g_retryWork = WorkType::NONE;
    } else {
        if (result == WorkerResult::START_FAILED) {
            copyText(g_lastResult, sizeof(g_lastResult), "task-start-failed");
        } else {
            copyText(g_lastResult, sizeof(g_lastResult), transportReason(g_lastHttpCode));
        }

        scheduleNextAttempt(nowMs);
        g_retryWork = g_work;
        EventLog::log(
            LOG_WARN,
            "Notification: echec type=%s code=%d reason=%s prochain=%lus",
            workCode(g_work),
            g_lastHttpCode,
            transportReason(g_lastHttpCode),
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
    if (IncidentManager::storageSdNotificationPending(IncidentNotification::INITIAL)) {
        return WorkType::INCIDENT_INITIAL;
    }
    if (IncidentManager::storageSdNotificationPending(IncidentNotification::ESCALATION)) {
        return WorkType::INCIDENT_ESCALATION;
    }
    if (IncidentManager::storageSdNotificationPending(IncidentNotification::RECOVERY)) {
        return WorkType::INCIDENT_RECOVERY;
    }
    if (g_testPending) return WorkType::MANUAL_TEST;
    if (g_zoneEventCount > 0U) return WorkType::ZONE_EVENT;
    return WorkType::NONE;
}

bool NotificationManager::sendCurrentWork() {
    NotificationConfig configCopy;
    portENTER_CRITICAL(&g_mux);
    configCopy = g_config;
    portEXIT_CRITICAL(&g_mux);

    const String host = extractHost(configCopy.server);
    IPAddress resolvedIp;
    const int dnsResult = WiFi.hostByName(host.c_str(), resolvedIp);
    const uint32_t epoch = currentEpoch();

    EventLog::log(
        LOG_INFO,
        "Notification: diagnostic host=%s dns=%s ip=%s heap=%lu maxblock=%lu epoch=%lu rssi=%ddBm stackFree=%u",
        host.c_str(),
        dnsResult == 1 ? "ok" : "failed",
        dnsResult == 1 ? resolvedIp.toString().c_str() : "-",
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMaxAllocHeap()),
        static_cast<unsigned long>(epoch),
        WiFi.RSSI(),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );

    if (dnsResult != 1) {
        g_lastHttpCode = ERROR_DNS;
        return false;
    }

    WiFiClient client;
    client.setTimeout(NETWORK_TIMEOUT_MS / 1000U);

    EventLog::log(
        LOG_INFO,
        "Notification: tcp preparation heap=%lu maxblock=%lu stackFree=%u",
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMaxAllocHeap()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );

    const bool tcpOk = client.connect(host.c_str(), 80);
    if (!tcpOk) {
        g_lastHttpCode = ERROR_TCP;
        EventLog::log(
            LOG_ERROR,
            "Notification: tcp host=%s port=80 status=failed heap=%lu maxblock=%lu epoch=%lu stackFree=%u",
            host.c_str(),
            static_cast<unsigned long>(ESP.getFreeHeap()),
            static_cast<unsigned long>(ESP.getMaxAllocHeap()),
            static_cast<unsigned long>(epoch),
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
        );
        client.stop();
        return false;
    }

    EventLog::log(
        LOG_INFO,
        "Notification: tcp host=%s port=80 status=ok heap=%lu maxblock=%lu stackFree=%u",
        host.c_str(),
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMaxAllocHeap()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );

    const PersistentIncidentSnapshot incident = IncidentManager::storageSd();
    ZoneEvent zoneEvent;
    if (g_work == WorkType::ZONE_EVENT) {
        portENTER_CRITICAL(&g_mux);
        if (g_zoneEventCount == 0U) {
            portEXIT_CRITICAL(&g_mux);
            client.stop();
            return false;
        }
        zoneEvent = g_zoneEvents[g_zoneEventHead];
        portEXIT_CRITICAL(&g_mux);
    }
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
        case WorkType::ZONE_EVENT:
            title = zoneEvent.active
                ? "AquaLook - début d’arrosage"
                : "AquaLook - fin d’arrosage";
            message = "Zone ";
            message += zoneEvent.name;
            message += zoneEvent.active ? " démarrée" : " arrêtée";
            break;
        default:
            client.stop();
            return false;
    }

    if (g_work != WorkType::MANUAL_TEST && g_work != WorkType::ZONE_EVENT) {
        message += " Occurrences: ";
        message += incident.occurrences;
        message += ". Cause: ";
        message += incident.lastReason;
        message += ".";
    }

    String path = extractBasePath(configCopy.server);
    if (!path.startsWith("/")) path = "/" + path;
    if (path == "/") path = "";
    path += "/";
    path += configCopy.topic;

    client.print("POST ");
    client.print(path);
    client.print(" HTTP/1.1\r\nHost: ");
    client.print(host);
    client.print("\r\nUser-Agent: AquaLook/5.8\r\nContent-Type: text/plain; charset=utf-8\r\nTitle: ");
    client.print(title);
    client.print("\r\nPriority: ");
    client.print(priority);
    client.print("\r\nTags: ");
    client.print(tags);
    client.print("\r\n");
    if (configCopy.token[0] != '\0') {
        client.print("Authorization: Bearer ");
        client.print(configCopy.token);
        client.print("\r\n");
    }
    client.print("Content-Length: ");
    client.print(message.length());
    client.print("\r\nConnection: close\r\n\r\n");
    client.print(message);

    const uint32_t responseDeadline = millis() + NETWORK_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           !deadlineReached(millis(), responseDeadline)) {
        vTaskDelay(pdMS_TO_TICKS(10U));
    }

    if (!client.available()) {
        g_lastHttpCode = ERROR_RESPONSE_TIMEOUT;
        EventLog::log(
            LOG_ERROR,
            "Notification: reponse absente connected=%s heap=%lu",
            client.connected() ? "yes" : "no",
            static_cast<unsigned long>(ESP.getFreeHeap())
        );
        client.stop();
        return false;
    }

    const String statusLine = client.readStringUntil('\n');
    const int statusCode = parseHttpStatus(statusLine);
    g_lastHttpCode = statusCode;

    EventLog::log(
        statusCode >= 200 && statusCode < 300 ? LOG_INFO : LOG_ERROR,
        "Notification: reponse http=%d heap=%lu stackFree=%u",
        statusCode,
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );

    client.stop();
    return statusCode >= 200 && statusCode < 300;
}

bool NotificationManager::validServer(const char* server) {
    if (!server) return false;
    const String value = server;
    return value.startsWith("http://") &&
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
        case WorkType::ZONE_EVENT: return "zone-event";
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
