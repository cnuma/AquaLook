#include "SdStaticHandler.h"

#include <LittleFS.h>
#include <memory>

#include "EventLog.h"

namespace {

struct SdReadContext {
    FsFile file;
    StorageManager* storage = nullptr;
    String path;
    bool leaseHeld = false;

    void closeAndRelease() {
        if (file.isOpen()) file.close();

        if (leaseHeld && storage) {
            storage->releaseWebRead();
            leaseHeld = false;
        }
    }

    ~SdReadContext() {
        closeAndRelease();
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

// Portail de test embarque : volontairement independant de la SD afin que
// pull + compilation + upload suffisent a qualifier le WiFi.
// Le bouton Scanner lit le cache constitue AVANT l'ouverture du SoftAP.
// WIFI_RESET_SENTINEL est transforme en SSID/PWD vides par NvsBlobRecovery.
const char CAPTIVE_SETUP_HTML[] PROGMEM = R"html(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Configuration WiFi</title><style>body{font-family:sans-serif;background:#1a1a2e;color:#eee;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}.card{background:#16213e;border-radius:12px;padding:2rem;width:90%;max-width:390px;box-shadow:0 4px 20px rgba(0,0,0,.4)}h2{margin:0 0 .5rem;text-align:center;color:#4fc3f7}.hint{margin:0 0 1.2rem;text-align:center;color:#90caf9;font-size:.85rem}label{display:block;margin-bottom:.3rem;font-size:.9rem;color:#90caf9}input,select{width:100%;box-sizing:border-box;padding:.65rem .8rem;border:1px solid #334;border-radius:6px;background:#0f3460;color:#eee;font-size:1rem;margin-bottom:1rem}select option{background:#0f3460}.row{display:flex;gap:.5rem;margin-bottom:1rem}.row input{margin-bottom:0;flex:1}.scan{flex-shrink:0;padding:.65rem .9rem;border:1px solid #4fc3f7;border-radius:6px;background:transparent;color:#4fc3f7;font-size:.9rem;cursor:pointer;white-space:nowrap}.primary,.reset{width:100%;padding:.75rem;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer}.primary{border:0;background:#4fc3f7;color:#000}.reset{margin-top:1rem;border:1px solid #ef5350;background:transparent;color:#ef9a9a}#networks{display:none}#msg{min-height:1.2rem;margin-top:1rem;text-align:center;font-size:.9rem}</style></head><body><div class="card"><h2>&#127807; Configuration WiFi</h2><p class="hint">Le scan a ete effectue avant l'ouverture du portail pour conserver la connexion au PC.</p><label for="ssid">Reseau WiFi (SSID)</label><div class="row"><input id="ssid" type="text" placeholder="Nom du reseau" autocomplete="off"><button class="scan" id="scan" type="button" onclick="loadScan()">&#128246; Scanner</button></div><select id="networks" onchange="pick(this.value)"><option value="">-- Selectionner un reseau --</option></select><label for="pwd">Mot de passe</label><input id="pwd" type="password" placeholder="Mot de passe"><button class="primary" type="button" onclick="saveWifi()">Enregistrer et connecter</button><button class="reset" type="button" onclick="resetWifi()">RAZ WiFi (SSID / mot de passe)</button><div id="msg"></div></div><script>const $=id=>document.getElementById(id);const RESET='__AQUALOOK_WIFI_RESET__';function message(t,c){$('msg').textContent=t;$('msg').style.color=c||'#eee'}function pick(v){if(v){$('ssid').value=v;$('pwd').focus()}}async function loadScan(){const b=$('scan');b.disabled=true;message('Lecture du scan memorise...','#4fc3f7');try{const r=await fetch('/api/wifi/scan',{cache:'no-store'});if(!r.ok)throw new Error('HTTP '+r.status);const d=await r.json();if(d.scanning){setTimeout(loadScan,400);return}const s=$('networks');s.innerHTML='<option value="">-- Selectionner un reseau --</option>';const n=d.networks||[];n.forEach(x=>{const o=document.createElement('option');o.value=x.ssid;o.textContent=x.ssid+' ('+x.rssi+' dBm)'+(x.secured?' [securise]':'');s.appendChild(o)});s.style.display=n.length?'block':'none';message(n.length?n.length+' reseau(x) en cache':'Aucun reseau trouve lors du prescan',n.length?'#81c784':'#ffb74d')}catch(e){message('Lecture du cache impossible: '+e.message,'#f66')}finally{b.disabled=false}}async function saveWifi(){const ssid=$('ssid').value.trim(),pwd=$('pwd').value.trim();if(!ssid){message('SSID requis','#f66');return}message('Enregistrement...','#4fc3f7');try{const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pwd})});message(r.ok?'Enregistre - redemarrage...':'Erreur serveur',r.ok?'#81c784':'#f66')}catch(e){message('Erreur reseau','#f66')}}async function resetWifi(){if(!confirm('Effacer uniquement le SSID et le mot de passe WiFi puis redemarrer ?'))return;message('RAZ WiFi...','#ffb74d');try{const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:RESET,pwd:''})});message(r.ok?'WiFi efface - redemarrage...':'Erreur serveur',r.ok?'#81c784':'#f66')}catch(e){message('Redemarrage en cours...','#81c784')}}loadScan();</script></body></html>
)html";

}  // namespace

SdStaticHandler::SdStaticHandler(StorageManager* storage)
    : _storage(storage) {}

bool SdStaticHandler::canHandle(AsyncWebServerRequest* request) const {
    if (!_storage || !request) return false;
    if (request->method() != HTTP_GET && request->method() != HTTP_HEAD) return false;

    const String url = request->url();

    // Le portail de qualification WiFi est toujours servi depuis le firmware.
    if (url == "/setup") return true;

    // Diagnostic toujours disponible, meme sans carte SD.
    if (url == "/api/storage") return true;

    String sdPath;
    if (!mapRequestPath(url, sdPath)) return false;

    if (_storage->isSdAvailable() && _storage->existsOnSd(sdPath.c_str())) {
        return true;
    }

    if (url == "/logo.png" && !LittleFS.exists("/logo.png")) return true;

    return false;
}

void SdStaticHandler::handleRequest(AsyncWebServerRequest* request) {
    if (!request || !_storage) return;

    if (request->url() == "/setup") {
        AsyncWebServerResponse* response = request->beginResponse(
            200,
            "text/html; charset=utf-8",
            CAPTIVE_SETUP_HTML
        );
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        response->addHeader("X-AquaLook-Storage", "Firmware-Captive");
        request->send(response);
        return;
    }

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

        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", body);
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
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
        AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", FALLBACK_LOGO);
        response->addHeader("Cache-Control", "public, max-age=3600");
        response->addHeader("X-AquaLook-Storage", "Firmware-Fallback");
        request->send(response);
        return;
    }

