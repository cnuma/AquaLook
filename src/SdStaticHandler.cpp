#include "SdStaticHandler.h"

#include <memory>

#include "EventLog.h"

namespace {

struct SdReadContext {
    FsFile file;

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

}  // namespace

SdStaticHandler::SdStaticHandler(StorageManager* storage)
    : _storage(storage) {}

bool SdStaticHandler::canHandle(AsyncWebServerRequest* request) const {
    if (!_storage || !_storage->isSdAvailable() || !request) return false;
    if (request->method() != HTTP_GET && request->method() != HTTP_HEAD) return false;

    String sdPath;
    if (!mapRequestPath(request->url(), sdPath)) return false;
    return _storage->existsOnSd(sdPath.c_str());
}

void SdStaticHandler::handleRequest(AsyncWebServerRequest* request) {
    String sdPath;
    if (!request || !_storage || !mapRequestPath(request->url(), sdPath)) {
        request->send(404, "text/plain", "Not found");
        return;
    }

    auto context = std::make_shared<SdReadContext>();
    if (!_storage->openRead(sdPath.c_str(), context->file)) {
        EventLog::log(LOG_WARN, "Web SD: ouverture impossible %s", sdPath.c_str());
        request->send(404, "text/plain", "Not found");
        return;
    }

    const char* contentType = contentTypeForPath(sdPath);
    AsyncWebServerResponse* response = request->beginChunkedResponse(
        contentType,
        [context](uint8_t* buffer, size_t maxLen, size_t) -> size_t {
            if (!context->file.isOpen()) return 0;

            const int32_t count = context->file.read(buffer, maxLen);
            if (count <= 0) {
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
