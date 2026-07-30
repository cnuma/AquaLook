#include "MaintenanceBoot.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <cstring>

#include "ConfigManager.h"
#include "EventLog.h"
#include "MaintenanceRequest.h"
#include "MaintenanceResult.h"
#include "OtaDownloadTest.h"
#include "OtaBuildIdentity.h"
#include "OtaTlsTrust.h"

namespace {
constexpr char HTTPS_HOST[] = "api.github.com";
constexpr uint16_t HTTPS_PORT = 443U;
constexpr uint32_t WIFI_TIMEOUT_MS = 30000UL;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 10000UL;
constexpr uint32_t RESTART_DELAY_MS = 1500UL;
constexpr size_t MANIFEST_MAX_BYTES = 8192U;
constexpr uint8_t MAX_REDIRECTS = 2U;

struct GithubProbeOutcome {
    bool success = false;
    uint32_t tlsDurationMs = 0U;
    char httpLine[96] = "";
    char detail[128] = "";
};

struct ManifestFetchOutcome {
    bool success = false;
    uint32_t tlsDurationMs = 0U;
    uint32_t bodySize = 0U;
    char httpLine[96] = "";
    char detail[128] = "";
    String body;
};

void copyText(char* destination, size_t destinationSize, const char* source) {
    if (destinationSize == 0U) return;
    std::strncpy(destination, source ? source : "", destinationSize - 1U);
    destination[destinationSize - 1U] = '\0';
}

void logMemory(const char* stage) {
    EventLog::log(
        LOG_INFO,
        "Maintenance: memory stage=%s heapFree=%lu heapMin=%lu largestBlock=%lu",
        stage,
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMinFreeHeap()),
        static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT))
    );
}

bool connectWifi(const ConfigManager& configManager, char* detail, size_t detailSize) {
    const char* ssid = configManager.wifi().ssid;
    const char* password = configManager.wifi().password;
    if (ssid == nullptr || ssid[0] == '\0') {
        copyText(detail, detailSize, "wifi-ssid-absent");
        EventLog::log(LOG_ERROR, "Maintenance: WiFi impossible, SSID absent");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid, password);

    const uint32_t startedAtMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAtMs < WIFI_TIMEOUT_MS) {
        delay(50);
    }

    if (WiFi.status() != WL_CONNECTED) {
        snprintf(detail, detailSize, "wifi-status-%d", static_cast<int>(WiFi.status()));
        EventLog::log(LOG_ERROR, "Maintenance: WiFi echec ssid='%s' status=%d", ssid,
                      static_cast<int>(WiFi.status()));
        WiFi.disconnect(true);
        return false;
    }

    EventLog::log(LOG_INFO, "Maintenance: WiFi connecte ip=%s rssi=%ddBm",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
}

bool parseHttpsUrl(const String& url, String& host, String& path) {
    if (!url.startsWith("https://")) return false;
    const int hostStart = 8;
    const int slash = url.indexOf('/', hostStart);
    if (slash < 0) {
        host = url.substring(hostStart);
        path = "/";
    } else {
        host = url.substring(hostStart, slash);
        path = url.substring(slash);
    }
    return host.length() > 0 && path.length() > 0;
}

bool allowedGithubHost(const String& host) {
    return host == "github.com" || host == "api.github.com" ||
           host == "objects.githubusercontent.com" ||
           host == "release-assets.githubusercontent.com";
}

int parseHttpCode(const String& statusLine) {
    if (!statusLine.startsWith("HTTP/1.")) return 0;
    const int firstSpace = statusLine.indexOf(' ');
    if (firstSpace < 0 || statusLine.length() < firstSpace + 4) return 0;
    return statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
}