    auto context = std::make_shared<SdReadContext>();
    context->storage = _storage;
    context->path = sdPath;

    if (!_storage->openRead(sdPath.c_str(), context->file)) {
        if (_storage->isWebReadQuarantined()) {
            AsyncWebServerResponse* response = request->beginResponse(503, "text/plain", "SD recovery in progress");
            response->addHeader("Retry-After", "2");
            response->addHeader("X-AquaLook-Storage", "SD-Quarantine");
            request->send(response);
            return;
        }

        _storage->reportReadError(sdPath.c_str());
        request->send(503, "text/plain", "SD read error");
        return;
    }
    context->leaseHeld = true;

    const char* contentType = contentTypeForPath(sdPath);
    AsyncWebServerResponse* response = request->beginChunkedResponse(
        contentType,
        [context](uint8_t* buffer, size_t maxLen, size_t) -> size_t {
            if (!context->file.isOpen()) return 0;

            const int32_t count = context->file.read(buffer, maxLen);
            if (count < 0) {
                StorageManager* storage = context->storage;
                const String path = context->path;
                context->closeAndRelease();
                if (storage) storage->reportReadError(path.c_str());
                return 0;
            }
            if (count == 0) {
                context->closeAndRelease();
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
    if (requestPath.startsWith("/api/")) return false;

    // /setup est volontairement reserve au portail embarque de qualification.
    if (requestPath == "/setup") return false;

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
