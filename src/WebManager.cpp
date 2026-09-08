#include "WebManager.h"
#include "RelayTopologyStore.h"
#include "HeapMetrics.h"
#include "BootLoopGuard.h"
#include "EventBus.h"
#include "EventLog.h"
#include "ScriptVmSelfTest.h"
#include "ScriptHostRuntime.h"
#include "ApiAuth.h"
#include "ScriptStore.h"
#include "SystemDiagnostics.h"
#include "TimeUtils.h"
#include "WebAssetsUpdater.h"
#include "CloudSync.h"
#include "UpdateCheckScheduler.h"
#include "BootLoopGuard.h"
#include "DisplayManager.h"
#include <esp_heap_caps.h>
#include <nvs.h>
#include <nvs_flash.h>

// ─────────────────────────────────────────────────────────────
//  Page HTML du portail captif — servie en mode AP
//  Formulaire SSID/PWD minimaliste, POST vers /api/wifi
// ─────────────────────────────────────────────────────────────
static const char CAPTIVE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - WiFi</title><style>body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:#1a1a2e;color:#eee;font-family:sans-serif}.card{box-sizing:border-box;width:90%;max-width:360px;padding:2rem;background:#16213e;border-radius:12px;box-shadow:0 4px 20px #0006}h2{margin:0 0 .5rem;text-align:center;color:#4fc3f7}p{margin:0 0 1.4rem;text-align:center;color:#90caf9;font-size:.9rem;line-height:1.4}label{display:block;margin:.8rem 0 .3rem;color:#90caf9;font-size:.9rem}input,button{box-sizing:border-box;width:100%;padding:.75rem;border-radius:6px;font-size:1rem}input{border:1px solid #334;background:#0f3460;color:#eee}button{margin-top:1.2rem;border:0;background:#4fc3f7;color:#000;font-weight:700}#msg{min-height:1.2rem;margin-top:1rem;text-align:center;font-size:.9rem}</style></head><body><main class="card"><h2>&#127807; Configuration WiFi</h2><p>Mode de secours : saisissez manuellement le nom exact de votre reseau.</p><label for="ssid">Reseau WiFi (SSID)</label><input id="ssid" autocomplete="off" placeholder="Nom du reseau"><label for="pwd">Mot de passe</label><input id="pwd" type="password" placeholder="Mot de passe"><button type="button" onclick="saveWifi()">Enregistrer et connecter</button><div id="msg"></div></main><script>const $=x=>document.getElementById(x);async function saveWifi(){const s=$('ssid').value.trim(),p=$('pwd').value.trim(),m=$('msg');if(!s){m.textContent='SSID requis';m.style.color='#f66';return}m.textContent='Enregistrement...';m.style.color='#4fc3f7';try{const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,pwd:p})});m.textContent=r.ok?'Enregistre - redemarrage...':'Erreur serveur';m.style.color=r.ok?'#81c784':'#f66'}catch(e){m.textContent='Erreur reseau';m.style.color='#f66'}}</script></body></html>
)rawhtml";

// ───── Diffusion des pages HTML embarquees ──────────────────────────────
//
// Les pages /ota et /logs pesent une douzaine de kilo-octets et partent en
// plusieurs blocs TCP successifs. ESPAsyncWebServer calcule la taille du bloc a
// partir de la place annoncee par la pile TCP, remplit le bloc, puis ecrit — et
// ignore la valeur rendue par l'ecriture. Si la place a diminue entre-temps
// parce que d'autres connexions ont consomme le tampon partage,
// AsyncClient::add() n'envoie que ce qui rentre encore, mais le curseur de
// lecture de la reponse avance de la taille DEMANDEE. Les octets refuses ne
// sont jamais reproposes : le navigateur recoit une page amputee en son milieu,
// accompagnee d'un Content-Length annoncant la taille complete.
//
// Mesure du 17 aout 2026, ecran allume (plus gros bloc libre : 17 Ko), page
// /ota de 12 503 octets, comparaison octet a octet contre une reference :
//     1 chargement simultane   -> 1/1 intacte
//     2 chargements simultanes -> 2/2 intactes
//     3 chargements simultanes -> 1/3 intacte
//     8 chargements simultanes -> 0/8 intactes (jusqu'a 174 octets perdus au
//                                 milieu, le reste du document correct)
//
// Le defaut est dans la bibliotheque. Une correction de son controle de flux a
// ete tentee puis abandonnee le meme jour : elle degradait le comportement
// (connexions restant sans reponse). Tant qu'elle n'est pas comprise de bout en
// bout, on ne prend pas le risque de la modifier.
//
// La parade retenue est donc applicative et volontairement modeste : borner le
// nombre de pages servies en parallele et refuser explicitement au-dela, avec
// un code 503 et un Retry-After. Une page a recharger est un desagrement ; une
// page fausse presentee comme complete est une perte de confiance.
namespace {

constexpr uint8_t  PAGE_MAX_INFLIGHT   = 1U;
constexpr uint32_t PAGE_MIN_FREE_BYTES = 12000UL;
// Duree au-dela de laquelle un envoi en cours est forcement termine ou perdu :
// une page de 12 Ko sur un reseau local ne prend pas dix secondes. Ce delai
// existe uniquement pour que le compteur ne puisse jamais rester bloque en
// haut. Un compteur qui fuit rendrait les pages definitivement inaccessibles,
// soit une panne bien pire que le defaut qu'on cherche a contourner.
constexpr uint32_t PAGE_INFLIGHT_STALE_MS = 10000UL;

portMUX_TYPE g_pageInflightMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t  g_pageInflight        = 0U;
uint32_t g_pageInflightSinceMs = 0U;
uint32_t g_pageRejectCount     = 0U;
uint32_t g_pageRejectLogAtMs   = 0U;

}  // namespace

void WebManager::sendEmbeddedPage(AsyncWebServerRequest* req,
                                  const char* page,
                                  size_t pageLength,
                                  const char* extraHeaderName,
                                  const char* extraHeaderValue) {
    const uint32_t nowMs = millis();
    const uint32_t freeBytes =
        static_cast<uint32_t>(AquaLook::Heap::freeBytes());

    bool refuse = false;
    bool unstuck = false;
    uint32_t rejectCount = 0U;

    portENTER_CRITICAL(&g_pageInflightMux);
    if (g_pageInflight > 0U &&
        (nowMs - g_pageInflightSinceMs) >= PAGE_INFLIGHT_STALE_MS) {
        g_pageInflight = 0U;
        unstuck = true;
    }
    if (g_pageInflight >= PAGE_MAX_INFLIGHT || freeBytes < PAGE_MIN_FREE_BYTES) {
        refuse = true;
        rejectCount = ++g_pageRejectCount;
    } else {
        if (g_pageInflight == 0U) {
            g_pageInflightSinceMs = nowMs;
        }
        g_pageInflight++;
    }
    portEXIT_CRITICAL(&g_pageInflightMux);

    if (unstuck) {
        EventLog::log(LOG_WARN,
                      "Web: compteur de pages debloque apres %lu ms sans fin "
                      "d'envoi — securite anti-blocage",
                      static_cast<unsigned long>(PAGE_INFLIGHT_STALE_MS));
    }

    if (refuse) {
        // Journal limite : un refus arrive rarement seul.
        if (nowMs - g_pageRejectLogAtMs >= 10000UL) {
            g_pageRejectLogAtMs = nowMs;
            EventLog::log(LOG_WARN,
                          "Web: page refusee (libre=%lu max=%u refus=%lu) — "
                          "mieux vaut recharger qu'une page incomplete",
                          static_cast<unsigned long>(freeBytes),
                          static_cast<unsigned>(PAGE_MAX_INFLIGHT),
                          static_cast<unsigned long>(rejectCount));
        }
        AsyncWebServerResponse* busy = req->beginResponse(
            503, "text/html; charset=utf-8",
            "<!DOCTYPE html><html lang=\"fr\"><head><meta charset=\"UTF-8\">"
            "<meta http-equiv=\"refresh\" content=\"3\">"
            "<title>AquaLook</title></head><body style=\"font-family:sans-serif;"
            "margin:2rem\"><h1>Un instant</h1><p>AquaLook termine d'envoyer une "
            "autre page. Celle-ci se rechargera toute seule dans trois "
            "secondes.</p></body></html>");
        busy->addHeader("Retry-After", "3");
        busy->addHeader("Cache-Control", "no-store");
        req->send(busy);
        return;
    }

    // Libere le jeton quand la connexion se ferme, ce que la bibliotheque fait
    // systematiquement en fin de reponse (l'en-tete Connection: close est pose
    // par la reponse elle-meme). Le delai anti-blocage ci-dessus couvre le cas
    // ou cette fermeture tarderait.
    req->onDisconnect([]() {
        portENTER_CRITICAL(&g_pageInflightMux);
        if (g_pageInflight > 0U) {
            g_pageInflight--;
            g_pageInflightSinceMs = millis();
        }
        portEXIT_CRITICAL(&g_pageInflightMux);
    });

    // Surcharge (const uint8_t*, size_t) et NON (const char*) : elle seule
    // diffuse la page directement depuis la flash (AsyncProgmemResponse). La
    // surcharge const char* recopie la page entiere dans une String du tas,
    // puis appelle substring() sur le reste a chaque acquittement — une
    // nouvelle copie a chaque fois. Pour 12 Ko, le pic depasse 25 Ko alors que
    // le plus gros bloc libre tombe a 17 Ko quand l'ecran est allume ; quand
    // l'allocation echoue, la bibliotheque ne le verifie pas et envoie la
    // longueur prevue depuis une String vide. Le 17 aout 2026, /ota a ainsi
    // livre au navigateur neuf kilo-octets du tas du module a la place de la
    // page.
    AsyncWebServerResponse* response = req->beginResponse(
        200, "text/html; charset=utf-8",
        reinterpret_cast<const uint8_t*>(page), pageLength);
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    if (extraHeaderName != nullptr && extraHeaderValue != nullptr) {
        response->addHeader(extraHeaderName, extraHeaderValue);
    }
    req->send(response);
}

// ═══════════════════════════════════════════════════════════════
//  begin()
// ═══════════════════════════════════════════════════════════════
void WebManager::begin(NTPManager* ntp, WeatherManager* weather,
                       RelaisManager* relais, ScheduleManager* schedule,
                       ConfigManager* config, WiFiManager* wifi) {
    _ntp      = ntp;
    _weather  = weather;
    _relais   = relais;
    _schedule = schedule;
    _config   = config;
    _wifi     = wifi;

    // ESPAsyncWebServer garde les connexions HTTP ouvertes (keep-alive) par
    // defaut. Symptome observe sur le terrain : apres une periode sans appel
    // de page, le premier rechargement affiche la page sans donnees (la
    // requete /api/status tente de reutiliser une connexion TCP devenue
    // silencieusement morte cote ESP32 et reste bloquee jusqu'a un timeout
    // navigateur) ; un second rechargement force une connexion neuve et
    // fonctionne. "Connection: close" sur chaque reponse force le navigateur
    // a ouvrir une connexion neuve a chaque requete : cout negligeable ici
    // (page peu sollicitee, interrogee toutes les 8s), mais elimine cette
    // classe de blocage. S'applique a toutes les reponses (copie dans
    // AsyncWebServerResponse a la construction), fichiers statiques inclus.
    DefaultHeaders::Instance().addHeader("Connection", "close");

    // Invariant I1 : LittleFS déjà monté par ConfigManager
    setupRoutes();
    _server.begin();
    Serial.println("[Web] Serveur démarré port 80");
}

// ═══════════════════════════════════════════════════════════════
//  Routes
// ═══════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────
//  update — opérations différées hors de la tâche AsyncTCP
// ─────────────────────────────────────────────────────────────
void WebManager::update() {
    const uint32_t nowMs = millis();

    if (_restartPending && AquaLook::Time::deadlineReached(nowMs, _restartAtMs)) {
        _restartPending = false;
        BootLoopGuard::restartDeliberately("demande depuis l'interface Web");
        return;
    }

    // Avant le early-return ci-dessous : voir runPendingVerify() et la note
    // sur _verifyPending (WebManager.h).
    runPendingVerify();

    if (!_systemSavePending) return;
    if (!AquaLook::Time::deadlineReached(nowMs, _systemSaveAtMs)) return;

    CfgSystem pending;
    uint16_t manualDuration = 10;
    bool manualDurationValid = false;
    bool rebootAfter = false;

    portENTER_CRITICAL(&_pendingMux);
    if (!_systemSavePending) {
        portEXIT_CRITICAL(&_pendingMux);
        return;
    }
    pending = _pendingSystem;
    manualDuration = _pendingManualDuration;
    manualDurationValid = _pendingManualDurationValid;
    rebootAfter = _pendingSystemReboot;
    _systemSavePending = false;
    _pendingManualDurationValid = false;
    portEXIT_CRITICAL(&_pendingMux);

    EventLog::log(LOG_INFO, "Config: application differee (%u zones, manuel=%u min)",
                  pending.nbZones,
                  manualDurationValid ? manualDuration : _config->manual().durationMin);

    if (manualDurationValid) {
        _schedule->setManualDuration(manualDuration);
        _config->setSystemAndManualDuration(pending, manualDuration);
    } else {
        _config->setSystem(pending);
    }

    if (rebootAfter) {
        EventLog::log(LOG_INFO, "Systeme: redemarrage programme apres sauvegarde");
        _restartPending = true;
        _restartAtMs = millis() + 100U;
    }
}

namespace {
// Rejette d'un 414 toute URL anormalement longue AVANT que le serveur de
// fichiers statiques ne tente de l'ouvrir comme un chemin LittleFS. Sans ce
// garde, une URI de 16 Ko faisait redemarrer le module : l'ouverture du chemin
// geant, plus son impression sur la console serie, bloquaient la tache
// async_tcp au-dela des 5 s du chien de garde de tache, qui declenchait un
// panic (voir docs/ROBUSTESSE_RESEAU_2026-09-04.md, defaut n°1). 512 octets
// couvrent largement les URL legitimes du projet, chemins de fichiers compris.
constexpr size_t MAX_URL_LENGTH = 512;

class UriLengthGuard : public AsyncWebHandler {
public:
    bool canHandle(AsyncWebServerRequest* request) const override {
        return request->url().length() > MAX_URL_LENGTH;
    }
    void handleRequest(AsyncWebServerRequest* request) override {
        request->send(414, "text/plain", "URI trop longue");
    }
};
UriLengthGuard g_uriLengthGuard;
}  // namespace