GithubProbeOutcome probeGithub() {
    GithubProbeOutcome outcome;
    logMemory("before-client");

    WiFiClientSecure client;
    OtaTlsTrust::configure(client);
    client.setHandshakeTimeout(10U);
    client.setTimeout(RESPONSE_TIMEOUT_MS / 1000U);

    const uint32_t startedAtMs = millis();
    const bool connected = client.connect(HTTPS_HOST, HTTPS_PORT);
    outcome.tlsDurationMs = millis() - startedAtMs;
    EventLog::log(connected ? LOG_INFO : LOG_ERROR,
                  "Maintenance: GitHub TLS connected=%s durationMs=%lu",
                  connected ? "yes" : "no",
                  static_cast<unsigned long>(outcome.tlsDurationMs));

    if (!connected) {
        char errorBuffer[96] = "";
        const int errorCode = client.lastError(errorBuffer, sizeof(errorBuffer));
        snprintf(outcome.detail, sizeof(outcome.detail), "tls-error=%d %s", errorCode,
                 errorBuffer[0] ? errorBuffer : "unknown");
        client.stop();
        return outcome;
    }

    client.print(
        "HEAD /repos/cnuma/AquaLook/releases/latest HTTP/1.1\r\n"
        "Host: api.github.com\r\n"
        "User-Agent: AquaLook-Maintenance/0.3\r\n"
        "Accept: application/vnd.github+json\r\n"
        "Connection: close\r\n\r\n"
    );

    const uint32_t deadlineMs = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - deadlineMs) < 0) {
        delay(10);
    }

    if (!client.available()) {
        copyText(outcome.detail, sizeof(outcome.detail),
                 client.connected() ? "http-response-timeout" :
                                      "connection-closed-before-response");
        client.stop();
        return outcome;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    copyText(outcome.httpLine, sizeof(outcome.httpLine), statusLine.c_str());
    outcome.success = parseHttpCode(statusLine) >= 200 && parseHttpCode(statusLine) < 400;
    if (!outcome.success) copyText(outcome.detail, sizeof(outcome.detail), "unexpected-http-status");
    client.stop();
    delay(100);
    logMemory("after-close");
    return outcome;
}

ManifestFetchOutcome fetchManifestUrl(const String& url, uint8_t redirectCount) {
    ManifestFetchOutcome outcome;
    String host;
    String path;
    if (!parseHttpsUrl(url, host, path) || !allowedGithubHost(host)) {
        copyText(outcome.detail, sizeof(outcome.detail), "unauthorized-manifest-url");
        return outcome;
    }

    WiFiClientSecure client;
    OtaTlsTrust::configure(client);
    client.setHandshakeTimeout(10U);
    client.setTimeout(RESPONSE_TIMEOUT_MS / 1000U);

    const uint32_t startedAtMs = millis();
    if (!client.connect(host.c_str(), HTTPS_PORT)) {
        outcome.tlsDurationMs = millis() - startedAtMs;
        char errorBuffer[80] = "";
        const int errorCode = client.lastError(errorBuffer, sizeof(errorBuffer));
        snprintf(outcome.detail, sizeof(outcome.detail), "tls-error=%d", errorCode);
        client.stop();
        return outcome;
    }
    outcome.tlsDurationMs = millis() - startedAtMs;

    client.printf("GET %s HTTP/1.1\r\n", path.c_str());
    client.printf("Host: %s\r\n", host.c_str());
    client.print("User-Agent: AquaLook-Maintenance/0.3\r\n");
    client.print("Accept: application/json\r\nConnection: close\r\n\r\n");

    const uint32_t deadlineMs = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - deadlineMs) < 0) {
        delay(10);
    }
    if (!client.available()) {
        copyText(outcome.detail, sizeof(outcome.detail), "http-response-timeout");
        client.stop();
        return outcome;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    copyText(outcome.httpLine, sizeof(outcome.httpLine), statusLine.c_str());
    const int statusCode = parseHttpCode(statusLine);
    String location;
    int contentLength = -1;

    while (client.connected() || client.available()) {
        String header = client.readStringUntil('\n');
        header.trim();
        if (header.length() == 0) break;
        if (header.startsWith("Location:")) {
            location = header.substring(9);
            location.trim();
        } else if (header.startsWith("Content-Length:")) {
            String value = header.substring(15);
            value.trim();
            contentLength = value.toInt();
        }
    }

    if (statusCode >= 300 && statusCode < 400) {
        client.stop();
        if (redirectCount >= MAX_REDIRECTS || location.length() == 0) {
            copyText(outcome.detail, sizeof(outcome.detail), "redirect-limit-or-location-missing");
            return outcome;
        }
        ManifestFetchOutcome redirected = fetchManifestUrl(location, redirectCount + 1U);
        redirected.tlsDurationMs += outcome.tlsDurationMs;
        return redirected;
    }

    if (statusCode != 200) {
        snprintf(outcome.detail, sizeof(outcome.detail), "http-status-%d", statusCode);
        client.stop();
        return outcome;
    }
    if (contentLength > static_cast<int>(MANIFEST_MAX_BYTES)) {
        copyText(outcome.detail, sizeof(outcome.detail), "manifest-too-large");
        client.stop();
        return outcome;
    }

    outcome.body.reserve(contentLength > 0 ? static_cast<size_t>(contentLength) : 1024U);
    uint32_t lastDataAtMs = millis();
    while (client.connected() || client.available()) {
        while (client.available()) {
            outcome.body += static_cast<char>(client.read());
            lastDataAtMs = millis();
            if (outcome.body.length() > MANIFEST_MAX_BYTES) {
                copyText(outcome.detail, sizeof(outcome.detail), "manifest-too-large");
                client.stop();
                outcome.body = "";
                return outcome;
            }
        }
        if (millis() - lastDataAtMs > RESPONSE_TIMEOUT_MS) {
            copyText(outcome.detail, sizeof(outcome.detail), "manifest-body-timeout");
            client.stop();
            outcome.body = "";
            return outcome;
        }
        delay(1);
    }
    client.stop();

    outcome.bodySize = outcome.body.length();
    if (outcome.bodySize == 0U) {
        copyText(outcome.detail, sizeof(outcome.detail), "manifest-empty");
        return outcome;
    }
    outcome.success = true;
    return outcome;
}

