#include "OtaTlsProbe.h"

#if AQUALOOK_OTA_TLS_PROBE

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#include "EventLog.h"

namespace {
constexpr char HTTPS_HOST[] = "api.github.com";
constexpr uint16_t HTTPS_PORT = 443U;
constexpr uint32_t START_DELAY_MS = 5000UL;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 10000UL;
constexpr uint32_t TASK_STACK = 8192U;
constexpr UBaseType_t TASK_PRIORITY = 1U;
constexpr BaseType_t TASK_CORE = 0;

bool g_started = false;

void logMemory(const char* stage) {
    EventLog::log(
        LOG_INFO,
        "OTA-1.1: memory stage=%s heapFree=%lu heapMin=%lu largestBlock=%lu stackFree=%u",
        stage,
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMinFreeHeap()),
        static_cast<unsigned long>(
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)
        ),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );
}

void probeTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(START_DELAY_MS));

    EventLog::log(
        LOG_WARN,
        "OTA-1.1: probe start target=api.github.com method=HEAD insecure=yes otaWrite=no"
    );
    logMemory("before-client");

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(10U);
    client.setTimeout(RESPONSE_TIMEOUT_MS / 1000U);

    logMemory("before-connect");
    const uint32_t startedAtMs = millis();
    const bool connected = client.connect(HTTPS_HOST, HTTPS_PORT);
    const uint32_t durationMs = millis() - startedAtMs;

    EventLog::log(
        connected ? LOG_INFO : LOG_ERROR,
        "OTA-1.1: tls connected=%s durationMs=%lu",
        connected ? "yes" : "no",
        static_cast<unsigned long>(durationMs)
    );
    logMemory("after-connect");

    if (!connected) {
        char errorBuffer[128] = "";
        const int errorCode = client.lastError(errorBuffer, sizeof(errorBuffer));
        EventLog::log(
            LOG_ERROR,
            "OTA-1.1: result=failed phase=tls error=%d detail=%s",
            errorCode,
            errorBuffer[0] ? errorBuffer : "unknown"
        );
        client.stop();
        logMemory("after-failure");
        vTaskDelete(nullptr);
        return;
    }

    client.print(
        "HEAD /repos/cnuma/AquaLook/releases/latest HTTP/1.1\r\n"
        "Host: api.github.com\r\n"
        "User-Agent: AquaLook-OTA-Qualification/1.1\r\n"
        "Accept: application/vnd.github+json\r\n"
        "Connection: close\r\n\r\n"
    );

    const uint32_t deadlineMs = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - deadlineMs) < 0) {
        vTaskDelay(pdMS_TO_TICKS(10U));
    }

    if (!client.available()) {
        EventLog::log(
            LOG_ERROR,
            "OTA-1.1: result=failed phase=response connected=%s",
            client.connected() ? "yes" : "no"
        );
        client.stop();
        logMemory("after-timeout");
        vTaskDelete(nullptr);
        return;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    EventLog::log(LOG_INFO, "OTA-1.1: http statusLine=%s", statusLine.c_str());

    while (client.available()) client.read();
    client.stop();
    vTaskDelay(pdMS_TO_TICKS(100U));

    logMemory("after-close");
    EventLog::log(
        LOG_INFO,
        "OTA-1.1: result=success tls=yes request=yes otaWrite=no"
    );
    vTaskDelete(nullptr);
}
}  // namespace

void OtaTlsProbe::onWifiConnected() {
    if (g_started) return;
    g_started = true;

    const BaseType_t created = xTaskCreatePinnedToCore(
        probeTask,
        "ota-tls-probe",
        TASK_STACK,
        nullptr,
        TASK_PRIORITY,
        nullptr,
        TASK_CORE
    );

    if (created != pdPASS) {
        EventLog::log(LOG_ERROR, "OTA-1.1: task creation failed");
    }
}

#else

void OtaTlsProbe::onWifiConnected() {}

#endif
