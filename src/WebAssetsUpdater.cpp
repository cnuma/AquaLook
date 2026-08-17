#include "WebAssetsUpdater.h"

#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <cstring>

#include "EventLog.h"
#include "OtaTlsTrust.h"
#include "StorageManager.h"
#include <ArduinoJson.h>

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

// Meme liste que OtaDownloadTest::allowedGithubHost : les assets de release
// sont servis en redirigeant github.com vers l'un de ces hotes.
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

WebAssetVerifyResult downloadAndVerify(
    const String& url,
    uint32_t expectedSize,
    const char* expectedSha256Hex,
    uint8_t redirectCount,
    const WebAssetsUpdater::Sink& sink,
    // false : taille et empreinte inconnues d'avance (cas du manifeste, qui
    // EST la reference des empreintes). expectedSize sert alors de borne
    // maximale et non de valeur attendue. Le contenu reste valide ensuite par
    // analyse stricte, et chaque fichier qu'il decrit est verifie par SHA-256.
    bool verify
) {
    WebAssetVerifyResult result;
    HttpTarget target;
    if (!parseHttpsUrl(url, target) || !allowedGithubHost(target.host)) {
        copyText(result.detail, sizeof(result.detail), "unauthorized-url");
        return result;
    }

    WiFiClientSecure client;
    OtaTlsTrust::configure(client);
    client.setHandshakeTimeout(10U);
    client.setTimeout(RESPONSE_TIMEOUT_MS / 1000U);

    if (!client.connect(target.host.c_str(), HTTPS_PORT)) {
        copyText(result.detail, sizeof(result.detail), "tls-connect-failed");
        client.stop();
        return result;
    }

    client.printf("GET %s HTTP/1.1\r\n", target.path.c_str());
    client.printf("Host: %s\r\n", target.host.c_str());
    client.print(
        "User-Agent: AquaLook-WebAssetsUpdater/0.1\r\n"
        "Accept: application/octet-stream\r\n"
        "Connection: close\r\n\r\n"
    );

    const uint32_t headerDeadline = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - headerDeadline) < 0) {
        delay(10);
    }
    if (!client.available()) {
        copyText(result.detail, sizeof(result.detail), "http-timeout");
        client.stop();
        return result;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
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
            copyText(result.detail, sizeof(result.detail), "redirect-invalid");
            return result;
        }
        return downloadAndVerify(location, expectedSize, expectedSha256Hex,
                                 redirectCount + 1U, sink, verify);
    }

    if (statusCode != 200) {
        snprintf(result.detail, sizeof(result.detail), "http-%d", statusCode);
        client.stop();
        return result;
    }
    if (chunked) {
        copyText(result.detail, sizeof(result.detail), "chunked-not-supported");
        client.stop();
        return result;
    }
    if (contentLength < 0) {
        copyText(result.detail, sizeof(result.detail), "content-length-missing");
        client.stop();
        return result;
    }
    if (verify && static_cast<uint32_t>(contentLength) != expectedSize) {
        copyText(result.detail, sizeof(result.detail), "content-length-mismatch");
        client.stop();
        return result;
    }
    if (!verify && static_cast<uint32_t>(contentLength) > expectedSize) {
        copyText(result.detail, sizeof(result.detail), "too-large");
        client.stop();
        return result;
    }
    const uint32_t bytesToRead =
        verify ? expectedSize : static_cast<uint32_t>(contentLength);

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

    while (downloaded < bytesToRead) {
        const int available = client.available();
        if (available > 0) {
            const size_t remaining = bytesToRead - downloaded;
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
                if (sink && !sink(buffer, static_cast<size_t>(received))) {
                    copyText(result.detail, sizeof(result.detail), "sink-write-failed");
                    hashOk = false;
                    break;
                }
                downloaded += static_cast<uint32_t>(received);
                lastDataAt = millis();
            }
        } else {
            if (!client.connected()) {
                copyText(result.detail, sizeof(result.detail), "connection-closed");
                break;
            }
            if (millis() - lastDataAt > RESPONSE_TIMEOUT_MS) {
                copyText(result.detail, sizeof(result.detail), "body-timeout");
                break;
            }
            delay(1);
        }
    }

    result.downloadDurationMs = millis() - downloadStartedAt;
    result.downloadedSize = downloaded;

    unsigned char digest[32] = {};
    if (!hashOk || mbedtls_sha256_finish_ret(&shaContext, digest) != 0) {
        if (result.detail[0] == '\0') {
            copyText(result.detail, sizeof(result.detail), "sha256-update-failed");
        }
        mbedtls_sha256_free(&shaContext);
        client.stop();
        return result;
    }
    mbedtls_sha256_free(&shaContext);
    client.stop();
    digestToHex(digest, result.calculatedSha256);

    if (verify && result.downloadedSize != expectedSize) {
        if (result.detail[0] == '\0') {
            copyText(result.detail, sizeof(result.detail), "size-mismatch");
        }
        return result;
    }
    if (verify && strcmp(result.calculatedSha256, expectedSha256Hex) != 0) {
        copyText(result.detail, sizeof(result.detail), "sha256-mismatch");
        return result;
    }

    result.success = true;
    copyText(result.detail, sizeof(result.detail), "verified");
    EventLog::log(
        LOG_INFO,
        "WebAssets: verification OK bytes=%lu durationMs=%lu",
        static_cast<unsigned long>(result.downloadedSize),
        static_cast<unsigned long>(result.downloadDurationMs)
    );
    return result;
}
}