void WebManager::setupRoutes() {
    // Premier handler enregistre, donc premier consulte : il court-circuite les
    // URL demesurees avant tout autre routage, serveStatic compris.
    _server.addHandler(&g_uriLengthGuard);

    // ── Détection portail captif (iOS/Android/Windows) ──────────
    // Stratégie : répondre de façon à ce que chaque OS détecte un portail
    // et ouvre automatiquement le navigateur captif.
    //
    // iOS/macOS : hotspot-detect.html → redirection vers /setup
    // Android   : generate_204 → 302 redirect (attendait 204 = pas de portail)
    // Windows   : connecttest.txt → redirection (attendait "Microsoft Connect Test")
    //
    // Le onNotFound redirige tout le reste vers /setup en mode captif.
    // Ces handlers évitent les erreurs LittleFS sur ces URLs connues.

    auto captiveRedirect = [this](AsyncWebServerRequest* req) {
        String url = "http://";
        url += (_wifi && _wifi->isCaptivePortal())
               ? _wifi->getApIP().toString()
               : req->host();
        url += "/setup";
        req->redirect(url);
    };

    _server.on("/hotspot-detect.html",   HTTP_GET, captiveRedirect);
    _server.on("/generate_204",          HTTP_GET, captiveRedirect);
    _server.on("/gen_204",               HTTP_GET, captiveRedirect);
    _server.on("/connecttest.txt",       HTTP_GET, captiveRedirect);
    _server.on("/redirect",              HTTP_GET, captiveRedirect);
    _server.on("/success.txt",           HTTP_GET, captiveRedirect);
    _server.on("/ncsi.txt",              HTTP_GET, captiveRedirect);
    _server.on("/canonical.html",        HTTP_GET, captiveRedirect);
    _server.on("/chat",                  HTTP_GET, captiveRedirect);

    // Racine
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/index.html");
    });

    // Page portail captif — accessible même sans LittleFS
    _server.on("/setup", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", CAPTIVE_HTML);
    });
    // Redirect captif — répond à toute URL inconnue en mode AP
    _server.onNotFound([this](AsyncWebServerRequest* req) {
        if (_wifi && _wifi->isCaptivePortal()) {
            req->redirect("http://" + _wifi->getApIP().toString() + "/setup");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });

    // ── GET ───────────────────────────────────
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleStatus(req);
    });
    _server.on("/api/io", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleGetIo(req);
    });
    _server.on("/api/adminStatus", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleAdminStatus(req);
    });
    _server.on("/api/diagnostics", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleDiagnostics(req);
    });
    // Slots d une zone en particulier (demande a la demande depuis le modal)
    // Route query param : /api/zone?z=N — evite les problemes de regex AsyncWebServer
    _server.on("/api/zone", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!req->hasParam("z")) { sendError(req, "parametre z manquant"); return; }
        const String zStr = req->getParam("z")->value();
        // Rejeter explicitement une valeur non numerique : toInt() rendrait 0
        // sur "abc" ou "", et l'on renverrait la zone 0 au lieu d'un refus
        // (docs/ROBUSTESSE_RESEAU_2026-09-04.md, defaut n°7).
        bool numerique = zStr.length() > 0 && zStr.length() <= 3;
        for (unsigned i = 0; numerique && i < zStr.length(); ++i) {
            if (!isdigit((unsigned char)zStr[i])) numerique = false;
        }
        if (!numerique) { sendError(req, "zone invalide"); return; }
        uint8_t z = (uint8_t)zStr.toInt();
        if (!_config || z >= _config->nbZones()) { sendError(req, "zone invalide"); return; }
        JsonDocument doc;
        if (_schedule) {
            ZoneSchedule zs = _schedule->getZoneSchedule(z);
            JsonArray days = doc["daySlots"].to<JsonArray>();
            for (uint8_t d = 0; d < NB_DAYS; d++) {
                JsonArray row = days.add<JsonArray>();
                for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                    const TimeSlot& ts = zs.daySlots[d].slots[s];
                    JsonObject so = row.add<JsonObject>();
                    so["h"] = ts.hour; so["m"] = ts.minute;
                    so["d"] = ts.duration; so["e"] = ts.enabled;
                }
            }
            JsonArray isl = doc["intervalSlots"].to<JsonArray>();
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const TimeSlot& ts = zs.intervalSlots.slots[s];
                JsonObject so = isl.add<JsonObject>();
                so["h"] = ts.hour; so["m"] = ts.minute;
                so["d"] = ts.duration; so["e"] = ts.enabled;
            }
        }
        sendJson(req, doc);
    });

    // ── POST planning ─────────────────────────
    _server.on("/api/display", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleGetDisplay(req);
    });

#define POST_JSON(uri, handler) \
    addJsonHandler(uri, [this](AsyncWebServerRequest* req, JsonVariant& jv) { \
        JsonDocument doc; doc.set(jv); handler(req, doc); \
    })

    POST_JSON("/api/mode",          handleSetMode);
    POST_JSON("/api/io/config",     handleSetIoConfig);
    POST_JSON("/api/io/output",     handleSetIoOutput);
    POST_JSON("/api/interval",      handleSetInterval);
    POST_JSON("/api/intervalAnchor",handleSetIntervalAnchor);
    POST_JSON("/api/deleteInterval",handleDeleteIntervalProgramming);
    POST_JSON("/api/dayslot",       handleSetDaySlot);
    POST_JSON("/api/intervalslot",  handleSetIntervalSlot);
    POST_JSON("/api/rain",          handleSetRain);
    POST_JSON("/api/manual",        handleManual);
    POST_JSON("/api/manualDuration",handleSetManualDuration);
    POST_JSON("/api/saveSchedule",  handleSaveSchedule);

    // ── POST config (v2) ──────────────────────
    POST_JSON("/api/wifi",          handleSetWifi);
    POST_JSON("/api/wifiKeepalive", handleSetWifiKeepalive);
    POST_JSON("/api/touch",         handleSetTouch);
    POST_JSON("/api/ntp",           handleSetNtp);
    POST_JSON("/api/owm",           handleSetOwm);
    POST_JSON("/api/system",        handleSetSystem);
    POST_JSON("/api/auth-secret",  handleSetApiSecret);
    POST_JSON("/api/script-save",  handleSaveScript);
    POST_JSON("/api/script-erase", handleEraseScript);
    POST_JSON("/api/script-run",   handleRunScript);
    POST_JSON("/api/zoneName",      handleSetZoneName);
    POST_JSON("/api/zoneIdentify",  handleZoneIdentify);
    POST_JSON("/api/webAssetsUrl", handleSetWebAssetsUrl);
    POST_JSON("/api/zoneNotifications", handleSetZoneNotifications);
    POST_JSON("/api/display",       handleSetDisplay);
    POST_JSON("/api/logConfig",     handleSetLogConfig);
    POST_JSON("/api/updateCheck",   handleSetUpdateCheck);
    POST_JSON("/api/cloudSync",     handleSetCloudSync);

    // Validation temporaire de WebAssetsUpdater::verifyOnly — voir la note
    // sur handleVerifyWebAsset (WebManager.h) et ROADMAP.md.
    POST_JSON("/api/debug/verify-web-asset", handleVerifyWebAsset);

