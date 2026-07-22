#include "MaintenanceBoot.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#include "ConfigManager.h"
#include "EventLog.h"
#include "MaintenanceRequest.h"

namespace {
constexpr char HTTPS_HOST[] = "api.github.com";
constexpr uint16_t HTTPS_PORT = 443U;
constexpr uint32_t WIFI_TIMEOUT_MS = 30000UL;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 10000UL;
constexpr uint32_t RESTART_DELAY_MS = 1500UL;

void logMemory(const char* stage) {
    EventLog::log(
        LOG_INFO,
        "Maintenance: memory stage=%s heapFree=%lu heapMin=%lu largestBlock=%lu",
        stage,
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMinFreeHeap()),
        static_cast<unsigned long>(
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)
        )
    );
}

bool connectWifi(const ConfigManager& configManager) {
    const char* ssid = configManager.wifi().ssid;
    const char* password = configManager.wifi().password;
    if (ssid == nullptr || ssid[0] == '\0') {
        EventLog::log(LOG_ERROR, "Maintenance: WiFi impossible, SSID absent");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid, password);

    const uint32_t startedAtMs = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startedAtMs < WIFI_TIMEOUT_MS) {
        delay(50);
    }

    if (WiFi.status() != WL_CONNECTED) {
        EventLog::log(
            LOG_ERROR,
            "Maintenance: WiFi echec ssid='%s' status=%d",
            ssid,
            static_cast<int>(WiFi.status())
        );
        WiFi.disconnect(true);
        return false;
    }

    EventLog::log(
        LOG_INFO,
        "Maintenance: WiFi connecte ip=%s rssi=%ddBm",
        WiFi.localIP().toString().c_str(),
        WiFi.RSSI()
    );
    return true;
}

bool probeGithub() {
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
        "Maintenance: GitHub TLS connected=%s durationMs=%lu",
        connected ? "yes" : "no",
        static_cast<unsigned long>(durationMs)
    );
    logMemory("after-connect");

    if (!connected) {
        char errorBuffer[128] = "";
        const int errorCode = client.lastError(errorBuffer, sizeof(errorBuffer));
        EventLog::log(
            LOG_ERROR,
            "Maintenance: GitHub TLS echec error=%d detail=%s",
            errorCode,
            errorBuffer[0] ? errorBuffer : "unknown"
        );
        client.stop();
        return false;
    }

    client.print(
        "HEAD /repos/cnuma/AquaLook/releases/latest HTTP/1.1\r\n"
        "Host: api.github.com\r\n"
        "User-Agent: AquaLook-Maintenance/0.1\r\n"
        "Accept: application/vnd.github+json\r\n"
        "Connection: close\r\n\r\n"
    );

    const uint32_t deadlineMs = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - deadlineMs) < 0) {
        delay(10);
    }

    if (!client.available()) {
        EventLog::log(
            LOG_ERROR,
            "Maintenance: GitHub reponse absente connected=%s",
            client.connected() ? "yes" : "no"
        );
        client.stop();
        return false;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    EventLog::log(LOG_INFO, "Maintenance: GitHub status=%s", statusLine.c_str());

    client.stop();
    delay(100);
    logMemory("after-close");
    return true;
}

void restartToNormal() {
    WiFi.disconnect(true);
    delay(RESTART_DELAY_MS);
    ESP.restart();
}
}

bool MaintenanceBoot::runIfRequested(ConfigManager& configManager) {
    const MaintenanceRequest request = MaintenanceRequestStore::load();
    if (request == MaintenanceRequest::NONE) {
        return false;
    }

    EventLog::log(
        LOG_WARN,
        "Maintenance: demande detectee type=%s",
        MaintenanceRequestStore::name(request)
    );

    // Effacement avant exécution : aucune commande ne peut créer une boucle
    // de redémarrage persistante en cas de panne réseau ou TLS.
    if (!MaintenanceRequestStore::clear()) {
        EventLog::log(LOG_ERROR, "Maintenance: impossible d'effacer la demande NVS");
        return false;
    }

    if (request != MaintenanceRequest::PROBE_GITHUB) {
        EventLog::log(
            LOG_WARN,
            "Maintenance: commande refusee type=%s implementation=absente",
            MaintenanceRequestStore::name(request)
        );
        return false;
    }

    EventLog::log(
        LOG_WARN,
        "Maintenance: mode minimal actif command=probe_github otaWrite=no"
    );
    logMemory("start");

    const bool wifiOk = connectWifi(configManager);
    const bool githubOk = wifiOk && probeGithub();

    EventLog::log(
        githubOk ? LOG_INFO : LOG_ERROR,
        "Maintenance: resultat command=probe_github success=%s otaWrite=no",
        githubOk ? "yes" : "no"
    );

    restartToNormal();
    return true;
}
