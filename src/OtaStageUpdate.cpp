#include "OtaStageUpdate.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <cstring>

#include "EventLog.h"
#include "OtaPartitionWriter.h"
#include "OtaTlsTrust.h"

namespace {
constexpr uint16_t HTTPS_PORT = 443U;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 15000UL;
constexpr uint8_t MAX_REDIRECTS = 3U;
constexpr size_t READ_BUFFER_SIZE = 1024U;

struct HttpTarget {
    String host;
    String path;
};

void copyText(char* destination, size_t destinationSize, const char* source) {
    if (destinationSize == 0U) return;
    std::strncpy(destination, source ? source : "", destinationSize - 1U);
    destination[destinationSize - 1U] = '\0';
}

bool parseHttpsUrl(const String& url, HttpTarget& target) {
    if (!url.startsWith("https://")) return false;
    const int hostStart = 8;
    const int slash = url.indexOf('/', hostStart);
    target.host = slash < 0 ? url.substring(hostStart) : url.substring(hostStart, slash);
    target.path = slash < 0 ? "/" : url.substring(slash);
    return target.host.length() > 0U && target.path.length() > 0U;
}

bool allowedGithubHost(const String& host) {
    return host == "github.com" ||
           host == "api.github.com" ||
           host == "objects.githubusercontent.com" ||
           host == "release-assets.githubusercontent.com";
}

int parseHttpCode(const String& statusLine) {
    if (!statusLine.startsWith("HTTP/1.")) return 0;
    const int firstSpace = statusLine.indexOf(' ');
    if (firstSpace < 0 || statusLine.length() < firstSpace + 4) return 0;
    return statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
}

MaintenanceResult baseResult(const MaintenanceResult& manifest) {
    MaintenanceResult result = manifest;
    result.valid = true;
    result.success = false;
    result.downloadedSize = 0U;
    result.downloadDurationMs = 0U;
    result.calculatedSha256[0] = '\0';
    copyText(result.command, sizeof(result.command), "stage_update_test");
    result.recordedUptimeMs = millis();
    return result;
}

MaintenanceResult stageUrl(const MaintenanceResult& manifest,
                           const String& url,
                           uint8_t redirectCount,
                           uint32_t accumulatedTlsMs) {
    MaintenanceResult result = baseResult(manifest);
    HttpTarget target;
    if (!parseHttpsUrl(url, target) || !allowedGithubHost(target.host)) {
        copyText(result.detail, sizeof(result.detail), "unauthorized-firmware-url");
        return result;
    }

    WiFiClientSecure client;
    OtaTlsTrust::configure(client);
    client.setHandshakeTimeout(10U);
    client.setTimeout(RESPONSE_TIMEOUT_MS / 1000U);

    const uint32_t tlsStartedAt = millis();
    if (!client.connect(target.host.c_str(), HTTPS_PORT)) {
        result.tlsDurationMs = accumulatedTlsMs + (millis() - tlsStartedAt);
        copyText(result.detail, sizeof(result.detail), "firmware-tls-failed");
        client.stop();
        return result;
    }
    result.tlsDurationMs = accumulatedTlsMs + (millis() - tlsStartedAt);

    client.printf("GET %s HTTP/1.1\r\n", target.path.c_str());
    client.printf("Host: %s\r\n", target.host.c_str());
    client.print(
        "User-Agent: AquaLook-Maintenance/0.5\r\n"
        "Accept: application/octet-stream\r\n"
        "Connection: close\r\n\r\n"
    );

    const uint32_t headerDeadline = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - headerDeadline) < 0) {
        delay(10);
    }
    if (!client.available()) {
        copyText(result.detail, sizeof(result.detail), "firmware-http-timeout");
        client.stop();
        return result;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    copyText(result.httpLine, sizeof(result.httpLine), statusLine.c_str());
    const int statusCode = parseHttpCode(statusLine);
    String location;
    int64_t contentLength = -1;
    bool chunked = false;

    while (client.connected() || client.available()) {
        String header = client.readStringUntil('\n');
        header.trim();
        if (header.length() == 0U) break;
        if (header.startsWith("Location:")) {
            location = header.substring(9);
            location.trim();
        } else if (header.startsWith("Content-Length:")) {
            String value = header.substring(15);
            value.trim();
            contentLength = value.toInt();
        } else if (header.startsWith("Transfer-Encoding:") &&
                   header.indexOf("chunked") >= 0) {
            chunked = true;
        }
    }

    if (statusCode >= 300 && statusCode < 400) {
        client.stop();
        if (redirectCount >= MAX_REDIRECTS || location.length() == 0U) {
            copyText(result.detail, sizeof(result.detail), "firmware-redirect-invalid");
            return result;
        }
        return stageUrl(manifest, location, redirectCount + 1U, result.tlsDurationMs);
    }

    if (statusCode != 200) {
        snprintf(result.detail, sizeof(result.detail), "firmware-http-%d", statusCode);
        client.stop();
        return result;
    }
    if (chunked) {
        copyText(result.detail, sizeof(result.detail), "chunked-transfer-not-supported");
        client.stop();
        return result;
    }
    if (contentLength < 0) {
        copyText(result.detail, sizeof(result.detail), "content-length-missing");
        client.stop();
        return result;
    }
    if (static_cast<uint32_t>(contentLength) != manifest.firmwareSize) {
        copyText(result.detail, sizeof(result.detail), "content-length-mismatch");
        client.stop();
        return result;
    }

    OtaPartitionWriter writer;
    if (!writer.begin(manifest.firmwareSize)) {
        snprintf(result.detail, sizeof(result.detail), "stage-%s",
                 OtaPartitionWriter::errorName(writer.result().error));
        client.stop();
        return result;
    }

    uint8_t buffer[READ_BUFFER_SIZE];
    uint32_t downloaded = 0U;
    const uint32_t downloadStartedAt = millis();
    uint32_t lastDataAt = downloadStartedAt;

    while (downloaded < manifest.firmwareSize) {
        const int available = client.available();
        if (available > 0) {
            const size_t remaining = manifest.firmwareSize - downloaded;
            const size_t wanted = min(
                static_cast<size_t>(available),
                min(sizeof(buffer), remaining)
            );
            const int received = client.read(buffer, wanted);
            if (received > 0) {
                if (!writer.write(buffer, static_cast<size_t>(received))) break;
                downloaded += static_cast<uint32_t>(received);
                lastDataAt = millis();
            }
        } else {
            if (!client.connected()) break;
            if (millis() - lastDataAt > RESPONSE_TIMEOUT_MS) {
                copyText(result.detail, sizeof(result.detail), "firmware-body-timeout");
                break;
            }
            delay(1);
        }
    }

    result.downloadDurationMs = millis() - downloadStartedAt;
    result.downloadedSize = downloaded;
    result.minFreeHeap = ESP.getMinFreeHeap();
    client.stop();

    if (result.downloadedSize != manifest.firmwareSize) {
        if (result.detail[0] == '\0') {
            copyText(result.detail, sizeof(result.detail), "firmware-size-mismatch");
        }
        writer.abort();
        return result;
    }

    const OtaPartitionWriter::Result stage = writer.finish(manifest.sha256);
    copyText(result.calculatedSha256, sizeof(result.calculatedSha256), stage.calculatedSha256);
    if (!stage.success) {
        snprintf(result.detail, sizeof(result.detail), "stage-%s",
                 OtaPartitionWriter::errorName(stage.error));
        return result;
    }

    result.success = true;
    snprintf(result.detail, sizeof(result.detail),
             "firmware-staged-inactive-%s", stage.partitionLabel);
    EventLog::log(
        LOG_INFO,
        "Maintenance: STAGE_UPDATE_TEST bytes=%lu durationMs=%lu sha256=ok partition=%s address=0x%06lX otaActivate=no",
        static_cast<unsigned long>(result.downloadedSize),
        static_cast<unsigned long>(result.downloadDurationMs),
        stage.partitionLabel,
        static_cast<unsigned long>(stage.partitionAddress)
    );
    return result;
}
}

MaintenanceResult OtaStageUpdate::run(const MaintenanceResult& validatedManifest) {
    MaintenanceResult result = baseResult(validatedManifest);
    if (!validatedManifest.valid ||
        !validatedManifest.success ||
        !validatedManifest.updateAvailable ||
        validatedManifest.firmwareUrl[0] == '\0' ||
        validatedManifest.firmwareSize == 0U ||
        std::strlen(validatedManifest.sha256) != 64U) {
        copyText(result.detail, sizeof(result.detail), "validated-manifest-required");
        return result;
    }
    return stageUrl(validatedManifest, validatedManifest.firmwareUrl, 0U, 0U);
}