#undef POST_JSON

    _server.on("/api/debug/verify-web-asset/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleVerifyWebAssetStatus(req);
    });

    // Validation temporaire de l'ecriture SD reseau — voir la note sur
    // DeployFileState (WebManager.h) et ROADMAP.md.
    _server.on(
        "/api/debug/deploy-file",
        HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleDeployFileComplete(req); },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            handleDeployFileBody(req, data, len, index, total);
        }
    );

    // Diagnostic temporaire pour la fragmentation memoire constatee lors des
    // tests HTTPS (voir ROADMAP.md, "constat du 16 aout 2026") : lecture
    // seule, aucun effet de bord, a retirer une fois la piste tranchee.
    // Sortie du mode degrade, sur action explicite. Volontairement une action
    // de l'utilisateur et non un retour automatique : survivre avec la meteo
    // coupee ne prouve rien sur la meteo, et un retour automatique relancerait
    // la boucle au premier cycle suivant.
    _server.on("/api/bootguard/clear", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!BootLoopGuard::isDegraded()) { sendOk(req); return; }
        if (!BootLoopGuard::clearDegraded()) {
            sendError(req, "effacement impossible", 500);
            return;
        }
        sendOk(req);
    });

    _server.on("/api/debug/heap-info", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleHeapInfo(req);
    });

    // Diagnostic temporaire de saturation NVS (voir ROADMAP.md, "constat du
    // 16 aout 2026 — NVS saturee") : lecture seule, aucune ecriture.
    _server.on("/api/debug/nvs-stats", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleNvsStats(req);
    });

    // Auto-test d'ecriture SD, desormais a la demande. Il tournait a chaque
    // demarrage et ecrivait dans /www, ce qui creait une fenetre de corruption
    // du repertoire des ressources Web a chaque boot — perte reelle constatee
    // le 17 aout 2026. Il ecrit maintenant sous /diag.
    // Mise a jour des ressources Web : declenchee par l'utilisateur, executee
    // au redemarrage en mode maintenance (memoire large, tache dediee, pas de
    // concurrence). Meme protection que les routes /api/maintenance/* : jamais
    // pendant un arrosage, l'invariant interdisant de retarder une commande de
    // vanne.
    _server.on("/api/webassets/update", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!_config || !_relais) { sendError(req, "runtime indisponible", 503); return; }
        for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
            if (_relais->getState(zone)) {
                EventLog::log(LOG_WARN,
                              "WebAssets: mise a jour refusee, zone %u en arrosage",
                              static_cast<unsigned>(zone + 1U));
                sendError(req, "arrosage en cours", 409);
                return;
            }
        }
        if (!MaintenanceRequestStore::save(MaintenanceRequest::WEB_ASSETS_UPDATE)) {
            sendError(req, "enregistrement de la demande impossible", 500);
            return;
        }
        EventLog::log(LOG_WARN,
                      "WebAssets: mise a jour demandee, redemarrage en mode maintenance");
        // Bandeau LCD et voyant passent au violet des maintenant : l'operation
        // est engagee, et l'utilisateur doit savoir qu'il ne faut plus
        // solliciter le module. Le drapeau disparait avec le redemarrage.
        EventBus::updateInProgress = true;
        EventBus::displayDirty = true;   // redessiner le bandeau tout de suite
        // Ecran violet plein, affiche AVANT le redemarrage : c'est le seul
        // moment ou l'affichage fonctionne encore. L'image reste ensuite a
        // l'ecran pendant toute la maintenance - voir showUpdateScreen().
        if (_display) {
            _display->showUpdateScreen("MISE A JOUR",
                                       "Ressources Web en cours d'installation");
        }
        _restartPending = true;
        _restartAtMs = millis() + 750U;
        JsonDocument out;
        out["ok"] = true;
        out["restart"] = true;
        out["command"] = "web_assets_update";
        sendJson(req, out, 202);
    });

    // Deploiement transactionnel : prepare le transit, puis bascule.
    // Les fichiers eux-memes passent par /api/debug/deploy-file, qui ecrit
    // desormais dans /www.new tant qu'un transit est ouvert.
    _server.on("/api/debug/deploy-begin", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!_storage) { sendError(req, "stockage indisponible", 503); return; }
        if (!_storage->beginAssetStaging()) { sendError(req, "preparation du transit impossible", 503); return; }
        _deployStagingOpen = true;
        sendOk(req);
    });

    _server.on("/api/debug/deploy-commit", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!_storage) { sendError(req, "stockage indisponible", 503); return; }
        if (!_deployStagingOpen) { sendError(req, "aucun transit ouvert", 409); return; }

        // Marquer l'origine AVANT la bascule, comme le fait la mise a jour
        // reseau : le marqueur bascule ainsi avec les fichiers qu'il decrit.
        //
        // Sans lui, un depot direct laissait intact le marqueur ecrit par la
        // derniere mise a jour reseau : le module servait des pages 5.9.15 en
        // annoncant 5.9.12. L'interface affichait donc une version fausse avec
        // aplomb -- constate le 31 aout 2026. Un depot direct ne porte aucun
        // numero de version (il vient d'un poste, pas d'une publication), et le
        // dire est plus honnete que de laisser croire a l'ancien.
        {
            static const char VJSON[] =
                "{\"version\":\"depot direct\",\"source\":\"deploy\",\"fileCount\":0}";
            String vpath = String(StorageManager::ASSETS_STAGING) + "/assets-version.json";
            FsFile vf;
            if (_storage->openWrite(vpath.c_str(), vf)) {
                _storage->writeChunk(vf, reinterpret_cast<const uint8_t*>(VJSON),
                                     sizeof(VJSON) - 1U);
                _storage->closeFile(vf);
            }
        }

        const bool ok = _storage->commitAssetStaging();
        _deployStagingOpen = false;
        if (!ok) { sendError(req, "bascule echouee, ancienne version conservee", 500); return; }
        sendOk(req);
    });

    // Autotest de la machine a scripts. En lecture seule et sans effet de
    // bord : il n'instancie que des machines jouets avec un hote simule,
    // aucune vanne n'est touchee.
    _server.on("/api/debug/script-selftest", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        runScriptVmSelfTest(doc);
        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // Essai a blanc de la machine a scripts contre le module REEL.
    //
    // Le programme ne fait que LIRE : entree stabilisee, etat et reliquat
    // d'une zone. Aucune action, donc aucune vanne touchee -- c'est ce qui
    // permet de verifier le pont vers le materiel sans engager d'eau.
    _server.on("/api/debug/script-dryrun", HTTP_GET, [this](AsyncWebServerRequest* req) {
        const uint16_t inputId = req->hasParam("input")
            ? (uint16_t)req->getParam("input")->value().toInt() : 0U;
        const uint16_t zoneId = req->hasParam("zone")
            ? (uint16_t)req->getParam("zone")->value().toInt() : 0U;

        // lire entree -> var0 ; zone active -> var1 ; reliquat -> var2
        const uint8_t code[] = {
            (uint8_t)AquaLook::Domain::ScriptOp::READ_INPUT,
            (uint8_t)(inputId & 0xFF), (uint8_t)(inputId >> 8),
            (uint8_t)AquaLook::Domain::ScriptOp::STORE, 0,
            (uint8_t)AquaLook::Domain::ScriptOp::ZONE_ACTIVE,
            (uint8_t)(zoneId & 0xFF), (uint8_t)(zoneId >> 8),
            (uint8_t)AquaLook::Domain::ScriptOp::STORE, 1,
            (uint8_t)AquaLook::Domain::ScriptOp::ZONE_REMAIN,
            (uint8_t)(zoneId & 0xFF), (uint8_t)(zoneId >> 8),
            (uint8_t)AquaLook::Domain::ScriptOp::STORE, 2,
            (uint8_t)AquaLook::Domain::ScriptOp::HALT
        };

        // Variante AGISSANTE, sur demande explicite : le meme chemin, mais le
        // script suspend ou reprend la zone. Elle sert a eprouver le pont
        // jusqu'au bout -- script, hote, planificateur, vanne -- ce qu'un
        // programme en lecture seule ne peut pas montrer.
        const String act = req->hasParam("action")
            ? req->getParam("action")->value() : String();
        uint8_t actCode = 0U;
        if (act == "pause")  actCode = (uint8_t)AquaLook::Domain::ScriptAction::ZONE_PAUSE;
        if (act == "resume") actCode = (uint8_t)AquaLook::Domain::ScriptAction::ZONE_RESUME;
        const uint8_t actionCode[] = {
            (uint8_t)AquaLook::Domain::ScriptOp::PUSH, 0, 0, 0, 0,
            (uint8_t)AquaLook::Domain::ScriptOp::ACTION, actCode,
            (uint8_t)(zoneId & 0xFF), (uint8_t)(zoneId >> 8),
            (uint8_t)AquaLook::Domain::ScriptOp::ZONE_REMAIN,
            (uint8_t)(zoneId & 0xFF), (uint8_t)(zoneId >> 8),
            (uint8_t)AquaLook::Domain::ScriptOp::STORE, 2,
            (uint8_t)AquaLook::Domain::ScriptOp::HALT
        };

        ScriptRuntimeContext ctx;
        ctx.inputs = _inputs;
        ctx.schedule = _schedule;
        ctx.config = _config;

        AquaLook::Domain::ScriptVm vm;
        vm.load(actCode != 0U
                    ? AquaLook::Domain::ScriptProgram(actionCode, sizeof(actionCode))
                    : AquaLook::Domain::ScriptProgram(code, sizeof(code)),
                &scriptHostOps(), &ctx);
        for (uint8_t i = 0U; i < 8U; ++i) {
            const AquaLook::Domain::ScriptStatus st = vm.tick();
            if (st == AquaLook::Domain::ScriptStatus::FINISHED ||
                st == AquaLook::Domain::ScriptStatus::ABORTED) break;
        }

        JsonDocument doc;
        doc["entree"] = inputId;
        doc["zone"] = zoneId;
        const bool fini = vm.status() == AquaLook::Domain::ScriptStatus::FINISHED;
        doc["ok"] = fini;
        if (!fini) {
            doc["arret"] = AquaLook::Domain::scriptAbortName(vm.abortReason());
        } else {
            doc["entreeActive"] = vm.variable(0) != 0;
            doc["zoneActive"] = vm.variable(1) != 0;
            doc["resteSec"] = vm.variable(2);
        }
        doc["lectures"] = ctx.reads;
        doc["actions"] = ctx.actions;
        doc["refus"] = ctx.refusals;
        if (_schedule && _config) {
            const uint8_t z = _config->zoneIndexById(zoneId);
            if (z < MAX_ZONES) {
                doc["suspendue"] = _schedule->isZonePaused(z);
                doc["reliquatSec"] = _schedule->getPausedRemainingMs(z) / 1000UL;
            }
        }
        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // Etat de l'authentification. Volontairement lisible sans secret : dire
    // qu'une porte est verrouillee n'aide personne a l'ouvrir, et le cacher
    // empecherait l'interface d'expliquer pourquoi elle refuse.
    _server.on("/api/auth/state", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["configure"] = ApiAuth::hasSecret();
        doc["nonce"] = ApiAuth::lastNonce();
        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // ── Scripts de l'utilisateur ────────────────────────────────────────
    //
    // ATTENTION aux prefixes : ESPAsyncWebServer fait repondre un handler a
    // ses sous-chemins. "/api/scripts" capturait donc "/api/scripts/one" et
    // renvoyait la liste a la place du script demande. Les routes filles
    // portent un tiret pour n'avoir aucun prefixe commun -- plus sur que de
    // compter sur l'ordre d'enregistrement, qui se romprait au premier ajout.
    // La liste ne renvoie que les entetes : relire six bytecodes pour
    // afficher une liste couterait cher sans rien apporter.
    _server.on("/api/scripts", HTTP_GET, [this](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["max"] = ScriptStore::MAX_SCRIPTS;
        doc["tailleMax"] = ScriptStore::MAX_BYTECODE;
        JsonArray arr = doc["scripts"].to<JsonArray>();
        ScriptStore::Meta metas[ScriptStore::MAX_SCRIPTS];
        ScriptStore::loadAllMeta(metas, ScriptStore::MAX_SCRIPTS);
        for (uint8_t i = 0U; i < ScriptStore::MAX_SCRIPTS; ++i) {
            JsonObject o = arr.add<JsonObject>();
            o["i"] = i;
            o["utilise"] = metas[i].used;
            if (!metas[i].used) continue;
            o["nom"] = metas[i].name;
            o["actif"] = metas[i].enabled;
            o["declencheur"] = metas[i].trigger;
            o["entree"] = metas[i].triggerInputId;
            o["octets"] = metas[i].codeSize;
            if (_scripts) {
                o["encours"] = _scripts->isRunning(i);
                o["dernierArret"] = _scripts->lastAbort(i);
            }
        }
        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // Texte source d'un script, range sur la carte SD.
    //
    // Le module ne le lit JAMAIS pour agir -- il n'execute que le bytecode.
    // Le garder sert a une seule chose : que l'editeur retrouve ce que
    // l'utilisateur a ecrit, commentaires et mise en forme compris. Un
    // editeur qui rendrait un texte reconstitue a partir du bytecode
    // perdrait tout cela, et cesserait d'etre un editeur.
    _server.on("/api/script-source", HTTP_GET, [this](AsyncWebServerRequest* req) {
        const uint8_t i = req->hasParam("i")
            ? (uint8_t)req->getParam("i")->value().toInt() : 255U;
        if (i >= ScriptStore::MAX_SCRIPTS || !_storage) {
            req->send(404, "text/plain", "");
            return;
        }
        char path[32];
        snprintf(path, sizeof(path), "/scripts/s%u.txt", (unsigned)i);
        if (!_storage->isSdAvailable() || !_storage->existsOnSd(path)) {
            // Pas de source enregistree n'est pas une erreur : le script
            // tourne quand meme, seule l'edition est appauvrie.
            req->send(204, "text/plain", "");
            return;
        }
        FsFile f;
        if (!_storage->openRead(path, f)) { req->send(500, "text/plain", ""); return; }
        String text;
        text.reserve(2048);
        while (f.available()) text += (char)f.read();
        _storage->closeFile(f);
        req->send(200, "text/plain; charset=utf-8", text);
    });

    _server.on("/api/script-one", HTTP_GET, [](AsyncWebServerRequest* req) {
        const uint8_t i = req->hasParam("i")
            ? (uint8_t)req->getParam("i")->value().toInt() : 255U;
        ScriptStore::Meta meta;
        uint8_t code[ScriptStore::MAX_BYTECODE];
        JsonDocument doc;
        if (!ScriptStore::load(i, meta, code, sizeof(code))) {
            doc["ok"] = false;
            doc["error"] = "emplacement vide";
        } else {
            doc["ok"] = true;
            doc["i"] = i;
            doc["nom"] = meta.name;
            doc["actif"] = meta.enabled;
            doc["declencheur"] = meta.trigger;
            doc["entree"] = meta.triggerInputId;
            JsonArray c = doc["code"].to<JsonArray>();
            for (uint16_t k = 0U; k < meta.codeSize; ++k) c.add(code[k]);
        }
        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    _server.on("/api/debug/sd-selftest", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!_storage) { sendError(req, "stockage indisponible", 503); return; }
        _storage->runWriteSelfTest();
        sendOk(req);
    });

    _server.on("/api/captive", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleStartCaptive(req);
    });
    _server.on("/api/resetConfig", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleResetConfig(req);
    });
    _server.on("/api/relay/topology/persist", HTTP_POST,
               [this](AsyncWebServerRequest* req) { handlePersistTopology(req); });
    _server.on("/api/relay/topology/reset", HTTP_POST,
               [this](AsyncWebServerRequest* req) { handleResetTopology(req); });
    // Lecture d'une entree tout ou rien, par son identifiant stable.
    // Sert a verifier un cablage sans ecrire de script, et c'est la brique
    // que READ_INPUT utilisera cote machine a scripts.
    _server.on("/api/relay/input", HTTP_GET, [this](AsyncWebServerRequest* req) {
        const uint16_t id = req->hasParam("id")
            ? static_cast<uint16_t>(req->getParam("id")->value().toInt()) : 0U;
        JsonDocument doc;
        doc["id"] = id;
        bool stable = false, raw = false;
        // "active" est la valeur STABILISEE -- la seule sur laquelle un script
        // ait le droit de decider. "raw" n'est la que pour le diagnostic :
        // la publier n'autorise pas a s'en servir.
        const bool ok = _inputs && _inputs->read(id, stable);
        doc["ok"] = ok;
        if (ok) doc["active"] = stable;
        else doc["error"] = "entree inconnue, valeur pas encore stabilisee ou carte muette";
        if (_inputs && _inputs->readRaw(id, raw)) doc["raw"] = raw;
        if (_inputs) doc["transitions"] = _inputs->transitions(id);
        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    _server.on("/api/relay/topology", HTTP_GET,
               [this](AsyncWebServerRequest* req) { handleGetTopology(req); });
    addJsonHandler("/api/relay/topology",
                   [this](AsyncWebServerRequest* req, JsonVariant& jv) {
                       JsonDocument doc; doc.set(jv); handleSetTopology(req, doc);
                   });
    // Scan réseau WiFi — portail captif
    _server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleWifiScan(req);
    });

    // Journal d'événements — lien caché, diagnostic uniquement
    _server.on("/api/logs.txt", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleGetLogs(req);
    });
    _server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/api/logs.txt");
    });

    // Fichiers statiques LittleFS
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/status
// ═══════════════════════════════════════════════════════════════
void WebManager::handleStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;

    doc["time"]    = _ntp->getTimeStr();
    doc["synced"]  = _ntp->isSynced();
    doc["uptime"]  = millis() / 1000UL;
    doc["heap"]    = ESP.getFreeHeap();

    // Un seul indicateur pour le header Web : etat deja charge en memoire
    // par NotificationManager (aucune lecture NVS supplementaire ici, un
    // /api/status interroge frequemment ne doit pas ajouter d'allocations
    // repetees -- voir MaintenanceResultStore::loadRaw()).
    {
        const NotificationStatus notifStatus = NotificationManager::status();
        doc["updatePending"] = notifStatus.updateAvailable || notifStatus.webAssetsUpdateAvailable;
    }

    // Etat reel de la persistance de la configuration. La sauvegarde etant
    // differee (anti-usure flash), une reponse HTTP "ok" ne signifie que
    // "accepte", jamais "enregistre" : sans cette information, l'interface
    // laisserait croire qu'un reglage est acquis alors qu'il peut etre perdu
    // au prochain redemarrage. On dit ce qu'on sait, pas ce qui arrange.
    if (_config) {
        JsonObject save = doc["configSave"].to<JsonObject>();
        save["pending"] = _config->savePending();
        save["failed"]  = _config->saveFailed();
    }

    JsonObject weather = doc["weather"].to<JsonObject>();
    weather["rainExpected"] = _weather->isRainExpected();
    weather["rainMm"]       = _weather->getRainMm();
    weather["tempC"]        = _weather->getTempC();
    weather["status"]       = _weather->getStatusStr();
    weather["fetched"]      = _weather->hasFetched();

    // Prévisions J+0..J+4
    JsonArray forecast = doc["forecast"].to<JsonArray>();
    for (uint8_t i = 0; i < 5; i++) {
        ForecastDay fd = _weather->getForecastDay(i);
        JsonObject fo  = forecast.add<JsonObject>();
        fo["rainMm"]          = fd.rainMm;
        fo["tempMax"]         = fd.tempMax;
        fo["tempMin"]         = fd.tempMin;
        fo["feelsLikeMax"]    = fd.feelsLikeMax;
        fo["rainProbability"] = fd.rainProbability;
        fo["humidityMax"]     = fd.humidityMax;
        fo["windMaxKmh"]      = fd.windMaxKmh;
        fo["windDeg"]         = fd.windDeg;
        fo["gustMaxKmh"]      = fd.gustMaxKmh;
        fo["cloudsMax"]       = fd.cloudsMax;
        fo["pressureAvg"]     = fd.pressureAvg;
        fo["description"]     = fd.description;
        fo["icon"]            = fd.icon;
        fo["valid"]           = fd.valid;
    }

    // Zones
    JsonArray zones = doc["zones"].to<JsonArray>();
    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;
    for (uint8_t z = 0; z < nbZ; z++) {
        JsonObject zo  = zones.add<JsonObject>();
        zo["active"]   = _relais   ? _relais->getState(z)      : false;
        zo["elapsed"]  = _schedule ? _schedule->getElapsedMs(z) : 0;
        zo["remaining"]= _schedule ? _schedule->getRemainingMs(z): 0;
        zo["schedActive"] = _schedule ? _schedule->isZoneActive(z) : false;
        zo["reason"]   = _schedule ? _schedule->getLastReason(z).c_str() : "";
        // Une zone sans voie physique ne peut pas arroser, et le moteur le
        // sait -- mais l utilisateur, lui, ne voyait qu une zone qui refuse
        // de demarrer, sans explication. Le cas est reel : une carte 2 voies
        // pour 8 zones declarees. On expose donc l affectation, pour que
        // l ecran comme le web puissent la faire ressortir.
        const bool mapped = _relais.relay &&
            RelayTopology::resolveZoneValve(
                _relais.relay->topology(), z,
                _config ? _config->nbZones() : z + 1U).valid;
        zo["hasOutput"] = mapped;
        if (_config) zo["name"] = _config->zone(z).name;
        if (_config) zo["color"] = _config->zoneColor(z);
        if (_config) zo["id"] = _config->zoneId(z);
        if (_config) {
            const uint8_t notifyMask = _config->zoneNotificationMask(z);
            zo["notificationMask"] = notifyMask;
            zo["notifyStart"] = (notifyMask & ZONE_NOTIFY_START) != 0U;
            zo["notifyStop"] = (notifyMask & ZONE_NOTIFY_STOP) != 0U;
        }

        ZoneSchedule zs = _schedule->getZoneSchedule(z);

        zo["mode"]               = zs.mode;
        zo["intervalDays"]       = zs.intervalDays;
        zo["intervalAnchorDay"]  = zs.intervalAnchorDay;
        // Compatibilité avec le JavaScript existant pendant la transition.
        zo["lastWateredDay"]     = zs.intervalAnchorDay;

        JsonObject rain = zo["rain"].to<JsonObject>();
        rain["threshMm"] = zs.rain.thresholdMm;
        rain["hours"]    = zs.rain.forecastHours;

        // daySlots et intervalSlots exclus du status global (trop volumineux pour N zones)
        // Ils sont disponibles via GET /api/zone?z=N
    }

    doc["manualDurationMin"] = _schedule->getManualDurationMin();

    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/adminStatus — état système pour page ADMIN web
// ═══════════════════════════════════════════════════════════════
void WebManager::handleAdminStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;

    // Système
    doc["uptime"]   = millis() / 1000UL;
    doc["heap"]     = ESP.getFreeHeap();
    doc["heapMin"]  = ESP.getMinFreeHeap();
    doc["chipRev"]  = ESP.getChipRevision();

    // WiFi
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    if (_wifi) {
        wifi["state"] = _wifi->stateStr();
        wifi["rssi"]  = _wifi->getRssi();
        wifi["ip"]    = _wifi->isConnected()
                        ? _wifi->getIP().toString()
                        : (_wifi->isCaptivePortal()
                           ? _wifi->getApIP().toString()
                           : "");
        wifi["keepaliveHost"] = _wifi->keepaliveHost();
    }
    if (_config) wifi["ssid"] = _config->wifi().ssid;

    // NTP
    if (_config) {
        JsonObject ntp = doc["ntp"].to<JsonObject>();
        ntp["server"]    = _config->ntp().server;
        ntp["gmtOffset"] = _config->ntp().gmtOffset;
        ntp["dstOffset"] = _config->ntp().dstOffset;
        ntp["synced"]    = _ntp->isSynced();
        ntp["time"]      = _ntp->getTimeStr();
    }

    // Garde anti-boucle : etat toujours expose, meme au repos, pour que la
    // page puisse dire "tout va bien" plutot que de ne rien dire.
    {
        JsonObject bg = doc["bootGuard"].to<JsonObject>();
        bg["degraded"] = BootLoopGuard::isDegraded();
        bg["suspectBoots"] = BootLoopGuard::suspectBootCount();
        bg["threshold"] = BootLoopGuard::DEGRADED_THRESHOLD;
    }

    // Verification periodique des mises a jour
    if (_updateCheck != nullptr) {
        const UpdateCheckConfig& uc = _updateCheck->config();
        JsonObject chk = doc["updateCheck"].to<JsonObject>();
        chk["enabled"]      = uc.enabled;
        chk["hour"]         = uc.hour;
        chk["minute"]       = uc.minute;
        chk["intervalDays"] = uc.intervalDays;
        chk["lastCheckEpochDay"] = _updateCheck->lastCheckEpochDay();
    }

    // Synchronisation cloud
    if (_cloudSync != nullptr) {
        const CloudSyncConfig& cs = _cloudSync->config();
        JsonObject cloud = doc["cloudSync"].to<JsonObject>();
        cloud["enabled"] = cs.enabled;
        cloud["host"] = cs.host;
        cloud["port"] = cs.port;
        cloud["useHttps"] = cs.useHttps;
        cloud["moduleId"] = cs.moduleId;
        cloud["intervalMinutes"] = cs.intervalMinutes;
        // Presence seule, aucun caractere revele : l'ancien masque montrait les
        // 4 premiers, ce qui reduisait sans raison l'espace de recherche
        // (docs/ROBUSTESSE_RESEAU_2026-09-04.md, defaut n°6).
        cloud["tokenMasked"] = cs.token[0] ? "****" : "";
    }

    // OWM
    if (_config) {
        JsonObject owm = doc["owm"].to<JsonObject>();
        // Presence seule, aucun caractere revele (defaut n°6).
        const char* key = _config->owm().apiKey;
        owm["apiKeyMasked"] = key[0] ? "****" : "";
        owm["hasKey"]  = (key[0] != '\0');
        owm["lat"]     = _config->owm().lat;
        owm["lon"]     = _config->owm().lon;
        owm["units"]   = _config->owm().units;
        owm["city"]    = _config->owm().city;
        owm["country"] = _config->owm().country;
        owm["provider"] = _config->weatherProvider();
        owm["fetched"] = _weather->hasFetched();
    }

    // Système config
    if (_config) {
        JsonObject sys = doc["system"].to<JsonObject>();
        sys["maxWateringMin"]  = _config->system().maxWateringMin;
        sys["screenTimeout"]   = _config->system().screenTimeoutMin;
        sys["ledMode"]         = _config->system().ledMode;
        sys["nbZones"]         = _config->system().nbZones;
        sys["nbRelais"]        = _config->system().nbRelaisPhysical;
        sys["relayLogic"]      = _config->system().relayLogic;
        sys["relayController"] = _config->system().relayController;
        sys["maxZones"]        = (uint8_t)MAX_ZONES;
    }

    // Noms de zones
    if (_config) {
        JsonArray znames = doc["zoneNames"].to<JsonArray>();
        for (uint8_t z = 0; z < (_config ? _config->nbZones() : NB_ZONES); z++) {
            znames.add(_config->zone(z).name);
        }
    }

    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/diagnostics — instrumentation système légère
// ═══════════════════════════════════════════════════════════════
void WebManager::handleDiagnostics(AsyncWebServerRequest* req) {
    JsonDocument doc;
    SystemDiagnostics::fillJson(doc, _wifi);
    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  POST planning
// ═══════════════════════════════════════════════════════════════

void WebManager::handleSetMode(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    uint8_t mode = doc["mode"] | 255;
    if (zone >= MAX_ZONES || mode > SCHEDULE_MODE_INTERVAL) {
        sendError(req, "parametres invalides");
        return;
    }

    // Le changement de mode ne modifie jamais silencieusement l'ancre.
    _schedule->setMode(zone, mode);
    if (_config) _config->setZoneMode(zone, mode);

    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetInterval(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone = doc["zone"]     | 255;
    uint8_t days = doc["interval"] | 0;
    if (zone >= MAX_ZONES || days < 1 || days > 30) { sendError(req, "parametres invalides"); return; }
    _schedule->setIntervalDays(zone, days);
    if (_config) _config->setZoneIntervalDays(zone, days);
    sendOk(req);
}

void WebManager::handleSetIntervalAnchor(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    uint32_t anchorDay = doc["anchorDay"] | 0UL;
    if (zone >= MAX_ZONES || anchorDay == 0) {
        sendError(req, "date de debut invalide");
        return;
    }

    _schedule->setIntervalAnchorDay(zone, anchorDay);
    if (_config) _config->setZoneIntervalAnchorDay(zone, anchorDay);
    EventLog::log(LOG_INFO, "Zone %u: ancre intervalle=%lu",
                  zone + 1, (unsigned long)anchorDay);
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleDeleteIntervalProgramming(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    if (zone >= MAX_ZONES) {
        sendError(req, "zone invalide");
        return;
    }

    _schedule->clearIntervalProgramming(zone);
    if (_config) _config->clearZoneIntervalProgramming(zone);
    EventLog::log(LOG_INFO, "Zone %u: programmation intervalle supprimee", zone + 1);
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetDaySlot(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone    = doc["zone"]    | 255;
    uint8_t day     = doc["day"]     | 255;
    uint8_t slotIdx = doc["slotIdx"] | 255;
    if (zone >= MAX_ZONES || day >= NB_DAYS || slotIdx >= MAX_SLOTS) {
        sendError(req, "parametres invalides"); return;
    }
    uint8_t  h   = doc["hour"]     | 6;
    uint8_t  m   = doc["minute"]   | 0;
    uint16_t dur = doc["duration"] | 5;
    bool     en  = doc["enabled"]  | false;
    _schedule->setDaySlot(zone, day, slotIdx, h, m, dur, en);
    if (_config) _config->setZoneDaySlot(zone, day, slotIdx, h, m, dur, en);
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetIntervalSlot(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t  zone    = doc["zone"]    | 255;
    uint8_t  slotIdx = doc["slotIdx"] | 255;
    if (zone >= MAX_ZONES || slotIdx >= MAX_SLOTS) { sendError(req, "parametres invalides"); return; }
    uint8_t  h   = doc["hour"]     | 6;
    uint8_t  m   = doc["minute"]   | 0;
    uint16_t dur = doc["duration"] | 5;
    bool     en  = doc["enabled"]  | false;
    _schedule->setIntervalSlot(zone, slotIdx, h, m, dur, en);
    if (_config) _config->setZoneIntervalSlot(zone, slotIdx, h, m, dur, en);
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetRain(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    if (zone >= MAX_ZONES) { sendError(req, "zone invalide"); return; }
    float   thr  = doc["threshold"] | DEFAULT_RAIN_THRESHOLD;
    uint8_t hrs  = doc["hours"]     | DEFAULT_FORECAST_HOURS;
    _schedule->setRainConfig(zone, thr, hrs);
    if (_config) _config->setZoneRain(zone, thr, hrs);
    sendOk(req);
}

void WebManager::handleManual(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone  = doc["zone"]  | 255;
    bool    state = doc["state"] | false;
    if (zone >= MAX_ZONES) { sendError(req, "zone invalide"); return; }
    if (state) _schedule->startManualWatering(zone);
    else       _schedule->stopManualWatering(zone);
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetManualDuration(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    uint16_t min = doc["minutes"] | 0;
    if (min == 0 || min > 120) { sendError(req, "duree invalide (1-120)"); return; }

    // Ne jamais écrire LittleFS depuis le callback AsyncTCP.
    portENTER_CRITICAL(&_pendingMux);
    _pendingSystem = _config->system();
    _pendingManualDuration = min;
    _pendingManualDurationValid = true;
    _pendingSystemReboot = false;
    _systemSaveAtMs = millis() + 500;
    _systemSavePending = true;
    portEXIT_CRITICAL(&_pendingMux);

    sendOk(req);
}

void WebManager::handleSaveSchedule(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    uint8_t zone = doc["zone"] | 255;
    const uint8_t nbZs = _config ? _config->nbZones() : NB_ZONES;
    if (zone < nbZs) {
        _config->syncZoneFromSchedule(zone, _schedule->getZoneSchedule(zone));
    } else {
        for (uint8_t z = 0; z < nbZs; z++)
            _config->syncZoneFromSchedule(z, _schedule->getZoneSchedule(z));
    }
    sendOk(req);
}

// ═══════════════════════════════════════════════════════════════
//  POST config v2
// ═══════════════════════════════════════════════════════════════

void WebManager::handleSetWifi(AsyncWebServerRequest* req, JsonDocument& doc) {
    const char* ssid = doc["ssid"] | "";
    const char* pwd = doc["pwd"] | (doc["password"] | "");

    // Trim défensif — supprime espaces parasites en début/fin
    // (peuvent venir de l'autocomplete mobile ou du copier-coller)
    char ssidBuf[64]; strlcpy(ssidBuf, ssid, sizeof(ssidBuf));
    char pwdBuf[64];  strlcpy(pwdBuf,  pwd,  sizeof(pwdBuf));
    // Trim début
    auto trimStr = [](char* s) {
        char* start = s;
        while (*start == ' ') start++;
        if (start != s) memmove(s, start, strlen(start) + 1);
        // Trim fin
        int len = strlen(s);
        while (len > 0 && s[len-1] == ' ') s[--len] = '\0';
    };
    trimStr(ssidBuf);
    trimStr(pwdBuf);
    ssid = ssidBuf;
    pwd  = pwdBuf;

    if (strlen(ssid) == 0) { sendError(req, "ssid vide"); return; }

    if (!_config) {
        sendError(req, "config indisponible");
        EventLog::log(LOG_ERROR, "WiFi: ConfigManager absent — identifiants non sauvegardes");
        return;
    }

    sendOk(req);  // Invariant I10 : répondre AVANT le reboot
    EventLog::log(LOG_INFO, "WiFi: sauvegarde identifiants via NVS");
    _config->setWifi(ssid, pwd);  // inclut save() NVS + ESP.restart()
}

// Cible de la sonde de keepalive (voir WiFiManager::checkKeepaliveReachable).
// Remplie automatiquement avec la passerelle a la premiere connexion, mais
// modifiable ici — IP fictive pour tester la detection de connexion
// "zombie", ou plus tard un hote cloud. Chaine vide = desactive la sonde.
void WebManager::handleSetWifiKeepalive(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_wifi) { sendError(req, "wifi indisponible"); return; }

    const char* host = doc["host"] | "";
    if (strlen(host) >= 64) { sendError(req, "hote trop long"); return; }

    _wifi->setKeepaliveHost(host);
    sendOk(req);
}

// Voir la note sur DeployFileState (WebManager.h) et ROADMAP.md, "Mise a
// jour distante des ressources Web" — route de validation temporaire du
// chemin d'ecriture SD reseau, un fichier a la fois, nom simple uniquement.
void WebManager::handleDeployFileBody(
    AsyncWebServerRequest* req,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
) {
    if (index == 0) {
        auto* state = new DeployFileState();
        req->_tempObject = state;

        const String name = req->hasParam("name") ? req->getParam("name")->value() : "";
        const bool validName =
            name.length() > 0 && name.length() < 48 &&
            name.indexOf('/') < 0 && name.indexOf("..") < 0;

        if (!validName || !_storage) {
            state->openFailed = true;
            return;
        }

        // Ecrire dans le transit tant qu'un deploiement est ouvert : ne jamais
        // toucher /www directement, une interruption y laisserait un melange
        // incoherent d'anciens et de nouveaux fichiers (incident du 17 aout
        // 2026, ou /www a ete perdu par une ecriture interrompue).
        const String dir = _deployStagingOpen ? String("/www.new") : String("/www");
        state->tmpPath   = dir + "/." + name + ".deploytmp";
        state->finalPath = dir + "/" + name;

        if (!_storage->openWrite(state->tmpPath.c_str(), state->file)) {
            state->openFailed = true;
        }
    }

    auto* state = static_cast<DeployFileState*>(req->_tempObject);
    if (!state || state->openFailed || !_storage) return;

    const int32_t written = _storage->writeChunk(state->file, data, len);
    if (written != static_cast<int32_t>(len)) {
        state->openFailed = true;
    }
}

void WebManager::handleDeployFileComplete(AsyncWebServerRequest* req) {
    auto* state = static_cast<DeployFileState*>(req->_tempObject);
    req->_tempObject = nullptr;

    if (!state) {
        sendError(req, "requete invalide");
        return;
    }

    if (state->openFailed || !_storage) {
        if (state->file.isOpen()) _storage->closeFile(state->file);
        if (state->tmpPath.length()) _storage->deleteOnSd(state->tmpPath.c_str());
        delete state;
        sendError(req, "ecriture echouee");
        return;
    }

    _storage->closeFile(state->file);
    const bool renamed = _storage->renameOnSd(state->tmpPath.c_str(), state->finalPath.c_str());
    const String finalPath = state->finalPath;
    delete state;

    if (!renamed) {
        sendError(req, "renommage echoue");
        return;
    }

    EventLog::log(LOG_INFO, "Deploiement: fichier ecrit -> %s", finalPath.c_str());
    sendOk(req);
}

// Bascule les warnings "Timing: ..." (boucle lente, composants lents) —
// utiles en diagnostic, bavards une fois le point etudie resolu. Persiste
// (voir EventLog::setTimingLogsEnabled) : survit au redemarrage.
void WebManager::handleSetLogConfig(AsyncWebServerRequest* req, JsonDocument& doc) {
    const bool enabled = doc["timingLogsEnabled"] | true;
    EventLog::setTimingLogsEnabled(enabled);
    EventLog::log(
        LOG_INFO,
        "Config: logs Timing %s",
        enabled ? "actives" : "desactives"
    );
    sendOk(req);
}

// Voir la note sur handleVerifyWebAsset (WebManager.h) et ROADMAP.md, "Mise
// a jour distante des ressources Web" — etape 4 : telecharger et verifier
// un fichier sans l'ecrire. {url, size, sha256} passes manuellement pour
// l'instant, en attendant un declenchement pilote par le manifeste.
//
// N'execute PAS le telechargement ici : ce callback tourne sur la tache
// AsyncTCP, jamais sur la boucle principale, et l'operation doit pouvoir
// suspendre/reprendre un TFT_eSprite (voir DisplayManager::
// suspendForMemoryRelief(), reserve a la boucle principale). Depose juste
// la demande dans des champs fixes (jamais de String sous section critique)
// et repond immediatement ; /api/debug/verify-web-asset/status donne le
// resultat une fois WebManager::update() (loop principale) passe dessus.
void WebManager::handleVerifyWebAsset(AsyncWebServerRequest* req, JsonDocument& doc) {
    const char* url = doc["url"] | "";
    const uint32_t size = doc["size"] | 0U;
    const char* sha256 = doc["sha256"] | "";

    if (strlen(url) == 0U || strlen(url) >= VERIFY_URL_MAX ||
        size == 0U || strlen(sha256) != 64U) {
        sendError(req, "arguments invalides (url/size/sha256)");
        return;
    }

    // Refus UNIQUEMENT pendant un arrosage en cours. Le telechargement bloque
    // la boucle principale le temps du transfert (mesure sur materiel le 16
    // aout 2026 : 10,1 s pour 1,4 Mo), ce qui retarderait d'autant la commande
    // d'arret d'une voie ouverte — l'invariant OTA existant interdit de
    // retarder ou d'interrompre silencieusement un cycle d'arrosage.
    // Deliberement PAS de refus "par precaution" avant un creneau proche : une
    // mise a jour apporte des corrections et reste prioritaire hors arrosage
    // actif. Un creneau qui demarre pendant une verification deja lancee est
    // seulement decale de quelques secondes au demarrage (la duree du cycle
    // etant calculee depuis le demarrage reel, l'arret n'est jamais tronque),
    // et la verification sera terminee bien avant l'arret.
    // Meme controle que les routes /api/maintenance/* existantes.
    if (_config && _relais) {
        for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
            if (_relais->getState(zone)) {
                EventLog::log(LOG_WARN,
                              "WebAssets: verification refusee, zone %u en arrosage",
                              static_cast<unsigned>(zone + 1U));
                sendError(req, "arrosage en cours", 409);
                return;
            }
        }
    }

    portENTER_CRITICAL(&_pendingMux);
    if (_verifyPending || _verifyRunning) {
        portEXIT_CRITICAL(&_pendingMux);
        sendError(req, "verification deja en cours", 409);
        return;
    }
    std::strncpy(_verifyUrl, url, VERIFY_URL_MAX - 1U);
    _verifyUrl[VERIFY_URL_MAX - 1U] = '\0';
    _verifySize = size;
    std::strncpy(_verifySha256, sha256, sizeof(_verifySha256) - 1U);
    _verifySha256[sizeof(_verifySha256) - 1U] = '\0';
    _verifyResultReady = false;
    _verifyPending = true;
    portEXIT_CRITICAL(&_pendingMux);

    JsonDocument out;
    out["queued"] = true;
    sendJson(req, out, 202);
}

void WebManager::handleVerifyWebAssetStatus(AsyncWebServerRequest* req) {
    bool running, ready;
    WebAssetVerifyResult result;
    portENTER_CRITICAL(&_pendingMux);
    running = _verifyRunning || _verifyPending;
    ready = _verifyResultReady;
    result = _verifyResult;
    portEXIT_CRITICAL(&_pendingMux);

    JsonDocument out;
    out["running"] = running;
    out["ready"] = ready;
    if (ready) {
        out["ok"] = result.success;
        out["downloadedSize"] = result.downloadedSize;
        out["downloadDurationMs"] = result.downloadDurationMs;
        out["sha256"] = result.calculatedSha256;
        out["detail"] = result.detail;
    }
    sendJson(req, out);
}

// Appele depuis WebManager::update() (boucle principale, jamais depuis la
// tache AsyncTCP) : execute la demande deposee par handleVerifyWebAsset,
// suspend/reprend le sprite d'ecran autour du telechargement. Voir la note
// sur DisplayManager::suspendForMemoryRelief() pour la justification de
// securite (pourquoi ce n'est correct qu'ici, jamais dans le callback).
void WebManager::runPendingVerify() {
    char url[VERIFY_URL_MAX];
    uint32_t size;
    char sha256[65];

    portENTER_CRITICAL(&_pendingMux);
    if (!_verifyPending) {
        portEXIT_CRITICAL(&_pendingMux);
        return;
    }
    _verifyPending = false;
    _verifyRunning = true;
    std::strncpy(url, _verifyUrl, sizeof(url));
    size = _verifySize;
    std::strncpy(sha256, _verifySha256, sizeof(sha256));
    portEXIT_CRITICAL(&_pendingMux);

    EventLog::log(LOG_INFO, "WebAssets: verification demarree url=%s taille=%lu",
                  url, static_cast<unsigned long>(size));

    if (_display) _display->suspendForMemoryRelief();
    const WebAssetVerifyResult result = WebAssetsUpdater::verifyOnly(url, size, sha256);
    if (_display) _display->resumeAfterMemoryRelief();

    portENTER_CRITICAL(&_pendingMux);
    _verifyResult = result;
    _verifyResultReady = true;
    _verifyRunning = false;
    portEXIT_CRITICAL(&_pendingMux);
}

// Voir la note sur handleHeapInfo (WebManager.h) et ROADMAP.md, "constat du
// 16 aout 2026" : identifier precisement la fragmentation qui bloque le
// handshake TLS en fonctionnement normal, avant de tenter un correctif.
// heap_caps_print_heap_info() liste chaque bloc libre sur le port Serie
// (visible dans le journal), heap_caps_get_info() donne le resume expose ici.
void WebManager::handleHeapInfo(AsyncWebServerRequest* req) {
#if AQUALOOK_BOARD_S3
    // Cette route existe precisement pour parcourir le tas bloc par bloc -
    // c'est tout son interet sur la carte historique. Sur l'ESP32-S3 et ses
    // 8 Mo de PSRAM, ce meme parcours depasse le delai du chien de garde
    // d'interruption et fait REDEMARRER le module (voir HeapMetrics.h).
    // Repondre une explication vaut mieux qu'offrir un bouton qui plante.
    JsonDocument out;
    out["available"] = false;
    out["reason"] = "heap-walk-unsafe-on-s3";
    out["detail"] = "Parcours complet du tas desactive sur cette carte : "
                    "il declenche le watchdog d'interruption (voir "
                    "HeapMetrics.h). Utiliser /api/diagnostic.";
    String body;
    serializeJson(out, body);
    req->send(503, "application/json", body);
    return;
#else
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_8BIT);

    Serial.println("[HeapInfo] Dump detaille des blocs libres (MALLOC_CAP_8BIT) :");
    heap_caps_print_heap_info(MALLOC_CAP_8BIT);

    JsonDocument out;
    out["totalFreeBytes"] = info.total_free_bytes;
    out["totalAllocatedBytes"] = info.total_allocated_bytes;
    out["largestFreeBlock"] = info.largest_free_block;
    out["minimumFreeBytes"] = info.minimum_free_bytes;
    out["allocatedBlocks"] = info.allocated_blocks;
    out["freeBlocks"] = info.free_blocks;
    out["totalBlocks"] = info.total_blocks;
    sendJson(req, out);
#endif
}

// Diagnostic temporaire de saturation NVS — voir ROADMAP.md. Lecture seule :
// nvs_get_stats() pour le global, puis une ouverture en lecture seule de
// chaque namespace connu pour attribuer la consommation. Une entree NVS fait
// 32 octets ; un blob occupe des entrees supplementaires pour ses chunks.
void WebManager::handleNvsStats(AsyncWebServerRequest* req) {
    JsonDocument out;

    nvs_stats_t stats;
    if (nvs_get_stats(nullptr, &stats) == ESP_OK) {
        out["usedEntries"] = stats.used_entries;
        out["freeEntries"] = stats.free_entries;
        out["totalEntries"] = stats.total_entries;
        out["namespaceCount"] = stats.namespace_count;
        out["usedBytesApprox"] = stats.used_entries * 32U;
        out["totalBytesApprox"] = stats.total_entries * 32U;
    } else {
        out["error"] = "nvs_get_stats-failed";
    }

    // Liste exhaustive des namespaces du projet au 16 aout 2026. Un namespace
    // absent de NVS n'apparait simplement pas ouvert (aucune ecriture faite).
    static const char* const NAMESPACES[] = {
        "aqualook",      // ConfigManager (blob principal + ancres intervalle)
        "aq_log_cfg",    // EventLog (bascule logs Timing) — ajoute le 16/08/2026
        "aq_wifi_ka",    // WiFiManager (cible keepalive) — ajoute le 16/08/2026
        "aq_incidents",  // IncidentManager (incidents carte SD)
        "aq_maint",      // MaintenanceRequestStore
        "aq_maint_res",  // MaintenanceResultStore
        "aq_notify",     // NotificationManager
        "aq_ota_guard",  // OtaBootGuard
        "aq_upd_chk",    // UpdateCheckScheduler — ajoute le 17/08/2026
        "aq_boot",       // BootLoopGuard — ajoute le 17/08/2026
        "aq_cloud"       // CloudSyncScheduler — ajoute le 18/08/2026
    };

    JsonObject perNs = out["namespaces"].to<JsonObject>();
    for (const char* ns : NAMESPACES) {
        nvs_handle_t handle;
        if (nvs_open(ns, NVS_READONLY, &handle) != ESP_OK) {
            perNs[ns] = -1;  // namespace absent (jamais ecrit)
            continue;
        }
        size_t used = 0U;
        if (nvs_get_used_entry_count(handle, &used) == ESP_OK) {
            perNs[ns] = used;
        } else {
            perNs[ns] = -2;
        }
        nvs_close(handle);
    }

    sendJson(req, out);
}

void WebManager::handleSetTouch(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    _config->setTouchCalib(
        doc["xMin"] | (int)TOUCH_X_MIN,
        doc["xMax"] | (int)TOUCH_X_MAX,
        doc["yMin"] | (int)TOUCH_Y_MIN,
        doc["yMax"] | (int)TOUCH_Y_MAX
    );
    sendOk(req);
}

void WebManager::handleSetUpdateCheck(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_updateCheck) { sendError(req, "planificateur indisponible", 503); return; }

    const UpdateCheckConfig& current = _updateCheck->config();
    const bool enabled = doc["enabled"] | current.enabled;
    const uint8_t hour = doc["hour"] | current.hour;
    const uint8_t minute = doc["minute"] | current.minute;
    const uint8_t days = doc["intervalDays"] | current.intervalDays;

    // Bornes verifiees cote module et pas seulement dans la page : la page
    // n'est qu'un client parmi d'autres, et un reglage hors bornes fixerait
    // une echeance jamais atteinte, donc une verification qui n'aurait jamais
    // lieu — silencieusement.
    if (!_updateCheck->set(enabled, hour, minute, days)) {
        sendError(req, "heure (0-23), minute (0-59) ou intervalle (1-30 jours) hors bornes");
        return;
    }
    sendOk(req);
}

void WebManager::handleSetCloudSync(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_cloudSync) { sendError(req, "synchronisation cloud indisponible", 503); return; }

    const CloudSyncConfig& current = _cloudSync->config();
    const bool enabled = doc["enabled"] | current.enabled;
    const char* host = doc["host"] | current.host;
    const uint16_t port = doc["port"] | current.port;
    const bool useHttps = doc["useHttps"] | current.useHttps;
    const char* moduleId = doc["moduleId"] | current.moduleId;
    const char* token = doc["token"] | current.token;
    const uint16_t interval = doc["intervalMinutes"] | current.intervalMinutes;

    if (!_cloudSync->set(enabled, host, port, useHttps, moduleId, token, interval)) {
        // Trois causes possibles, et le message doit les couvrir toutes :
        // l'ajout du refus HTTPS a rendu l'ancien libelle trompeur, il
        // parlait d'hote et d'intervalle pour une erreur de transport.
        sendError(req, "hote requis si active, intervalle hors bornes (1-1440 min), "
                       "ou HTTP refuse vers un hote hors reseau local (HTTPS obligatoire)");
        return;
    }
    sendOk(req);
}

void WebManager::handleSetNtp(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    const char* server = doc["server"] | "pool.ntp.org";
    int32_t gmt = doc["gmtOffset"] | (int32_t)3600;
    int32_t dst = doc["dstOffset"] | (int32_t)3600;
    _config->setNtp(server, gmt, dst);
    // EventBus::configDirty positionné dans ConfigManager::setNtp()
    sendOk(req);
}

void WebManager::handleSetOwm(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    const char* apiKey  = doc["apiKey"]  | "";
    float lat           = doc["lat"]     | 0.0f;
    float lon           = doc["lon"]     | 0.0f;
    const char* units   = doc["units"]   | "metric";
    const char* city    = doc["city"]    | "";
    const char* country = doc["country"] | "FR";
    _config->setOwm(apiKey, lat, lon, units, city, country);
    // Absent du corps = inchange. La page des reglages meteo n'est pas la
    // seule a poster ici, et une valeur par defaut ramenerait silencieusement
    // le fournisseur a OpenWeatherMap.
    if (doc["provider"].is<uint8_t>()) {
        _config->setWeatherProvider(doc["provider"].as<uint8_t>());
    }
    sendOk(req);
}

void WebManager::handleSetSystem(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }

    // Construire une copie complète, appliquer les seuls champs reçus,
    // puis effectuer UNE sauvegarde LittleFS. Les anciens setters provoquaient
    // jusqu'à trois écritures successives pendant une bascule de zones.
    CfgSystem next = _config->system();

    if (doc["maxWateringMin"].is<uint16_t>() || doc["maxWateringMin"].is<int>()) {
        next.maxWateringMin = doc["maxWateringMin"] | (uint16_t)60;
    }
    if (doc["screenTimeout"].is<uint8_t>() || doc["screenTimeout"].is<int>()) {
        next.screenTimeoutMin = doc["screenTimeout"] | (uint8_t)5;
    }
    if (doc["ledMode"].is<uint8_t>() || doc["ledMode"].is<int>()) {
        next.ledMode = constrain((uint8_t)(doc["ledMode"] | 1), (uint8_t)0, (uint8_t)4);
    }
    if (doc["relayLogic"].is<uint8_t>() || doc["relayLogic"].is<int>()) {
        next.relayLogic = constrain((uint8_t)(doc["relayLogic"] | 1), (uint8_t)0, (uint8_t)1);
    }
    if (doc["relayController"].is<uint8_t>() || doc["relayController"].is<int>()) {
        next.relayController = constrain((uint8_t)(doc["relayController"] | RELAY_CONTROLLER_XL9535),
                                         (uint8_t)RELAY_CONTROLLER_XL9535,
                                         (uint8_t)RELAY_CONTROLLER_MCP23017);
    }

    const uint8_t oldNbZones = _config->nbZones();
    if (doc["nbZones"].is<uint8_t>() || doc["nbZones"].is<int>()) {
        // Le nombre de zones est une notion LOGIQUE : combien de zones
        // l'utilisateur veut piloter. Il etait arrondi au pair superieur sur
        // XL9535 (5 devenait 6) parce que le cablage etait DEDUIT et supposait
        // une carte dont les sorties allaient par paires. Le cablage etant
        // desormais decrit explicitement -- le banc porte deux cartes de deux
        // voies -- cette supposition n'a plus lieu d'etre, et arrondir en
        // silence donnait a l'utilisateur une valeur qu'il n'avait pas
        // demandee. Retire le 7 septembre 2026.
        next.nbZones = constrain((uint8_t)(doc["nbZones"] | oldNbZones),
                                 (uint8_t)1, (uint8_t)MAX_ACTIVE_ZONES);
    }

    // Invariant matériel AquaLook : une zone correspond exactement à une sortie relais.
    next.nbRelaisPhysical = next.nbZones;

    const bool needReboot = (next.nbZones != oldNbZones) ||
                            (next.relayController != _config->system().relayController) ||
                            (next.relayLogic != _config->system().relayLogic);

    uint16_t manualDuration = _config->manual().durationMin;
    bool manualDurationValid = false;
    if (doc["manualDurationMin"].is<uint16_t>() || doc["manualDurationMin"].is<int>()) {
        manualDuration = constrain((uint16_t)(doc["manualDurationMin"] | manualDuration),
                                   (uint16_t)1, (uint16_t)120);
        manualDurationValid = true;
    }

    // Ne jamais écrire LittleFS depuis le callback AsyncTCP. On copie la
    // demande puis on répond immédiatement. update() effectuera la sauvegarde
    // dans la tâche Arduino, après que la réponse HTTP a quitté la pile réseau.
    portENTER_CRITICAL(&_pendingMux);
    _pendingSystem = next;
    _pendingManualDuration = manualDuration;
    _pendingManualDurationValid = manualDurationValid;
    _pendingSystemReboot = needReboot;
    _systemSaveAtMs = millis() + 500;
    _systemSavePending = true;
    portEXIT_CRITICAL(&_pendingMux);

    sendOk(req);
}

void WebManager::handleSetApiSecret(AsyncWebServerRequest* req, JsonDocument& doc) {
    const char* actuel = doc["actuel"] | "";
    const char* nouveau = doc["nouveau"] | "";
    if (!ApiAuth::setSecret(actuel, nouveau)) {
        sendError(req, ApiAuth::hasSecret()
            ? "secret actuel incorrect, ou nouveau secret trop court (12 caracteres minimum)"
            : "secret trop court (12 caracteres minimum)");
        return;
    }
    sendOk(req);
}

// Enregistrement d'un script compile par le navigateur.
//
// Le bytecode arrive du dehors : ScriptStore::save le fait valider avant
// d'ecrire, et un refus laisse INTACT le programme precedent. Refuser sans
// detruire est la moindre des choses quand quelqu'un vient de taper vingt
// lignes.
void WebManager::handleSaveScript(AsyncWebServerRequest* req, JsonDocument& doc) {
    const uint8_t index = doc["i"] | 255U;
    JsonArrayConst code = doc["code"].as<JsonArrayConst>();
    if (index >= ScriptStore::MAX_SCRIPTS) { sendError(req, "emplacement invalide"); return; }

    // Signature AVANT tout traitement : on ne touche pas au magasin, ni meme
    // aux tampons, pour une requete dont on ne sait pas d'ou elle vient.
    //
    // Le message signe est une forme CANONIQUE reconstruite ici a partir des
    // valeurs qui comptent -- pas le corps HTTP brut, dont la
    // re-serialisation ne redonnerait pas les memes octets.
    {
        String canonical = "script-save|";
        canonical += index;
        canonical += '|';
        canonical += (uint32_t)(doc["nonce"] | 0U);
        canonical += '|';
        for (JsonVariantConst v : code) {
            const int b = v | 0;
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", b & 0xFF);
            canonical += hex;
        }
        if (!ApiAuth::verify(canonical, doc["nonce"] | 0U, doc["sig"] | "")) {
            sendError(req, "signature refusee : script non enregistre", 403);
            return;
        }
    }
    if (code.isNull() || code.size() == 0U) { sendError(req, "programme vide"); return; }
    if (code.size() > ScriptStore::MAX_BYTECODE) {
        sendError(req, "programme trop long");
        return;
    }

    uint8_t bytes[ScriptStore::MAX_BYTECODE];
    uint16_t n = 0U;
    for (JsonVariantConst v : code) {
        const int value = v | -1;
        if (value < 0 || value > 255) { sendError(req, "octet invalide"); return; }
        bytes[n++] = static_cast<uint8_t>(value);
    }

    ScriptStore::Meta meta;
    meta.used = true;
    meta.enabled = doc["actif"] | true;
    meta.trigger = doc["declencheur"] | ScriptStore::TRIGGER_INPUT_CHANGE;
    meta.triggerInputId = doc["entree"] | 0U;
    meta.codeSize = n;
    strlcpy(meta.name, doc["nom"] | "sans nom", sizeof(meta.name));

    const char* reason = "";
    if (!ScriptStore::save(index, meta, bytes, reason)) {
        sendError(req, reason);
        return;
    }

    // Le source suit le bytecode, jamais l'inverse : si l'ecriture SD echoue,
    // le script tourne quand meme. On le signale sans faire echouer
    // l'enregistrement -- perdre le confort d'edition n'est pas perdre la
    // regle.
    bool sourceSaved = false;
    const char* source = doc["source"] | "";
    if (_storage && _storage->isSdAvailable() && source[0] != ' ') {
        // openWrite cree deja le repertoire parent si besoin.
        char path[32];
        snprintf(path, sizeof(path), "/scripts/s%u.txt", (unsigned)index);
        FsFile f;
        if (_storage->openWrite(path, f)) {
            const size_t len = strlen(source);
            sourceSaved = _storage->writeChunk(
                f, reinterpret_cast<const uint8_t*>(source), len) == (int32_t)len;
            _storage->closeFile(f);
        }
        if (!sourceSaved) {
            EventLog::log(LOG_WARN,
                          "Scripts: source %u non enregistree, le programme tourne quand meme",
                          (unsigned)index);
        }
    }
    JsonDocument out;
    out["ok"] = true;
    out["octets"] = n;
    String body;
    serializeJson(out, body);
    req->send(200, "application/json", body);
}

void WebManager::handleEraseScript(AsyncWebServerRequest* req, JsonDocument& doc) {
    const uint8_t index = doc["i"] | 255U;
    {
        String canonical = "script-erase|";
        canonical += index;
        canonical += '|';
        canonical += (uint32_t)(doc["nonce"] | 0U);
        if (!ApiAuth::verify(canonical, doc["nonce"] | 0U, doc["sig"] | "")) {
            sendError(req, "signature refusee : rien n'a ete efface", 403);
            return;
        }
    }
    if (!ScriptStore::erase(index)) { sendError(req, "effacement impossible"); return; }
    sendOk(req);
}

// Lancement a la demande, pour essayer un script.
//
// Signe comme l'enregistrement : lancer un script, c'est commander des
// vannes. Une route d'essai non protegee serait une porte derobee vers
// exactement ce que la signature protege.
void WebManager::handleRunScript(AsyncWebServerRequest* req, JsonDocument& doc) {
    const uint8_t index = doc["i"] | 255U;
    {
        String canonical = "script-run|";
        canonical += index;
        canonical += '|';
        canonical += (uint32_t)(doc["nonce"] | 0U);
        if (!ApiAuth::verify(canonical, doc["nonce"] | 0U, doc["sig"] | "")) {
            sendError(req, "signature refusee : rien n'a ete lance", 403);
            return;
        }
    }
    if (!_scripts) { sendError(req, "executeur indisponible", 503); return; }
    const char* reason = "";
    if (!_scripts->runNow(index, reason)) { sendError(req, reason); return; }
    sendOk(req);
}

void WebManager::handleSetZoneName(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    uint8_t     zone  = doc["zone"] | 255;
    const char* name  = doc["name"] | "";
    // La couleur voyage avec le nom : les deux sont l'identite de la zone, et
    // se saisissent sur le meme ecran. Absente, elle est simplement laissee
    // telle quelle -- un client qui ne connait pas ce champ ne l'efface pas.
    const char* color = doc["color"] | "";
    if (zone >= MAX_ZONES || strlen(name) == 0) { sendError(req, "parametres invalides"); return; }
    _config->setZoneName(zone, name);
    if (color[0] == '#') _config->setZoneColor(zone, color);
    // EventBus::displayDirty positionné dans ConfigManager::setZoneName()
    sendOk(req);
}

// Source des mises a jour des ressources Web.
//
// Configurable parce qu'un module installe sur site n'est plus joignable que
// par le WiFi : figer la source dans le firmware imposerait de le reflasher,
// donc de le demonter, pour changer de canal.
//
// HTTPS impose - voir ConfigManager::setWebAssetsUrl() pour le raisonnement :
// le manifeste porte les SHA-256 qui authentifient les fichiers, donc servi
// en clair il permettrait d'en substituer un autre et la verification par
// hash validerait l'attaque au lieu de l'empecher.
void WebManager::handleSetWebAssetsUrl(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    const char* url = doc["url"] | "";
    if (strlen(url) == 0) { sendError(req, "url requise"); return; }
    if (!_config->setWebAssetsUrl(url)) {
        sendError(req, "url refusee : https:// obligatoire, 159 caracteres max");
        return;
    }
    sendOk(req);
}

// Identification physique d'une zone : fait clignoter sa LED en blanc sur
// le ruban WS2812, pour ne pas se tromper au branchement. Ne touche NI aux
// relais NI a la configuration - c'est un simple retour visuel temporaire,
// sans effet de bord sur l'arrosage.
void WebManager::handleZoneIdentify(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_display) { sendError(req, "affichage indisponible"); return; }

    const uint8_t zone = doc["zone"] | 255U;
    // Duree bornee : une identification oubliee masquerait l'etat reel de la
    // zone. 3 a 120 s, 20 s par defaut - le temps de reperer une LED.
    uint16_t seconds = doc["seconds"] | 20U;
    if (seconds < 3U)   seconds = 3U;
    if (seconds > 120U) seconds = 120U;

    // zone == 255 est le code d'annulation, volontairement accepte : il faut
    // pouvoir eteindre l'identification sans attendre son echeance.
    if (zone != 255U && _config && zone >= _config->nbZones()) {
        sendError(req, "zone invalide");
        return;
    }
    _display->identifyZone(zone, (uint32_t)seconds * 1000UL);
    sendOk(req);
}

void WebManager::handleSetZoneNotifications(AsyncWebServerRequest* req,
                                             JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    const uint8_t zone = doc["zone"] | 255U;
    if (zone >= _config->nbZones()) { sendError(req, "zone invalide"); return; }
    uint8_t mask = 0U;
    if (doc["notifyStart"] | false) mask |= ZONE_NOTIFY_START;
    if (doc["notifyStop"] | false) mask |= ZONE_NOTIFY_STOP;
    _config->setZoneNotificationMask(zone, mask);
    sendOk(req);
}

void WebManager::handleStartCaptive(AsyncWebServerRequest* req) {
    sendOk(req);
    EventBus::captiveRequested = true;  // WiFiManager le consomme dans update()
}

// Lecture du cablage en vigueur. "source" ne peut plus valoir que "nvs"
// (cablage enregistre) ou "absent" (module jamais cable) : la deduction de
// secours a ete retiree le 7 septembre 2026. "wired" dit si une carte valide
// existe -- c'est ce que l'interface utilise pour guider vers la page de
// cablage plutot que d'afficher des zones qui ne peuvent pas arroser.
void WebManager::handleGetTopology(AsyncWebServerRequest* req) {
    if (!_relais.relay) { sendError(req, "indisponible"); return; }
    const RelayTopology::RelayTopologyConfig& topo = _relais.relay->topology();

    JsonDocument doc;
    doc["source"] = _relais.relay->topologyFromStore() ? "nvs" : "absent";
    doc["wired"] = _relais.relay->isWired();
    doc["persisted"] = RelayTopologyStore::exists();

    JsonArray boards = doc["boards"].to<JsonArray>();
    for (uint8_t b = 0; b < RelayTopology::MAX_RELAY_BOARDS; ++b) {
        const RelayTopology::RelayBoardConfig& bd = topo.boards[b];
        if (!bd.enabled) continue;
        JsonObject o = boards.add<JsonObject>();
        o["i"] = b;
        o["controller"] = bd.controller;
        o["name"] = RelayTopology::controllerName(bd.controller);
        o["addr"] = bd.i2cAddress;
        o["channels"] = bd.channelCount;
        o["logic"] = bd.logic;
        o["transport"] = bd.transport;
        o["node"] = bd.node;
    }

    JsonArray asg = doc["assignments"].to<JsonArray>();
    for (uint8_t a = 0; a < RelayTopology::MAX_RELAY_ASSIGNMENTS; ++a) {
        const RelayTopology::RelayAssignment& as = topo.assignments[a];
        if (!as.enabled) continue;
        JsonObject o = asg.add<JsonObject>();
        o["i"] = a;
        o["role"] = as.role;
        o["roleName"] = RelayTopology::roleName(as.role);
        o["target"] = as.targetIndex;
        o["board"] = as.boardIndex;
        o["channel"] = as.channelIndex;
        o["direction"] = as.direction;
        o["input"] = as.isInput();
        o["flags"] = as.flags;
        o["id"] = as.id;
    }
    sendJson(req, doc);
}

// Ecriture d'une topologie arbitraire. Validee poste par poste, refusee
// pendant un arrosage (recabler les voies a chaud laisserait un relais dans
// un etat incoherent), et appliquee au prochain demarrage.
void WebManager::handleSetTopology(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config || !_relais.relay) { sendError(req, "indisponible"); return; }

    for (uint8_t z = 0; z < _config->nbZones(); ++z) {
        if (_relais.relay->getState(z)) { sendError(req, "arrosage en cours"); return; }
    }

    RelayTopology::RelayTopologyConfig topo;
    RelayTopology::clear(topo);

    for (JsonObjectConst o : doc["boards"].as<JsonArrayConst>()) {
        const uint8_t i = o["i"] | 255;
        if (i >= RelayTopology::MAX_RELAY_BOARDS) { sendError(req, "carte invalide"); return; }
        RelayTopology::RelayBoardConfig& bd = topo.boards[i];
        bd.enabled = true;
        bd.controller = o["controller"] | RelayTopology::CONTROLLER_XL9535;
        bd.i2cAddress = o["addr"] | RelayTopology::defaultAddressForController(bd.controller);
        bd.channelCount = o["channels"] | 8;
        bd.logic = o["logic"] | RelayTopology::LOGIC_DIRECT;
        bd.transport = o["transport"] | RelayTopology::TRANSPORT_I2C_LOCAL;
        bd.node = o["node"] | 0;
        if (!RelayTopology::isSupportedTransport(bd.transport)) {
            sendError(req, "transport sans pilote"); return;
        }
        if (!RelayTopology::validateBoard(bd)) { sendError(req, "carte invalide"); return; }
    }

    for (JsonObjectConst o : doc["assignments"].as<JsonArrayConst>()) {
        const uint8_t i = o["i"] | 255;
        if (i >= RelayTopology::MAX_RELAY_ASSIGNMENTS) { sendError(req, "affectation invalide"); return; }
        RelayTopology::RelayAssignment& as = topo.assignments[i];
        as.enabled = true;
        as.role = o["role"] | RelayTopology::ROLE_UNUSED;
        as.targetIndex = o["target"] | 0;
        as.boardIndex = o["board"] | 0;
        as.channelIndex = o["channel"] | 0;
        // Le sens se DEDUIT du role plutot que d'etre saisi separement : deux
        // champs qui doivent s'accorder finissent toujours par diverger, et
        // c'est l'utilisateur qui paye la contradiction.
        as.direction = RelayTopology::isInputRole(as.role)
            ? RelayTopology::DIRECTION_INPUT
            : RelayTopology::DIRECTION_OUTPUT;
        as.flags = o["flags"] | 0;
        // Une entree sans identifiant serait invisible aux scripts. A defaut
        // d'un identifiant fourni, on en attribue un stable et unique.
        as.id = o["id"] | 0;
        if (as.id == 0U) as.id = static_cast<uint16_t>(i + 1U);
        if (!RelayTopology::isSupportedRole(as.role)) { sendError(req, "role invalide"); return; }
    }

    for (uint8_t i = 0; i < RelayTopology::MAX_RELAY_ASSIGNMENTS; ++i) {
        if (!topo.assignments[i].enabled) continue;
        if (!RelayTopology::validateAssignment(topo, i)) {
            sendError(req, "affectation incoherente"); return;
        }
    }

    if (!RelayTopologyStore::save(topo, _config->nbZones())) {
        sendError(req, "topologie refusee"); return;
    }

    JsonDocument out;
    out["ok"] = true;
    out["applied"] = "reboot";  // pas de recablage a chaud
    sendJson(req, out);
}