WebAssetVerifyResult WebAssetsUpdater::verifyOnly(
    const char* url,
    uint32_t expectedSize,
    const char* expectedSha256Hex
) {
    if (!url || url[0] == '\0' || expectedSize == 0U ||
        !expectedSha256Hex || strlen(expectedSha256Hex) != 64U) {
        WebAssetVerifyResult result;
        copyText(result.detail, sizeof(result.detail), "invalid-arguments");
        return result;
    }
    return downloadAndVerify(String(url), expectedSize, expectedSha256Hex, 0U,
                             WebAssetsUpdater::Sink(), true);
}

// Variante publique avec destination : meme mecanique, les octets verifies
// sont en plus transmis au sink au fil du telechargement.
WebAssetVerifyResult WebAssetsUpdater::downloadToSink(
    const char* url,
    uint32_t expectedSize,
    const char* expectedSha256Hex,
    const Sink& sink
) {
    if (!url || url[0] == '\0' || expectedSize == 0U ||
        !expectedSha256Hex || strlen(expectedSha256Hex) != 64U) {
        WebAssetVerifyResult result;
        copyText(result.detail, sizeof(result.detail), "invalid-arguments");
        return result;
    }
    return downloadAndVerify(String(url), expectedSize, expectedSha256Hex, 0U, sink, true);
}

// ── Detection d'une mise a jour ───────────────────────────────────────────

namespace {

// Lit la version actuellement deployee depuis /www/assets-version.json.
// Absent ou illisible => version inconnue, ce qui fait considerer toute
// version publiee comme une mise a jour : c'est le comportement voulu pour un
// module dont les ressources viennent d'une synchronisation manuelle.
void readInstalledVersion(StorageManager* storage, char* out, size_t outSize) {
    if (outSize == 0U) return;
    out[0] = '\0';
    if (!storage) return;

    FsFile f;
    if (!storage->openRead("/www/assets-version.json", f)) return;

    char buf[256];
    const int32_t n = storage->readChunk(f, reinterpret_cast<uint8_t*>(buf), sizeof(buf) - 1U);
    storage->closeFile(f);
    if (n <= 0) return;
    buf[n] = '\0';

    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok) return;