bool validVersion(const char* value) {
    if (!value || !value[0]) return false;
    uint8_t dots = 0U;
    bool digitInPart = false;
    for (const char* p = value; *p; ++p) {
        if (*p >= '0' && *p <= '9') {
            digitInPart = true;
        } else if (*p == '.' && digitInPart && dots < 2U) {
            dots++;
            digitInPart = false;
        } else {
            return false;
        }
    }
    return dots == 2U && digitInPart;
}

int compareVersions(const char* left, const char* right) {
    const char* l = left;
    const char* r = right;
    for (uint8_t part = 0U; part < 3U; ++part) {
        const unsigned long lv = strtoul(l, nullptr, 10);
        const unsigned long rv = strtoul(r, nullptr, 10);
        if (lv < rv) return -1;
        if (lv > rv) return 1;
        l = strchr(l, '.');
        r = strchr(r, '.');
        if (part < 2U) {
            if (!l || !r) return 0;
            ++l;
            ++r;
        }
    }
    return 0;
}

bool validSha256(const char* value) {
    if (!value || strlen(value) != 64U) return false;
    for (size_t index = 0U; index < 64U; ++index) {
        const char c = value[index];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

bool validChannel(const char* channel) {
    return channel && (!strcmp(channel, "stable") || !strcmp(channel, "beta") ||
                       !strcmp(channel, "dev"));
}

bool validFirmwareUrl(const char* url) {
    if (!url) return false;
    String host;
    String path;
    return parseHttpsUrl(String(url), host, path) && allowedGithubHost(host);
}

MaintenanceResult validateManifest(const ManifestFetchOutcome& fetch) {
    MaintenanceResult result;
    result.valid = true;
    result.success = false;
    result.tlsDurationMs = fetch.tlsDurationMs;
    result.recordedUptimeMs = millis();
    result.minFreeHeap = ESP.getMinFreeHeap();
    result.manifestSize = fetch.bodySize;
    copyText(result.command, sizeof(result.command), "check_version");
    copyText(result.httpLine, sizeof(result.httpLine), fetch.httpLine);
    copyText(result.installedVersion, sizeof(result.installedVersion), OtaBuildIdentity::VERSION);
    copyText(result.target, sizeof(result.target), OtaBuildIdentity::OTA_TARGET);
    copyText(result.environment, sizeof(result.environment), OtaBuildIdentity::PLATFORMIO_ENVIRONMENT);
    copyText(result.board, sizeof(result.board), OtaBuildIdentity::BOARD);

    if (!fetch.success) {
        copyText(result.detail, sizeof(result.detail), fetch.detail);
        return result;
    }

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, fetch.body);
    if (error) {
        copyText(result.detail, sizeof(result.detail), "invalid-json");
        return result;
    }

    const char* schema = document["schema"] | "";
    const char* version = document["release"]["version"] | "";
    const char* channel = document["release"]["channel"] | "";
    if (strcmp(schema, "aqualook-ota-manifest-v1") != 0) {
        copyText(result.detail, sizeof(result.detail), "invalid-schema");
        return result;
    }
    if (!validVersion(version)) {
        copyText(result.detail, sizeof(result.detail), "invalid-version");
        return result;
    }
    if (!validChannel(channel)) {
        copyText(result.detail, sizeof(result.detail), "invalid-channel");
        return result;
    }

    JsonVariantConst target = document["targets"][OtaBuildIdentity::OTA_TARGET];
    if (target.isNull()) {
        copyText(result.detail, sizeof(result.detail), "target-missing");
        return result;
    }

    const char* board = target["board"] | "";
    const char* environment = target["environment"] | "";
    const char* firmwareUrl = target["firmwareUrl"] | "";
    const char* sha256 = target["sha256"] | "";
    const uint32_t firmwareSize = target["size"] | 0U;

    if (strcmp(board, OtaBuildIdentity::BOARD) != 0) {
        copyText(result.detail, sizeof(result.detail), "board-mismatch");
        return result;
    }
    if (strcmp(environment, OtaBuildIdentity::PLATFORMIO_ENVIRONMENT) != 0) {
        copyText(result.detail, sizeof(result.detail), "environment-mismatch");
        return result;
    }
    if (!validFirmwareUrl(firmwareUrl)) {
        copyText(result.detail, sizeof(result.detail), "invalid-firmware-url");
        return result;
    }
    if (!validSha256(sha256)) {
        copyText(result.detail, sizeof(result.detail), "invalid-sha256");
        return result;
    }

    const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (firmwareSize == 0U) {
        copyText(result.detail, sizeof(result.detail), "firmware-size-zero");
        return result;
    }
    if (!updatePartition || firmwareSize > updatePartition->size) {
        copyText(result.detail, sizeof(result.detail), "firmware-too-large");
        return result;
    }

    copyText(result.availableVersion, sizeof(result.availableVersion), version);
    copyText(result.channel, sizeof(result.channel), channel);
    copyText(result.firmwareUrl, sizeof(result.firmwareUrl), firmwareUrl);
    copyText(result.sha256, sizeof(result.sha256), sha256);
    result.firmwareSize = firmwareSize;
    result.updateAvailable = compareVersions(OtaBuildIdentity::VERSION, version) < 0;
    result.notificationPending = result.updateAvailable;
    result.success = true;
    copyText(result.detail, sizeof(result.detail),
             result.updateAvailable ? "update-available" :
             (compareVersions(OtaBuildIdentity::VERSION, version) == 0 ?
                 "version-identical" : "remote-version-older"));
    return result;
}

void persistProbeResult(const GithubProbeOutcome& outcome) {
    MaintenanceResult result;
    result.valid = true;
    result.success = outcome.success;
    result.tlsDurationMs = outcome.tlsDurationMs;
    result.recordedUptimeMs = millis();
    result.minFreeHeap = ESP.getMinFreeHeap();
    copyText(result.command, sizeof(result.command), "probe_github");
    copyText(result.httpLine, sizeof(result.httpLine), outcome.httpLine);
    copyText(result.detail, sizeof(result.detail), outcome.detail);
    if (!MaintenanceResultStore::save(result)) {
        EventLog::log(LOG_ERROR, "Maintenance: echec sauvegarde resultat NVS");
    }
}

void restartToNormal() {
    WiFi.disconnect(true);
    delay(RESTART_DELAY_MS);
    ESP.restart();
}
}