// Enregistre le cablage actuellement en memoire. N'avait d'interet que pour
// figer une topologie DERIVEE ; la deduction ayant ete retiree, l'interface
// n'expose plus ce bouton. La route est conservee : elle reste un moyen
// legitime de figer un cablage construit par un autre chemin.
void WebManager::handlePersistTopology(AsyncWebServerRequest* req) {
    if (!_config || !_relais.relay) { sendError(req, "indisponible"); return; }
    if (!RelayTopologyStore::save(_relais.relay->topology(), _config->nbZones())) {
        sendError(req, "topologie refusee");
        return;
    }
    sendOk(req);
}

// Efface le cablage persiste. Il n'y a plus de deduction derriere : au
// prochain demarrage le module ne pilotera plus aucune sortie tant qu'un
// nouveau cablage n'aura pas ete decrit.
void WebManager::handleResetTopology(AsyncWebServerRequest* req) {
    RelayTopologyStore::clear();
    sendOk(req);
}

void WebManager::handleResetConfig(AsyncWebServerRequest* req) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    sendOk(req);
    _config->resetPersistent();
    delay(200);
    BootLoopGuard::restartDeliberately("remise a zero de la configuration");
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/display — retourne les tokens de design LCD courants
// ═══════════════════════════════════════════════════════════════
void WebManager::handleGetDisplay(AsyncWebServerRequest* req) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    const CfgDisplay& d = _config->display();
    JsonDocument doc;
    // Couleurs
    doc["cBg"]        = d.cBg;       doc["cSurface"]   = d.cSurface;
    doc["cSurface2"]  = d.cSurface2; doc["cBorder"]    = d.cBorder;
    doc["cText"]      = d.cText;     doc["cText2"]     = d.cText2;
    doc["cMuted"]     = d.cMuted;    doc["cActiveBg"]  = d.cActiveBg;
    doc["cZone0"]     = d.cZone0;    doc["cZone1"]     = d.cZone1;
    doc["cZone2"]     = d.cZone2;    doc["cZone3"]     = d.cZone3;
    // Formes
    doc["rSm"]        = d.rSm;       doc["rMd"]        = d.rMd;
    doc["rLg"]        = d.rLg;       doc["accentBarW"] = d.accentBarW;
    // Timing
    doc["refreshNomMs"] = d.refreshNomMs;
    doc["refreshActMs"] = d.refreshActMs;
    // Layout
    doc["planGap"]    = d.planGap;
    doc["windGustAlertKmh"] = _config->windAlert().gustKmh;
    doc["windSevereKmh"]    = _config->windAlert().severeKmh;
    // Source des mises a jour Web, pour que l'interface montre d'ou le
    // module se met a jour - une information a verifier avant de laisser
    // un module partir sur site.
    doc["webAssetsUrl"]     = _config->webAssetsUrl();
    doc["g2Gpad"]     = d.g2Gpad;
    doc["g4Gpad"]     = d.g4Gpad;
    // Options météo LCD
    doc["showWeatherIcon"] = d.showWeatherIcon;
    doc["showWeatherTemp"] = d.showWeatherTemp;
    doc["weatherVisualsEnabled"] = _config->weatherVisualsEnabled();
    doc["weatherTipCondition"] = d.weatherTipCondition;
    doc["weatherTipTemp"]      = d.weatherTipTemp;
    doc["weatherTipRain"]      = d.weatherTipRain;
    doc["weatherTipPop"]       = d.weatherTipPop;
    doc["weatherTipHumidity"]  = d.weatherTipHumidity;
    doc["weatherTipWind"]      = d.weatherTipWind;
    doc["weatherTipGust"]      = d.weatherTipGust;
    doc["weatherTipClouds"]    = d.weatherTipClouds;
    doc["weatherTipPressure"]  = d.weatherTipPressure;
    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  POST /api/display — met à jour les tokens de design LCD