    // Le fichier ecrit par tools/sync-sd-assets.ps1 ne porte pas de version de
    // release mais un horodatage et un sha git. On accepte les deux formes :
    // "version" si le module l'a ecrit lui-meme, sinon "gitSha" a defaut.
    const char* v = doc["version"] | doc["gitSha"] | "";
    strncpy(out, v, outSize - 1U);
    out[outSize - 1U] = '\0';
}

}  // namespace

namespace {
// Recuperation bornee, sans empreinte attendue : reservee au manifeste.
WebAssetVerifyResult fetchUnverified(const char* url, uint32_t maxBytes,
                                     const WebAssetsUpdater::Sink& sink) {
    return downloadAndVerify(String(url), maxBytes, "", 0U, sink, false);
}
}  // namespace

WebAssetsUpdater::CheckResult WebAssetsUpdater::checkForUpdate(StorageManager* storage) {
    CheckResult result;

    // Le manifeste est petit et sa taille n'est pas connue d'avance : on le
    // recupere sans verification d'empreinte (il EST la reference des
    // empreintes). Son contenu est valide par analyse stricte ci-dessous, et
    // chaque fichier qu'il decrit sera lui verifie par SHA-256.
    String body;
    body.reserve(3072);
    bool overflow = false;

    const Sink collect = [&body, &overflow](const uint8_t* data, size_t len) -> bool {
        if (body.length() + len > MANIFEST_MAX_BYTES) { overflow = true; return false; }
        for (size_t i = 0; i < len; ++i) body += static_cast<char>(data[i]);
        return true;
    };

    // downloadToSink exige taille et empreinte attendues. Pour le manifeste on
    // ne les a pas : on passe par une recuperation directe, bornee en taille.
    WebAssetVerifyResult dl = fetchUnverified(MANIFEST_URL, MANIFEST_MAX_BYTES, collect);

    if (overflow) {
        copyText(result.detail, sizeof(result.detail), "manifest-too-large");
        return result;
    }
    if (!dl.success) {
        copyText(result.detail, sizeof(result.detail), dl.detail);
        return result;
    }

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        copyText(result.detail, sizeof(result.detail), "manifest-parse-failed");
        return result;
    }

    const char* schema = doc["schema"] | "";
    if (strcmp(schema, "aqualook-web-manifest-v1") != 0) {
        copyText(result.detail, sizeof(result.detail), "manifest-schema-unknown");
        return result;
    }

    const char* version = doc["release"]["version"] | "";
    JsonArray files = doc["files"].as<JsonArray>();
    if (version[0] == '\0' || files.isNull() || files.size() == 0U) {
        copyText(result.detail, sizeof(result.detail), "manifest-incomplete");
        return result;
    }

    strncpy(result.availableVersion, version, sizeof(result.availableVersion) - 1U);
    result.fileCount = static_cast<uint8_t>(files.size());
    readInstalledVersion(storage, result.installedVersion, sizeof(result.installedVersion));

    result.ok = true;
    result.updateAvailable =
        strcmp(result.availableVersion, result.installedVersion) != 0;
    copyText(result.detail, sizeof(result.detail),
             result.updateAvailable ? "update-available" : "up-to-date");

    EventLog::log(LOG_INFO,
                  "WebAssets: verification installee=%s disponible=%s fichiers=%u -> %s",
                  result.installedVersion[0] ? result.installedVersion : "inconnue",
                  result.availableVersion,
                  static_cast<unsigned>(result.fileCount),
                  result.detail);
    return result;
}

