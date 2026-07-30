#include "OtaDownloadTest.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <cstring>

#include "EventLog.h"
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
    return target.host.length() > 0 && target.path.length() > 0;
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

void digestToHex(const unsigned char digest[32], char output[65]) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    for (size_t index = 0U; index < 32U; ++index) {
        output[index * 2U] = HEX_DIGITS[(digest[index] >> 4U) & 0x0FU];
        output[index * 2U + 1U] = HEX_DIGITS[digest[index] & 0x0FU];
    }
    output[64] = '\0';
}

MaintenanceResult baseResult(const MaintenanceResult& manifest) {
    MaintenanceResult result = manifest;
    result.valid = true;
    result.success = false;
    result.downloadedSize = 0U;
    result.downloadDurationMs = 0U;
    result.calculatedSha256[0] = '\0';
    copyText(result.command, sizeof(result.command), "download_update_test");
    result.recordedUptimeMs = millis();
    return result;
}

MaintenanceResult downloadUrl(const MaintenanceResult& manifest,
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
    // OTA-3.0 ne réalise aucune écriture flash. La validation CA/signature
    // reste obligatoire avant STAGE_UPDATE et INSTALL_UPDATE.
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
        "User-Agent: AquaLook-Maintenance/0.4\r\n"
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
        return downloadUrl(manifest, location, redirectCount + 1U, result.tlsDurationMs);
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

    mbedtls_sha256_context shaContext;
    mbedtls_sha256_init(&shaContext);
    if (mbedtls_sha256_starts_ret(&shaContext, 0) != 0) {
        copyText(result.detail, sizeof(result.detail), "sha256-init-failed");
        mbedtls_sha256_free(&shaContext);
        client.stop();
        return result;
    }

    uint8_t buffer[READ_BUFFER_SIZE];
    uint32_t downloaded = 0U;
    const uint32_t downloadStartedAt = millis();
    uint32_t lastDataAt = downloadStartedAt;
    bool hashOk = true;

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
                if (mbedtls_sha256_update_ret(
                        &shaContext,
                        buffer,
                        static_cast<size_t>(received)) != 0) {
                    hashOk = false;
                    break;
                }
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

    unsigned char digest[32] = {};
    if (!hashOk || mbedtls_sha256_finish_ret(&shaContext, digest) != 0) {
        copyText(result.detail, sizeof(result.detail), "sha256-update-failed");
        mbedtls_sha256_free(&shaContext);
        client.stop();
        return result;
    }
    mbedtls_sha256_free(&shaContext);
    client.stop();
    digestToHex(digest, result.calculatedSha256);

    if (result.downloadedSize != manifest.firmwareSize) {
        copyText(result.detail, sizeof(result.detail), "firmware-size-mismatch");
        return result;
    }
    if (strcmp(result.calculatedSha256, manifest.sha256) != 0) {
        copyText(result.detail, sizeof(result.detail), "firmware-sha256-mismatch");
        return result;
    }

    result.success = true;
    copyText(result.detail, sizeof(result.detail), "firmware-download-verified");
    EventLog::log(
        LOG_INFO,
        "Maintenance: DOWNLOAD_UPDATE_TEST bytes=%lu durationMs=%lu sha256=ok otaWrite=no",
        static_cast<unsigned long>(result.downloadedSize),
        static_cast<unsigned long>(result.downloadDurationMs)
    );
    return result;
}
}

MaintenanceResult OtaDownloadTest::run(const MaintenanceResult& validatedManifest) {
    MaintenanceResult result = baseResult(validatedManifest);
    if (!validatedManifest.valid ||
        !validatedManifest.success ||
        !validatedManifest.updateAvailable ||
        validatedManifest.firmwareUrl[0] == '\0' ||
        validatedManifest.firmwareSize == 0U ||
        strlen(validatedManifest.sha256) != 64U) {
        copyText(result.detail, sizeof(result.detail), "validated-manifest-required");
        return result;
    }
    return downloadUrl(validatedManifest, validatedManifest.firmwareUrl, 0U, 0U);
}