//  Hot-reload : aucun reboot — EventBus::displayDirty déclenche
//  applyDisplayConfig() au prochain cycle DisplayManager::update().
// ═══════════════════════════════════════════════════════════════
void WebManager::handleSetDisplay(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }

    // Partir de la config courante pour ne patcher que les champs présents
    CfgDisplay d = _config->display();

    // Helper de validation couleur — accepte uniquement #rrggbb (7 chars)
    auto setColor = [](const char* src, char* dst) {
        if (src && src[0] == '#' && strlen(src) == 7) strlcpy(dst, src, 8);
    };
    setColor(doc["cBg"]       | "", d.cBg);
    setColor(doc["cSurface"]  | "", d.cSurface);
    setColor(doc["cSurface2"] | "", d.cSurface2);
    setColor(doc["cBorder"]   | "", d.cBorder);
    setColor(doc["cText"]     | "", d.cText);
    setColor(doc["cText2"]    | "", d.cText2);
    setColor(doc["cMuted"]    | "", d.cMuted);
    setColor(doc["cActiveBg"] | "", d.cActiveBg);
    setColor(doc["cZone0"]    | "", d.cZone0);
    setColor(doc["cZone1"]    | "", d.cZone1);
    setColor(doc["cZone2"]    | "", d.cZone2);
    setColor(doc["cZone3"]    | "", d.cZone3);

    if (doc["rSm"].is<int>())        d.rSm        = constrain((uint8_t)(doc["rSm"]  | 4),  1, 20);
    if (doc["rMd"].is<int>())        d.rMd        = constrain((uint8_t)(doc["rMd"]  | 6),  1, 20);
    if (doc["rLg"].is<int>())        d.rLg        = constrain((uint8_t)(doc["rLg"]  | 10), 1, 30);
    if (doc["accentBarW"].is<int>()) d.accentBarW = constrain((uint8_t)(doc["accentBarW"] | 3), 1, 8);

    if (doc["refreshNomMs"].is<int>()) {
        uint16_t v = doc["refreshNomMs"] | (uint16_t)5000;
        d.refreshNomMs = constrain(v, (uint16_t)500, (uint16_t)30000);
    }
    if (doc["refreshActMs"].is<int>()) {
        uint16_t v = doc["refreshActMs"] | (uint16_t)1000;
        d.refreshActMs = constrain(v, (uint16_t)200, (uint16_t)5000);
    }
    if (doc["planGap"].is<int>()) d.planGap = constrain((uint8_t)(doc["planGap"] | 6), (uint8_t)0, (uint8_t)20);
    if (doc["windGustAlertKmh"].is<int>() || doc["windSevereKmh"].is<int>()) {
        CfgWindAlert w = _config->windAlert();
        if (doc["windGustAlertKmh"].is<int>()) w.gustKmh   = (uint8_t)(doc["windGustAlertKmh"] | 30);
        if (doc["windSevereKmh"].is<int>())    w.severeKmh = (uint8_t)(doc["windSevereKmh"] | 50);
        _config->setWindAlert(w);
    }
    if (doc["g2Gpad"].is<int>())  d.g2Gpad  = constrain((uint8_t)(doc["g2Gpad"]  | 1), (uint8_t)0, (uint8_t)8);
    if (doc["g4Gpad"].is<int>())  d.g4Gpad  = constrain((uint8_t)(doc["g4Gpad"]  | 1), (uint8_t)0, (uint8_t)8);
    if (doc["showWeatherIcon"].is<bool>()) d.showWeatherIcon = doc["showWeatherIcon"];
    if (doc["showWeatherTemp"].is<bool>()) d.showWeatherTemp = doc["showWeatherTemp"];
    if (doc["weatherVisualsEnabled"].is<bool>())
        _config->setWeatherVisualsEnabled(doc["weatherVisualsEnabled"]);
    if (doc["weatherTipCondition"].is<bool>()) d.weatherTipCondition = doc["weatherTipCondition"];
    if (doc["weatherTipTemp"].is<bool>())      d.weatherTipTemp      = doc["weatherTipTemp"];
    if (doc["weatherTipRain"].is<bool>())      d.weatherTipRain      = doc["weatherTipRain"];
    if (doc["weatherTipPop"].is<bool>())       d.weatherTipPop       = doc["weatherTipPop"];
    if (doc["weatherTipHumidity"].is<bool>())  d.weatherTipHumidity  = doc["weatherTipHumidity"];
    if (doc["weatherTipWind"].is<bool>())      d.weatherTipWind      = doc["weatherTipWind"];
    if (doc["weatherTipGust"].is<bool>())      d.weatherTipGust      = doc["weatherTipGust"];
    if (doc["weatherTipClouds"].is<bool>())    d.weatherTipClouds    = doc["weatherTipClouds"];
    if (doc["weatherTipPressure"].is<bool>())  d.weatherTipPressure  = doc["weatherTipPressure"];

    _config->setDisplay(d);   // save() + EventBus::displayDirty = true
    sendOk(req);
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/wifi/scan
//
//  Comportement en 2 temps :
//    1er appel  → lance le scan asynchrone, répond {scanning:true}
//    Appels suivants tant que scan en cours → {scanning:true}
//    Quand terminé → répond la liste triée par RSSI, dédoublonnée,
//                    puis libère la mémoire (clearScan)
//
//  Format réponse finale :
//    { scanning:false, networks:[{ssid,rssi,secured}, ...] }
//
//  Le client doit poller /api/wifi/scan toutes les ~500 ms
//  jusqu'à recevoir scanning:false.
//
//  Le scan est lancé une seule fois, dès l'activation du portail captif
//  (WiFiManager::startCaptivePortal), avant qu'un client ne s'y connecte —
//  scanner pendant qu'un client est déjà associé au point d'accès le
//  déconnecte brièvement (une seule radio, changement de canal). Cette
//  route ne fait donc que lire le résultat déjà prêt ou en cours ; elle ne
//  relance jamais de scan elle-même, et ne l'efface pas après lecture afin
//  qu'un rechargement de page renvoie la même liste sans redéclencher de
//  scan. Un nouveau scan nécessite un redémarrage du module.
// ═══════════════════════════════════════════════════════════════
void WebManager::handleWifiScan(AsyncWebServerRequest* req) {
    if (!_wifi) { sendError(req, "wifi indisponible"); return; }

    int16_t n = _wifi->getScanCount();

    if (n < 0) {
        // Scan en cours (lancé au démarrage du portail captif)
        req->send(200, "application/json", "{\"scanning\":true}");
        return;
    }

    // n == 0 et scan non lancé (getScanCount retourne 0 si !_scanPending) :
    // filet de sécurité seulement, ne devrait pas arriver en usage normal
    // puisque le scan est déjà lancé à l'activation du portail captif.
    if (n == 0) {
        _wifi->startScan();
        req->send(200, "application/json", "{\"scanning\":true}");
        return;
    }

    // Scan terminé — construire la réponse dédoublonnée, triée par RSSI
    JsonDocument doc;
    doc["scanning"] = false;
    JsonArray networks = doc["networks"].to<JsonArray>();

    // Tri par RSSI décroissant + dédoublonnage SSID
    // On utilise un tableau d'indices triés (N est petit, bubble sort suffit)
    uint8_t count = (uint8_t)n;
    uint8_t order[64];  // max 63 réseaux — limite raisonnable
    if (count > 64) count = 64;
    for (uint8_t i = 0; i < count; i++) order[i] = i;

    // Tri bulle sur RSSI (décroissant = meilleur signal en premier)
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - 1 - i; j++) {
            if (_wifi->getScanEntry(order[j]).rssi < _wifi->getScanEntry(order[j+1]).rssi) {
                uint8_t tmp = order[j]; order[j] = order[j+1]; order[j+1] = tmp;
            }
        }
    }

    // Dédoublonnage : ne garder que la première occurrence de chaque SSID
    bool seen[64] = {};
    for (uint8_t i = 0; i < count; i++) {
        WiFiManager::ScanEntry e = _wifi->getScanEntry(order[i]);
        if (e.ssid[0] == '\0') continue;  // réseau caché — ignorer

        // Vérifier si ce SSID a déjà été ajouté
        bool duplicate = false;
        for (uint8_t j = 0; j < i; j++) {
            if (seen[j] && strcmp(_wifi->getScanEntry(order[j]).ssid, e.ssid) == 0) {
                duplicate = true;
                break;
            }
        }
        seen[i] = !duplicate;
        if (duplicate) continue;

        JsonObject net = networks.add<JsonObject>();
        net["ssid"]    = e.ssid;
        net["rssi"]    = e.rssi;
        net["secured"] = e.secured;
    }

    // Le résultat n'est volontairement pas libéré ici (pas de clearScan()) :
    // un rechargement de page doit retrouver la même liste sans redéclencher
    // de scan. Il reste en mémoire jusqu'au redémarrage du module.
    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/logs.txt — journal texte de la session courante.
