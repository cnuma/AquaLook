#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#include "ConfigManager.h"
#include "WiFiManager.h"
#include "FaultManager.h"
#include "EventBus.h"

// Définitions minimales nécessaires au banc OTA-1 isolé.
// Ce fichier est exclu de tous les profils nominaux.
uint32_t FaultManager::_activeMask = 0U;
bool FaultManager::_unacknowledged = false;
bool FaultManager::_started = false;

void FaultManager::begin() {
    _started = true;
    _activeMask = 0U;
    _unacknowledged = false;
}

void FaultManager::update() {}

void FaultManager::setActive(FaultId id, bool active) {
    const uint32_t mask = 1UL << static_cast<uint8_t>(id);
    if (active) {
        _activeMask |= mask;
    } else {
        _activeMask &= ~mask;
    }
}

void FaultManager::notifyError() {
    _unacknowledged = true;
}

void FaultManager::acknowledge() {
    _unacknowledged = false;
}

bool FaultManager::hasActiveFaults() {
    return _activeMask != 0U;
}

bool FaultManager::hasUnacknowledgedErrors() {
    return _unacknowledged;
}

bool FaultManager::isAcknowledged() {
    return !_unacknowledged;
}

uint32_t FaultManager::activeMask() {
    return _activeMask;
}

void FaultManager::resolveColor(uint8_t normalRed,
                                uint8_t normalGreen,
                                uint8_t normalBlue,
                                uint8_t& outRed,
                                uint8_t& outGreen,
                                uint8_t& outBlue) {
    outRed = normalRed;
    outGreen = normalGreen;
    outBlue = normalBlue;
}

bool EventBus::displayDirty = false;
bool EventBus::configDirty = false;
bool EventBus::wifiDirty = false;
bool EventBus::captiveRequested = false;

namespace {
constexpr char HTTPS_HOST[] = "api.github.com";
constexpr uint16_t HTTPS_PORT = 443U;
constexpr uint32_t WIFI_TIMEOUT_MS = 60000UL;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 10000UL;

ConfigManager configManager;
WiFiManager wifiManager;
bool probeDone = false;
uint32_t wifiStartedAtMs = 0U;

void logMemory(const char* stage) {
    Serial.printf(
        "[OTA-1] memory stage=%s heapFree=%lu heapMin=%lu largestBlock=%lu stackFree=%u\n",
        stage,
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMinFreeHeap()),
        static_cast<unsigned long>(
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)
        ),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );
}

void runHttpsProbe() {
    probeDone = true;
    Serial.println("[OTA-1] probe=start target=api.github.com method=HEAD insecure=yes");
    Serial.println("[OTA-1] warning=certificate_validation_disabled_for_transport_qualification_only");
    logMemory("before-client");

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(10U);
    client.setTimeout(RESPONSE_TIMEOUT_MS / 1000U);

    logMemory("before-connect");
    const uint32_t startedAtMs = millis();
    const bool connected = client.connect(HTTPS_HOST, HTTPS_PORT);
    const uint32_t connectDurationMs = millis() - startedAtMs;

    Serial.printf(
        "[OTA-1] tls connected=%s durationMs=%lu\n",
        connected ? "yes" : "no",
        static_cast<unsigned long>(connectDurationMs)
    );
    logMemory("after-connect");

    if (!connected) {
        char errorBuffer[128] = "";
        const int errorCode = client.lastError(errorBuffer, sizeof(errorBuffer));
        Serial.printf(
            "[OTA-1] result=failed phase=tls error=%d detail=%s\n",
            errorCode,
            errorBuffer[0] ? errorBuffer : "unknown"
        );
        client.stop();
        logMemory("after-failure");
        return;
    }

    client.print(
        "HEAD /repos/cnuma/AquaLook/releases/latest HTTP/1.1\r\n"
        "Host: api.github.com\r\n"
        "User-Agent: AquaLook-OTA-Qualification/1.0\r\n"
        "Accept: application/vnd.github+json\r\n"
        "Connection: close\r\n\r\n"
    );

    const uint32_t deadlineMs = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - deadlineMs) < 0) {
        delay(10);
    }

    if (!client.available()) {
        Serial.printf(
            "[OTA-1] result=failed phase=response connected=%s\n",
            client.connected() ? "yes" : "no"
        );
        client.stop();
        logMemory("after-timeout");
        return;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    Serial.printf("[OTA-1] http statusLine=%s\n", statusLine.c_str());

    while (client.available()) {
        client.read();
    }
    client.stop();
    delay(100);

    logMemory("after-close");
    Serial.println("[OTA-1] result=success tls=yes request=yes otaWrite=no");
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("[OTA-1] isolated HTTPS qualification firmware");
    Serial.println("[OTA-1] irrigation_runtime=disabled ota_write=disabled");

    configManager.begin();
    wifiManager.begin(
        configManager.wifi().ssid,
        configManager.wifi().password
    );
    wifiStartedAtMs = millis();
    logMemory("boot");
}

void loop() {
    wifiManager.update();

    if (!probeDone && wifiManager.isConnected()) {
        Serial.printf(
            "[OTA-1] wifi=connected ip=%s rssi=%d\n",
            wifiManager.getIP().toString().c_str(),
            wifiManager.getRssi()
        );
        delay(500);
        runHttpsProbe();
    }

    if (!probeDone && millis() - wifiStartedAtMs >= WIFI_TIMEOUT_MS) {
        probeDone = true;
        Serial.println("[OTA-1] result=failed phase=wifi timeout=yes");
        logMemory("wifi-timeout");
    }

    delay(10);
}
