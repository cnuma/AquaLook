#include "SdStaticHandler.h"

#include <LittleFS.h>
#include <memory>

#include "EventLog.h"

namespace {

struct SdReadContext {
    FsFile file;
    StorageManager* storage = nullptr;
    String path;

    ~SdReadContext() {
        if (file.isOpen()) file.close();
    }
};

bool hasStaticExtension(const String& path) {
    return path.endsWith(".html") || path.endsWith(".css") ||
           path.endsWith(".js")   || path.endsWith(".json") ||
           path.endsWith(".png")  || path.endsWith(".jpg") ||
           path.endsWith(".jpeg") || path.endsWith(".gif") ||
           path.endsWith(".svg")  || path.endsWith(".ico") ||
           path.endsWith(".webp") || path.endsWith(".txt") ||
           path.endsWith(".xml")  || path.endsWith(".woff") ||
           path.endsWith(".woff2")|| path.endsWith(".ttf");
}

const char FALLBACK_LOGO[] PROGMEM = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" aria-label="AquaLook"><rect width="64" height="64" rx="12" fill="#0b1f2a"/><path d="M32 8C23 21 16 29 16 40a16 16 0 0 0 32 0C48 29 41 21 32 8Z" fill="#29b6f6"/><path d="M23 42c3 6 12 8 18 2" fill="none" stroke="#e8f7ff" stroke-width="4" stroke-linecap="round"/></svg>)svg";

}  // namespace

SdStaticHandler::SdStaticHandler(StorageManager* storage)
    : _storage(storage) {}

bool SdStaticHandler::canHandle(AsyncWebServerRequest* request) const {
    if (!_storage || !request) return false;
    if (request->method() != HTTP_GET && request->method() != HTTP_HEAD) return false;

    const String url = request->url();

    // Diagnostic toujours disponible, meme sans carte SD.
    if (url == "/api/storage") return true;

    String sdPath;
    if (!mapRequestPath(url, sdPath)) return false;

    if (_storage->isSdAvailable() && _storage->existsOnSd(sdPath.c_str())) {
        return true;
    }

    // Evite le 404 historique tout en laissant une future ressource LittleFS
    // prendre la main si elle est ajoutee plus tard.
    if (url == "/logo.png" && !LittleFS.exists("/logo.png")) return true;

    return false;
}

void SdStaticHandler::handleRequest(AsyncWebServerRequest* request) {
    if (!request || !_storage) return;

    if (request->url() == "/api/storage") {
        String body;
        body.reserve(320);
        body += F("{\"status\":\"");
        body += _storage->statusCode();
        body += F("\",\"message\":\"");
        body += _storage->statusMessage();
        body += F("\",\"sdAvailable\":");
        body += _storage->isSdAvailable() ? F("true") : F("false");
        body += F(",\"webAssetsAvailable\":");
        body += _storage->areWebAssetsAvailable() ? F("true") : F("false");
        body += F(",\"cardType\":\"");
        body += _storage->cardTypeName();
        body += F("\",\"capacityBytes\":");
        body += static_cast<unsigned long long>(_storage->cardSizeBytes());
        body += '}';

        AsyncWebServerResponse* response =
            request->beginResponse(200, "application/json", body);
        response->addHeader(
            "Cache-Control",
            "no-store, no-cache, must-revalidate"
        );
        request->send(response);
        return;
    }

    String sdPath;
    if (!mapRequestPath(request->url(), sdPath)) {
        request->send(404, "text/plain", "Not found");
        return;
    }

    if (request->url() == "/logo.png" &&
        !_storage->existsOnSd(sdPath.c_str()) &&
        !LittleFS.exists("/logo.png")) {
        AsyncWebServerResponse* response = request->beginResponse(
            200,
            "image/svg+xml",
            FALLBACK_LOGO
        );
        response->addHeader("Cache-Control", "public, max-age=3600");
        response->addHeader("X-AquaLook-Storage", "Firmware-Fallback");
        request->send(response);
        return;
    }

    auto context = std::make_shared<SdReadContext>();
    context->storage = _storage;
    context->path = sdPath;

    if (!_storage->openRead(sdPath.c_str(), context->file)) {
        _storage->reportReadError(sdPath.c_str());
        request->send(503, "text/plain", "SD read error");
        return;
    }

    const char* contentType = contentTypeForPath(sdPath);
    AsyncWebServerResponse* response = request->beginChunkedResponse(
        contentType,
        [context](uint8_t* buffer, size_t maxLen, size_t) -> size_t {
            if (!context->file.isOpen()) return 0;

            const int32_t count = context->file.read(buffer, maxLen);
            if (count < 0) {
                if (context->storage) {
                    context->storage->reportReadError(context->path.c_str());
                }
                context->file.close();
                return 0;
            }
            if (count == 0) {
                context->file.close();
                return 0;
            }
            return static_cast<size_t>(count);
        }
    );

    response->addHeader("Cache-Control", "public, max-age=300");
    response->addHeader("X-AquaLook-Storage", "SD");
    request->send(response);
}

bool SdStaticHandler::mapRequestPath(const String& requestPath, String& sdPath) {
    if (requestPath.length() == 0 || requestPath[0] != '/') return false;
    if (requestPath.indexOf("..") >= 0 || requestPath.indexOf('\\') >= 0) return false;
    if (requestPath.startsWith("/api/") || requestPath == "/setup" ||
        requestPath == "/logs") return false;
    if (!hasStaticExtension(requestPath)) return false;

    sdPath = "/www";
    sdPath += requestPath;
    return true;
}

const char* SdStaticHandler::contentTypeForPath(const String& path) {
    if (path.endsWith(".html")) return "text/html; charset=utf-8";
    if (path.endsWith(".css"))  return "text/css; charset=utf-8";
    if (path.endsWith(".js"))   return "application/javascript; charset=utf-8";
    if (path.endsWith(".json")) return "application/json; charset=utf-8";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".gif"))  return "image/gif";
    if (path.endsWith(".webp")) return "image/webp";
    if (path.endsWith(".ico"))  return "image/x-icon";
    if (path.endsWith(".xml"))  return "application/xml";
    if (path.endsWith(".woff")) return "font/woff";
    if (path.endsWith(".woff2"))return "font/woff2";
    if (path.endsWith(".ttf"))  return "font/ttf";
    return "text/plain; charset=utf-8";
}