//  /api/logs reste un alias de compatibilite par redirection HTTP.
// ═══════════════════════════════════════════════════════════════
void WebManager::handleGetLogs(AsyncWebServerRequest* req) {
    AsyncResponseStream* response =
        req->beginResponseStream("text/plain; charset=utf-8", 4096U);
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    for (uint8_t i = 0; i < EventLog::count(); ++i) {
        const LogEntry& entry = EventLog::get(i);
        char timeText[10];
        EventLog::msToHms(entry.ms, timeText, sizeof(timeText));
        response->printf("[%s] [%s] %s\n", timeText,
                         EventLog::levelStr(entry.level), entry.msg);
    }
    req->send(response);
}

// ═══════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════

void WebManager::sendJson(AsyncWebServerRequest* req,
                           const JsonDocument& doc, int code) {
    const uint32_t startedUs = micros();

    // Mesurer d'abord pour éviter la double allocation.
    const size_t len = measureJson(doc);
    AsyncResponseStream* resp = req->beginResponseStream("application/json", len + 1);
    resp->setCode(code);
    serializeJson(doc, *resp);
    req->send(resp);

    SystemDiagnostics::noteWebResponse(
        req->url().c_str(), code, len, micros() - startedUs);
}

// "ok" signifie exactement : la demande a ete acceptee et appliquee en memoire.
// Il ne dit RIEN de la persistance, et ne peut rien en dire : la sauvegarde est
// differee de SAVE_DEBOUNCE_MS pour menager la flash, donc au moment ou cette
// reponse part, l'ecriture n'a pas encore eu lieu.
//
// D'ou les deux champs supplementaires, qui rapportent l'etat reel du module :
//   persistPending : une sauvegarde est due et pas encore ecrite ;
//   persistFailed  : la derniere tentative a ECHOUE — un reglage sera perdu au
//                    prochain redemarrage.
//
// Ajoute le 16 aout 2026 : jusque-la cette route repondait {"ok":true} y compris
// lorsque l'ecriture NVS echouait (0 octet sur 4884, partition saturee).
// L'utilisateur voyait une confirmation, et ne decouvrait la perte qu'au
// redemarrage suivant. Annoncer un succes non verifie coute plus cher a la
// confiance qu'une erreur franche.
void WebManager::sendOk(AsyncWebServerRequest* req) {
    const uint32_t startedUs = micros();

    const bool pending = _config && _config->savePending();
    const bool failed  = _config && _config->saveFailed();

    // Tampon fixe plutot qu'un String : cette fonction est sur le chemin de
    // toutes les ecritures, on evite d'y faire churner le tas.
    char body[72];
    const int len = snprintf(
        body, sizeof(body),
        "{\"ok\":true,\"persistPending\":%s,\"persistFailed\":%s}",
        pending ? "true" : "false",
        failed  ? "true" : "false"
    );

    req->send(200, "application/json", body);
    SystemDiagnostics::noteWebResponse(
        req->url().c_str(), 200, len > 0 ? (size_t)len : 0U, micros() - startedUs);
}