bool MaintenanceBoot::runIfRequested(ConfigManager& configManager) {
    const MaintenanceRequest request = MaintenanceRequestStore::load();
    if (request == MaintenanceRequest::NONE) return false;

    EventLog::log(LOG_WARN, "Maintenance: demande detectee type=%s",
                  MaintenanceRequestStore::name(request));
    if (!MaintenanceRequestStore::clear()) {
        EventLog::log(LOG_ERROR, "Maintenance: impossible d'effacer la demande NVS");
        return false;
    }

    if (request != MaintenanceRequest::PROBE_GITHUB &&
        request != MaintenanceRequest::CHECK_VERSION &&
        request != MaintenanceRequest::DOWNLOAD_UPDATE_TEST) {
        EventLog::log(LOG_WARN, "Maintenance: commande refusee type=%s implementation=absente",
                      MaintenanceRequestStore::name(request));
        return false;
    }

    EventLog::log(LOG_WARN, "Maintenance: mode minimal actif command=%s otaWrite=no",
                  MaintenanceRequestStore::name(request));
    logMemory("start");

    char wifiDetail[128] = "";
    if (!connectWifi(configManager, wifiDetail, sizeof(wifiDetail))) {
        MaintenanceResult result;
        result.valid = true;
        result.success = false;
        result.recordedUptimeMs = millis();
        result.minFreeHeap = ESP.getMinFreeHeap();
        copyText(result.command, sizeof(result.command), MaintenanceRequestStore::name(request));
        copyText(result.detail, sizeof(result.detail), wifiDetail);
        MaintenanceResultStore::save(result);
        restartToNormal();
        return true;
    }

    bool success = false;
    if (request == MaintenanceRequest::PROBE_GITHUB) {
        const GithubProbeOutcome outcome = probeGithub();
        persistProbeResult(outcome);
        success = outcome.success;
    } else if (request == MaintenanceRequest::CHECK_VERSION) {
        const String manifestUrl = String("https://") + OtaBuildIdentity::MANIFEST_HOST +
                                   OtaBuildIdentity::MANIFEST_PATH;
        const ManifestFetchOutcome fetch = fetchManifestUrl(manifestUrl, 0U);
        const MaintenanceResult result = validateManifest(fetch);
        success = result.success;
        if (!MaintenanceResultStore::save(result)) {
            EventLog::log(LOG_ERROR, "Maintenance: echec sauvegarde resultat CHECK_VERSION");
        }
        EventLog::log(result.success ? LOG_INFO : LOG_ERROR,
                      "Maintenance: CHECK_VERSION success=%s installed=%s available=%s update=%s detail=%s otaWrite=no",
                      result.success ? "yes" : "no", result.installedVersion,
                      result.availableVersion[0] ? result.availableVersion : "n/a",
                      result.updateAvailable ? "yes" : "no", result.detail);
    } else {
        const MaintenanceResult validatedManifest = MaintenanceResultStore::load();
        const MaintenanceResult result = OtaDownloadTest::run(validatedManifest);
        success = result.success;
        if (!MaintenanceResultStore::save(result)) {
            EventLog::log(LOG_ERROR,
                          "Maintenance: echec sauvegarde resultat DOWNLOAD_UPDATE_TEST");
        }
        EventLog::log(result.success ? LOG_INFO : LOG_ERROR,
                      "Maintenance: DOWNLOAD_UPDATE_TEST success=%s bytes=%lu expected=%lu detail=%s otaWrite=no",
                      result.success ? "yes" : "no",
                      static_cast<unsigned long>(result.downloadedSize),
                      static_cast<unsigned long>(result.firmwareSize), result.detail);
    }

    EventLog::log(success ? LOG_INFO : LOG_ERROR,
                  "Maintenance: resultat command=%s success=%s otaWrite=no",
                  MaintenanceRequestStore::name(request), success ? "yes" : "no");
    restartToNormal();
    return true;
}
