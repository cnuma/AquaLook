#include "SdStaticHandler.h"

#include <LittleFS.h>
#include <memory>

#include "EventLog.h"
#include <esp_heap_caps.h>

namespace {

// ── Bornage de la concurrence sur les fichiers SD ─────────────────────────
//
// Chaque reponse en flux retient un tampon et un descripteur de fichier
// jusqu'a son terme. Sans borne, c'est l'epuisement memoire qui arbitre : le
// 16 aout 2026, trois requetes simultanees suffisaient a rendre le serveur
// totalement muet, y compris pour les routes qui ne touchent pas la SD.
// Corriger la cause (liberation des sprites en veille) a releve le plafond de
// 2 a plus de 8, mais n'a pas cree de borne : ecran allume, la marge reste
// mince. Mieux vaut refuser proprement une requete de trop que s'effondrer.
//
// Le garde-fou porte d'abord sur la MEMOIRE reellement disponible, et non sur
// un simple compteur : le nombre de reponses tenables depend de l'etat de
// l'ecran (sprites alloues ou non), donc un seuil fixe serait tantot trop
// permissif, tantot inutilement restrictif. Le compteur ne sert que de
// garde-fou ultime contre un emballement.
constexpr uint32_t SD_MIN_FREE_BYTES = 12000UL;
constexpr uint8_t  SD_MAX_INFLIGHT   = 8U;
constexpr uint32_t SD_REJECT_LOG_INTERVAL_MS = 10000UL;

portMUX_TYPE g_sdInflightMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t  g_sdInflight = 0U;
uint32_t g_sdRejectLogAtMs = 0U;
uint32_t g_sdRejectCount = 0U;

struct SdReadContext {
    FsFile file;
    StorageManager* storage = nullptr;
    String path;
    bool counted = false;

    ~SdReadContext() {
        if (file.isOpen() && storage) storage->closeFile(file);
        // Decremente ici plutot qu'a la fin du flux : ce destructeur s'execute
        // aussi lorsque le client coupe la connexion en cours de route, cas ou
        // un decompte place dans le rappel de lecture ne passerait jamais.
        if (counted) {
            portENTER_CRITICAL(&g_sdInflightMux);
            if (g_sdInflight > 0U) g_sdInflight--;
            portEXIT_CRITICAL(&g_sdInflightMux);
        }
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

    // Refus propre plutot qu'effondrement : si la memoire est deja basse ou si
    // trop de flux sont en cours, repondre 503 avec Retry-After. Le navigateur
    // reessaiera de lui-meme, ce qui degrade le temps de chargement au lieu de
    // rendre le module muet.
    const uint32_t freeBytes =
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    bool refuse = false;
    uint8_t inflightNow = 0U;

    portENTER_CRITICAL(&g_sdInflightMux);
    inflightNow = g_sdInflight;
    if (freeBytes < SD_MIN_FREE_BYTES || g_sdInflight >= SD_MAX_INFLIGHT) {
        refuse = true;
        g_sdRejectCount++;
    } else {
        g_sdInflight++;
    }
    portEXIT_CRITICAL(&g_sdInflightMux);

    if (refuse) {
        const uint32_t nowMs = millis();
        // Journal limite : un refus arrive rarement seul, et une rafale de
        // lignes aggraverait la situation memoire qu'on cherche a proteger.
        if (nowMs - g_sdRejectLogAtMs >= SD_REJECT_LOG_INTERVAL_MS) {
            g_sdRejectLogAtMs = nowMs;
            EventLog::log(LOG_WARN,
                          "Web: requete SD refusee (libre=%lu enCours=%u total=%lu) "
                          "— protection contre la saturation",
                          static_cast<unsigned long>(freeBytes),
                          static_cast<unsigned>(inflightNow),
                          static_cast<unsigned long>(g_sdRejectCount));
        }
        AsyncWebServerResponse* busy =
            request->beginResponse(503, "text/plain", "Occupe, reessayez");
        busy->addHeader("Retry-After", "1");
        request->send(busy);
        return;
    }

    auto context = std::make_shared<SdReadContext>();
    context->storage = _storage;
    context->path = sdPath;
    context->counted = true;

    if (!_storage->openRead(sdPath.c_str(), context->file)) {
        _storage->reportReadError(sdPath.c_str());
        request->send(503, "text/plain", "SD read error");
        return;
    }

    const char* contentType = contentTypeForPath(sdPath);
    AsyncWebServerResponse* response = request->beginChunkedResponse(
        contentType,
        [context](uint8_t* buffer, size_t maxLen, size_t) -> size_t {
            if (!context->file.isOpen() || !context->storage) return 0;

            // Lecture NON bloquante : ce rappel s'execute sur la tache unique
            // d'AsyncTCP, partagee par toutes les connexions. Y attendre le
            // mutex SD bloquerait le serveur entier des que deux fichiers SD
            // sont demandes en meme temps — ce que fait tout navigateur (voir
            // la note sur readChunkNonBlocking dans StorageManager.cpp).
            const int32_t count =
                context->storage->readChunkNonBlocking(context->file, buffer, maxLen);

            // Bus occupe : rendre la main sans rien ecrire ni terminer la
            // reponse. RESPONSE_TRY_AGAIN demande a la bibliotheque de
            // rappeler plus tard, ce qui laisse les autres connexions avancer.
            // Surtout ne pas retourner 0 ici : 0 signifie "fin du corps" et
            // tronquerait silencieusement le fichier servi.
            if (count == StorageManager::READ_CHUNK_BUSY) return RESPONSE_TRY_AGAIN;

            if (count < 0) {
                context->storage->reportReadError(context->path.c_str());
                context->storage->closeFile(context->file);
                return 0;
            }
            if (count == 0) {
                context->storage->closeFile(context->file);
                return 0;
            }
            return static_cast<size_t>(count);
        }
    );

    // Auparavant "public, max-age=300" : un navigateur pouvait rester
    // jusqu'a 5 min sur une ancienne version d'une page apres une mise a
    // jour cote SD, sans aucun moyen de le forcer autrement qu'un vidage
    // manuel du cache. "no-cache" force une revalidation aupres du serveur
    // a chaque chargement (donc toujours la derniere version), sans pour
    // autant empecher le navigateur de reutiliser une reponse identique
    // s'il sait la revalider — pas de cout reel sur un reseau local.
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("X-AquaLook-Storage", "SD");
    request->send(response);
}

bool SdStaticHandler::mapRequestPath(const String& requestPath, String& sdPath) {
    if (requestPath.length() == 0 || requestPath[0] != '/') return false;
    if (requestPath.indexOf("..") >= 0 || requestPath.indexOf('\\') >= 0) return false;
    if (requestPath.startsWith("/api/")) return false;

    // Pages hybrides : la version complete est servie depuis la SD quand le
    // fichier est present. Sinon, le handler decline la requete pour laisser
    // WebManager fournir le fallback embarque correspondant.
    if (requestPath == "/setup") {
        sdPath = "/www/setup.html";
        return true;
    }
    if (requestPath == "/logs") {
        sdPath = "/www/logs.html";
        return true;
    }

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