void WebManager::sendError(AsyncWebServerRequest* req,
                            const char* msg, int code) {
    const uint32_t startedUs = micros();
    String body = String("{\"error\":\"") + msg + "\"}";
    req->send(code, "application/json", body);
    SystemDiagnostics::noteWebResponse(
        req->url().c_str(), code, body.length(), micros() - startedUs);
}

void WebManager::addJsonHandler(const char* uri,
                                 ArJsonRequestHandlerFunction handler) {
    auto* h = new AsyncCallbackJsonWebHandler(uri, handler);
    h->setMethod(HTTP_POST);
    _server.addHandler(h);
}

// ═══════════════════════════════════════════════════════════════
//  API couche E/S TOR (IoExpander) — tout configurable, rien en dur
// ═══════════════════════════════════════════════════════════════

namespace {
const char* ioStateName(IoExpanderManager::State st, bool isInput) {
    if (st == IoExpanderManager::ST_UNKNOWN) return "indetermine";
    if (isInput) return st == IoExpanderManager::ST_ACTIVE ? "present" : "absent";
    return st == IoExpanderManager::ST_ACTIVE ? "actif" : "inactif";
}
}  // namespace

void WebManager::handleGetIo(AsyncWebServerRequest* req) {
    if (!_ioExpander) { sendError(req, "couche E/S indisponible", 503); return; }
    const IoExpander::Config& c = _ioExpander->config();

    JsonDocument doc;
    doc["enabled"]     = c.enabled != 0;
    doc["pollSeconds"] = c.pollSeconds;
    doc["maxBoards"]   = IoExpander::MAX_BOARDS;
    doc["maxBindings"] = IoExpander::MAX_BINDINGS;

    JsonArray boards = doc["boards"].to<JsonArray>();
    for (uint8_t i = 0; i < IoExpander::MAX_BOARDS; ++i) {
        JsonObject b = boards.add<JsonObject>();
        b["i"]       = i;
        b["enabled"] = c.boards[i].enabled != 0;
        b["addr"]    = c.boards[i].i2cAddress;
        b["ready"]   = _ioExpander->boardReady(i);
    }

    JsonArray binds = doc["bindings"].to<JsonArray>();
    for (uint8_t i = 0; i < IoExpander::MAX_BINDINGS; ++i) {
        const IoExpander::Binding& bd = c.bindings[i];
        JsonObject o = binds.add<JsonObject>();
        o["i"]           = i;
        o["enabled"]     = bd.enabled != 0;
        o["board"]       = bd.boardIndex;
        o["pin"]         = bd.pin;
        o["dir"]         = bd.direction;
        o["role"]        = bd.role;
        o["zone"]        = bd.zone;
        o["activeLevel"] = bd.activeLevel;
        o["pullup"]      = bd.pullup != 0;
        const bool isInput = IoExpander::roleIsInput(bd.role);
        if (bd.enabled) {
            o["state"]   = ioStateName(_ioExpander->inputState(i), isInput);
            o["missing"] = _ioExpander->valveMissing(i);
            if (!isInput) o["command"] = _ioExpander->outputCommand(i);
        }
    }
    sendJson(req, doc);
}