// ── Deploiement complet des ressources Web ────────────────────────────────
// Concu pour le mode maintenance : voir la note dans WebAssetsUpdater.h.
WebAssetsUpdater::DeployResult WebAssetsUpdater::deployFromManifest(
    StorageManager* storage) {
    DeployResult out;

    if (!storage || !storage->isCardMounted()) {
        copyText(out.detail, sizeof(out.detail), "carte-sd-indisponible");
        return out;
    }

    // Le manifeste est recupere une seule fois : il sert a la fois a comparer
    // les versions et a piloter le telechargement fichier par fichier.
    String body;
    body.reserve(3072);
    bool overflow = false;
    const Sink collect = [&body, &overflow](const uint8_t* data, size_t len) -> bool {
        if (body.length() + len > MANIFEST_MAX_BYTES) { overflow = true; return false; }
        for (size_t i = 0; i < len; ++i) body += static_cast<char>(data[i]);
        return true;
    };

    const WebAssetVerifyResult dl = fetchUnverified(MANIFEST_URL, MANIFEST_MAX_BYTES, collect);
    if (overflow || !dl.success) {
        copyText(out.detail, sizeof(out.detail),
                 overflow ? "manifeste-trop-gros" : dl.detail);
        return out;
    }

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        copyText(out.detail, sizeof(out.detail), "manifeste-illisible");
        return out;
    }
    if (strcmp(doc["schema"] | "", "aqualook-web-manifest-v1") != 0) {
        copyText(out.detail, sizeof(out.detail), "schema-inconnu");
        return out;
    }

    JsonArray files = doc["files"].as<JsonArray>();
    const char* version = doc["release"]["version"] | "";
    if (files.isNull() || files.size() == 0U || version[0] == '\0') {
        copyText(out.detail, sizeof(out.detail), "manifeste-incomplet");
        return out;
    }
    strncpy(out.version, version, sizeof(out.version) - 1U);
    out.fileCount = static_cast<uint8_t>(files.size());

    if (!storage->beginAssetStaging()) {
        copyText(out.detail, sizeof(out.detail), "transit-impossible");
        return out;
    }

    // Chaque fichier est ecrit dans le transit et verifie par SHA-256 pendant
    // son telechargement. Un seul echec annule tout : on ne bascule pas, et
    // /www reste intact.
    for (JsonObject f : files) {
        const char* name = f["name"] | "";
        const char* url = f["url"] | "";
        const char* sha = f["sha256"] | "";
        const uint32_t size = f["size"] | 0U;

        if (name[0] == '\0' || url[0] == '\0' || strlen(sha) != 64U || size == 0U) {
            copyText(out.detail, sizeof(out.detail), "entree-manifeste-invalide");
            return out;
        }
        // Un nom compose interdirait de garantir la destination : refus net.
        if (strchr(name, '/') || strstr(name, "..")) {
            copyText(out.detail, sizeof(out.detail), "nom-de-fichier-refuse");
            return out;
        }

        String dest = String(StorageManager::ASSETS_STAGING) + "/" + name;
        FsFile file;
        if (!storage->openWrite(dest.c_str(), file)) {
            copyText(out.detail, sizeof(out.detail), "ecriture-impossible");
            return out;
        }

        const Sink toCard = [storage, &file](const uint8_t* data, size_t len) -> bool {
            return storage->writeChunk(file, data, len) == static_cast<int32_t>(len);
        };
        const WebAssetVerifyResult r = downloadToSink(url, size, sha, toCard);
        storage->closeFile(file);

        if (!r.success) {
            EventLog::log(LOG_ERROR, "WebAssets: %s echoue (%s)", name, r.detail);
            copyText(out.detail, sizeof(out.detail), r.detail);
            return out;   // /www jamais touche : rien a defaire
        }
        out.filesDeployed++;
        EventLog::log(LOG_INFO, "WebAssets: %s verifie (%lu octets)",
                      name, static_cast<unsigned long>(r.downloadedSize));
    }

    if (!storage->commitAssetStaging()) {
        copyText(out.detail, sizeof(out.detail), "bascule-echouee");
        return out;
    }

    out.ok = true;
    copyText(out.detail, sizeof(out.detail), "deploye");
    EventLog::log(LOG_INFO,
                  "WebAssets: deploiement termine version=%s fichiers=%u",
                  out.version, static_cast<unsigned>(out.filesDeployed));
    return out;
}