void WebManager::handleSetIoConfig(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_ioExpander) { sendError(req, "couche E/S indisponible", 503); return; }
    const uint8_t nbZones = _config ? _config->nbZones() : (uint8_t)NB_ZONES;

    IoExpander::Config c = IoExpander::makeSafeDefault();
    c.enabled     = (doc["enabled"] | false) ? 1 : 0;
    int poll      = doc["pollSeconds"] | 5;
    c.pollSeconds = (uint8_t)constrain(poll, 1, 60);

    JsonArrayConst boards = doc["boards"].as<JsonArrayConst>();
    for (JsonObjectConst b : boards) {
        int idx = b["i"] | -1;
        if (idx < 0 || idx >= IoExpander::MAX_BOARDS) continue;
        IoExpander::Board& board = c.boards[idx];
        board.enabled    = (b["enabled"] | false) ? 1 : 0;
        board.i2cAddress = (uint8_t)(b["addr"] | 0x21);
        if (board.enabled && !IoExpander::validBoard(board)) {
            sendError(req, "adresse I2C de carte invalide (0x20-0x27)"); return;
        }
    }

    JsonArrayConst binds = doc["bindings"].as<JsonArrayConst>();
    for (JsonObjectConst o : binds) {
        int idx = o["i"] | -1;
        if (idx < 0 || idx >= IoExpander::MAX_BINDINGS) continue;
        IoExpander::Binding& bd = c.bindings[idx];
        bd.enabled     = (o["enabled"] | false) ? 1 : 0;
        bd.boardIndex  = (uint8_t)(o["board"] | 0);
        bd.pin         = (uint8_t)(o["pin"] | 0);
        bd.direction   = (uint8_t)(o["dir"] | 0);
        bd.role        = (uint8_t)(o["role"] | 0);
        bd.zone        = (uint8_t)(o["zone"] | IoExpander::ZONE_NONE);
        bd.activeLevel = (uint8_t)(o["activeLevel"] | 1);
        bd.pullup      = (o["pullup"] | true) ? 1 : 0;
    }

    // Validation globale : un seul binding incoherent fait rejeter l'ensemble,
    // pour ne jamais persister une configuration a moitie fausse.
    for (uint8_t i = 0; i < IoExpander::MAX_BINDINGS; ++i) {
        if (!IoExpander::validBinding(c, i, nbZones)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "binding %u incoherent (direction/role/zone)", i);
            sendError(req, msg);
            return;
        }
    }

    if (!_ioExpander->applyConfig(c)) { sendError(req, "ecriture NVS impossible", 500); return; }
    EventLog::log(LOG_INFO, "IoExpander: configuration mise a jour (active=%u)", c.enabled);
    sendOk(req);
}

void WebManager::handleSetIoOutput(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_ioExpander) { sendError(req, "couche E/S indisponible", 503); return; }
    int idx = doc["binding"] | -1;
    if (idx < 0 || idx >= IoExpander::MAX_BINDINGS) { sendError(req, "binding invalide"); return; }
    const bool on = doc["on"] | false;
    if (!_ioExpander->setOutput((uint8_t)idx, on)) {
        sendError(req, "binding non pilotable (pas une sortie active ou carte absente)");
        return;
    }
    sendOk(req);
}

