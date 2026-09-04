#pragma once
#include <Arduino.h>
#include <cstring>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "NTPManager.h"
#include "WeatherManager.h"
#include "RelaisManager.h"
#include "ScheduleManager.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "EventLog.h"
#include "FaultManager.h"
#include "IncidentManager.h"
#include "NotificationManager.h"
#include "SdStaticHandler.h"
#include "EquipmentOutputRuntimeAdapter.h"
#include "WebAssetsUpdater.h"
#include "IoExpanderManager.h"
#include "MaintenanceRequest.h"
#include "MaintenanceResult.h"

class DisplayManager;

class UpdateCheckScheduler;
class CloudSyncScheduler;

class WebManager {
public:
    // Diffuse une page HTML embarquee en flash, en bornant le nombre de pages
    // servies en parallele. Voir la note detaillee dans WebManager.cpp : au-dela
    // de deux chargements simultanes, la bibliotheque perd des octets en cours
    // de route et livre une page trouee sous un Content-Length complet. Un refus
    // explicite vaut mieux qu'une page fausse.
    void handleSetUpdateCheck(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetCloudSync(AsyncWebServerRequest* req, JsonDocument& doc);

    static void sendEmbeddedPage(AsyncWebServerRequest* req,
                                 const char* page,
                                 size_t pageLength,
                                 const char* extraHeaderName = nullptr,
                                 const char* extraHeaderValue = nullptr);

    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule,
               ConfigManager* config, WiFiManager* wifi = nullptr);

    void update();

    void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
        _outputs = outputs;
        _relais.outputs = outputs;
    }

    // Necessaire pour suspendre/reprendre temporairement un sprite d'ecran
    // autour d'une verification HTTPS de ressource Web (voir la note sur
    // _verifyPending plus bas et ROADMAP.md, "constat du 16 aout 2026").
    void setDisplay(DisplayManager* display) { _display = display; }
    void setIoExpander(IoExpanderManager* io) { _ioExpander = io; }

    void setUpdateCheckScheduler(UpdateCheckScheduler* scheduler) {
        _updateCheck = scheduler;
    }

    void setCloudSyncScheduler(CloudSyncScheduler* scheduler) {
        _cloudSync = scheduler;
    }

    void registerSdStaticHandler(StorageManager* storage) {
        if (_sdStaticHandlerRegistered || !storage) return;
        _sdStaticHandlerRegistered = true;
        _storage = storage;
        _server.addHandler(new SdStaticHandler(storage));
    }

    void registerFaultRoutes() {
        if (_faultRoutesRegistered) return;
        _faultRoutesRegistered = true;
        NotificationManager::begin();

        _server.on("/api/logs/ack", HTTP_POST,
            [](AsyncWebServerRequest* req) {
                EventLog::ackErrors();
                EventLog::log(LOG_INFO, "Erreurs acquittees depuis l'interface Web");
                req->send(200, "application/json", "{\"ok\":true}");
            }
        );

        _server.on("/api/logConfig", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                String body;
                body.reserve(48);
                body += F("{\"timingLogsEnabled\":");
                body += EventLog::timingLogsEnabled() ? F("true") : F("false");
                body += '}';
                AsyncWebServerResponse* response =
                    req->beginResponse(200, "application/json", body);
                response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                req->send(response);
            }
        );

        _server.on("/api/faults", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                String body;
                body.reserve(96);
                body += F("{\"active\":");
                body += FaultManager::hasActiveFaults() ? F("true") : F("false");
                body += F(",\"unacknowledged\":");
                body += FaultManager::hasUnacknowledgedErrors() ? F("true") : F("false");
                body += F(",\"mask\":");
                body += FaultManager::activeMask();
                body += '}';
                AsyncWebServerResponse* response =
                    req->beginResponse(200, "application/json", body);
                response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                req->send(response);
            }
        );

        _server.on("/api/incidents/storage-sd", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                const PersistentIncidentSnapshot incident = IncidentManager::storageSd();
                JsonDocument doc;
                doc["state"] = IncidentManager::stateCode(incident.state);
                doc["active"] = incident.state == IncidentState::ACTIVE;
                doc["acknowledged"] = incident.state == IncidentState::ACKNOWLEDGED;
                doc["occurrences"] = incident.occurrences;
                doc["firstEpoch"] = incident.firstEpoch;
                doc["lastEpoch"] = incident.lastEpoch;
                doc["pendingNotifications"] = incident.pendingNotifications;
                doc["lastReason"] = incident.lastReason;
                String body;
                serializeJson(doc, body);
                AsyncWebServerResponse* response =
                    req->beginResponse(200, "application/json", body);
                response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                req->send(response);
            }
        );

        _server.on("/api/incidents/storage-sd/ack", HTTP_POST,
            [](AsyncWebServerRequest* req) {
                const PersistentIncidentSnapshot incident = IncidentManager::storageSd();
                if (incident.state == IncidentState::ACTIVE) {
                    req->send(409, "application/json", "{\"ok\":false,\"error\":\"incident-active\"}");
                    return;
                }
                IncidentManager::acknowledgeStorageSd();
                req->send(200, "application/json", "{\"ok\":true}");
            }
        );

        _server.on("/api/notifications", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                const NotificationConfig config = NotificationManager::config();
                const NotificationStatus status = NotificationManager::status();
                JsonDocument doc;
                doc["enabled"] = config.enabled;
                doc["server"] = config.server;
                doc["topic"] = config.topic;
                doc["tokenConfigured"] = config.token[0] != '\0';
                doc["configured"] = status.configured;
                doc["workerRunning"] = status.workerRunning;
                doc["testPending"] = status.testPending;
                doc["pendingMask"] = status.pendingMask;
                doc["pendingZoneEvents"] = status.pendingZoneEvents;
                doc["attempts"] = status.attempts;
                doc["nextAttemptInSec"] = status.nextAttemptInSec;
                doc["lastHttpCode"] = status.lastHttpCode;
                doc["lastResult"] = status.lastResult;
                String body;
                serializeJson(doc, body);
                AsyncWebServerResponse* response =
                    req->beginResponse(200, "application/json", body);
                response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                req->send(response);
            }
        );

        _server.on("/api/notifications/test", HTTP_POST,
            [](AsyncWebServerRequest* req) {
                if (!NotificationManager::requestTest()) {
                    req->send(409, "application/json", "{\"ok\":false,\"error\":\"not-configured\"}");
                    return;
                }
                req->send(202, "application/json", "{\"ok\":true,\"queued\":true}");
            }
        );

        AsyncCallbackJsonWebHandler* notificationConfigHandler =
            new AsyncCallbackJsonWebHandler(
                "/api/notifications/config",
                [](AsyncWebServerRequest* req, JsonVariant& value) {
                    JsonObject obj = value.as<JsonObject>();
                    const bool enabled = obj["enabled"] | false;
                    const char* server = obj["server"] | "https://ntfy.sh";
                    const char* topic = obj["topic"] | "";
                    const char* token = obj["token"] | "";
                    const bool preserveToken = obj["preserveToken"] | true;
                    if (!NotificationManager::saveConfig(
                            enabled,
                            server,
                            topic,
                            token,
                            preserveToken)) {
                        req->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid-config\"}");
                        return;
                    }
                    req->send(200, "application/json", "{\"ok\":true}");
                }
            );
        notificationConfigHandler->setMethod(HTTP_POST);
        _server.addHandler(notificationConfigHandler);

        _server.on("/api/maintenance/last-result", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                const MaintenanceResult result = MaintenanceResultStore::load();
                JsonDocument doc;
                doc["valid"] = result.valid;
                if (result.valid) {
                    doc["command"] = result.command;
                    doc["success"] = result.success;
                    doc["httpLine"] = result.httpLine;
                    doc["tlsDurationMs"] = result.tlsDurationMs;
                    doc["recordedUptimeMs"] = result.recordedUptimeMs;
                    doc["minFreeHeap"] = result.minFreeHeap;
                    doc["detail"] = result.detail;
                    doc["manifestSize"] = result.manifestSize;
                    doc["firmwareSize"] = result.firmwareSize;
                    doc["downloadedSize"] = result.downloadedSize;
                    doc["downloadDurationMs"] = result.downloadDurationMs;
                    doc["updateAvailable"] = result.updateAvailable;
                    doc["notificationPending"] = result.notificationPending;
                    doc["installedVersion"] = result.installedVersion;
                    doc["availableVersion"] = result.availableVersion;
                    // Canal ressources Web : present dans le resultat depuis le
                    // 18 aout 2026, mais jamais expose ici. La page /ota
                    // affichait donc "a jour" en permanence pour les pages Web,
                    // faute de pouvoir lire l'inverse.
                    doc["webAssetsUpdateAvailable"] = result.webAssetsUpdateAvailable;
                    doc["webAssetsInstalledVersion"] = result.webAssetsInstalledVersion;
                    doc["webAssetsAvailableVersion"] = result.webAssetsAvailableVersion;
                    // Date reelle, pour repondre a "depuis quand ?" devant un
                    // echec. 0 = horloge non reglee au moment de l'operation.
                    doc["recordedEpoch"] = result.recordedEpoch;
                    doc["channel"] = result.channel;
                    doc["target"] = result.target;
                    doc["environment"] = result.environment;
                    doc["board"] = result.board;
                    doc["firmwareUrl"] = result.firmwareUrl;
                    doc["sha256"] = result.sha256;
                    doc["calculatedSha256"] = result.calculatedSha256;
                }
                String body;
                serializeJson(doc, body);
                AsyncWebServerResponse* response =
                    req->beginResponse(200, "application/json", body);
                response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                req->send(response);
            }
        );

        _server.on("/ota", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Mises a jour</title><style>body{margin:0;background:#101820;color:#eef;font-family:Arial,sans-serif;display:flex;justify-content:center}.card{box-sizing:border-box;width:94%;max-width:680px;padding:20px;margin:16px 0;background:#172532;border:1px solid #385064;border-radius:12px}h1{margin:0 0 14px;font-size:22px}h2{margin:24px 0 8px;font-size:16px;color:#bed0dc}p{line-height:1.5;color:#bed0dc;margin:6px 0}.sub{color:#91aabd;font-size:13px;margin:0 0 12px}.verdict{border-radius:10px;padding:16px;border:1px solid #385064;background:#101820}.verdict.av{border-color:#7e57c2;background:#1b1430}.verdict.ko{border-color:#d59b35;background:#2a2114}.verdict.ok{border-color:#41956b;background:#10241a}.verdict.un{border-color:#526d80}#alert:empty{display:none}#alert{margin-top:10px;border:1px solid #d59b35;background:#2a2114;border-radius:10px;padding:14px}#alert .vt{font-size:15px;color:#ffd88b}#alert.info{border-color:#385064;background:#101820}#alert.info .vt{color:#bed0dc}.vt{font-size:18px;font-weight:700;margin:0 0 8px}.verdict.av .vt{color:#b388ff}.verdict.ko .vt{color:#ffd88b}.verdict.ok .vt{color:#6fcf97}.vw{margin:10px 0 0;padding:10px 12px;background:#0a1219;border-radius:6px;font-size:13.5px;color:#d7e9f3}.vw b{color:#fff}.when{color:#91aabd;font-size:12.5px;margin-top:6px}.chan{display:flex;align-items:center;gap:10px;flex-wrap:wrap;padding:11px 13px;border:1px solid #385064;border-radius:8px;margin-top:8px;background:#101820}.cn{flex:1 1 150px;font-weight:700}.cv{font-family:monospace;font-size:12.5px;color:#d7e9f3}.cv i{font-style:normal;color:#91aabd}.chip{font-size:11px;padding:2px 9px;border-radius:99px;border:1px solid #526d80;color:#91aabd;white-space:nowrap}.chip.up{color:#6fcf97;border-color:#41956b}.chip.av{color:#b388ff;border-color:#7e57c2;font-weight:700}.chip.ko{color:#ffd88b;border-color:#d59b35}.step{border:1px solid #385064;border-radius:9px;padding:13px;margin-bottom:9px;background:#101820}.step.on{border-color:#4fc3f7}.step.off{opacity:.6}.sh{display:flex;align-items:center;gap:10px;margin-bottom:5px}.num{flex:none;width:23px;height:23px;border-radius:50%;background:#385064;color:#eef;display:flex;align-items:center;justify-content:center;font-weight:700;font-size:13px}.step.on .num{background:#4fc3f7;color:#06141b}.st{font-weight:700;font-size:15px}.why{margin:8px 0 0;padding:8px 10px;border-left:3px solid #d59b35;background:#2a2114;color:#ffd88b;font-size:13px;border-radius:0 6px 6px 0}button{box-sizing:border-box;display:block;width:100%;margin-top:10px;padding:11px;border:0;border-radius:7px;font-size:15px;font-weight:700;background:#4fc3f7;color:#06141b;cursor:pointer}button.pu{background:#7e57c2;color:#fff}button.sec{background:#526d80;color:#eef;font-weight:400;font-size:14px}button:disabled{opacity:.42;cursor:not-allowed}a.back{display:block;margin-top:16px;padding:11px;border:1px solid #526d80;border-radius:7px;color:#d7e9f3;text-align:center;text-decoration:none}details{margin-top:9px;border:1px solid #385064;border-radius:8px;padding:10px 12px;background:#101820}summary{cursor:pointer;color:#91aabd;font-size:14px}.row{display:flex;justify-content:space-between;gap:12px;padding:3px 0;font-size:13px}.lb{color:#91aabd}.vl{text-align:right;overflow-wrap:anywhere}.v-ok{color:#6fcf97;font-weight:700}.v-ko{color:#ff8a8a;font-weight:700}#act{min-height:20px;margin-top:10px;font-weight:700;color:#4fc3f7}.wait{display:none;margin-top:10px;padding:12px;border:1px solid #385064;border-radius:8px;background:#101820}.wait.on{display:block}.wl{display:flex;align-items:center;gap:11px}.sp{width:21px;height:21px;border:3px solid #385064;border-top-color:#4fc3f7;border-radius:50%;animation:s .9s linear infinite}.el{margin-top:7px;color:#91aabd;font-size:13px}.pg{height:6px;margin-top:9px;overflow:hidden;border-radius:4px;background:#263b4b}.pg span{display:block;width:35%;height:100%;background:#4fc3f7;animation:t 1.5s ease-in-out infinite}@keyframes s{to{transform:rotate(360deg)}}@keyframes t{0%{transform:translateX(-120%)}100%{transform:translateX(360%)}}</style></head><body><main class="card"><h1>Mises a jour</h1><div id="verdict" class="verdict un"><p class="vt">Lecture de l'etat...</p></div><div id="alert"></div><div id="chans"></div><div id="act"></div><div id="wait" class="wait"><div class="wl"><span class="sp"></span><span id="wt">Operation en cours...</span></div><div id="el" class="el">Temps ecoule : 0 s</div><div class="pg"><span></span></div></div><h2>Programme du module</h2><p class="sub">Trois etapes, dans cet ordre. Chacune s'active quand la precedente a reussi. Aucune n'est possible pendant un arrosage.</p><div id="s1" class="step on"><div class="sh"><span class="num">1</span><span class="st">Verifier ce qui est disponible</span></div><p>Le module redemarre environ 30 secondes en mode maintenance, interroge les deux sources, puis revient tout seul. <b>Rien n'est installe a cette etape.</b></p><button id="b-check" onclick="go('check')">Verifier maintenant</button></div><div id="s2" class="step"><div class="sh"><span class="num">2</span><span class="st">Preparer le nouveau programme</span></div><p>Telecharge le programme, verifie son empreinte SHA-256, puis l'ecrit dans la partition de reserve. Le module continue de tourner sur l'ancien : <b>rien n'est active a cette etape.</b></p><div id="w2" class="why"></div><button id="b-stage" onclick="go('stage')">Preparer</button></div><div id="s3" class="step"><div class="sh"><span class="num">3</span><span class="st">Installer et redemarrer</span></div><p>Bascule sur le programme prepare et redemarre dessus. Si le module ne redemarre pas correctement plusieurs fois de suite, <b>l'ancien programme est restaure automatiquement.</b></p><div id="w3" class="why"></div><button id="b-install" onclick="go('install')">Installer</button></div><h2>Pages Web</h2><p class="sub">Les pages, styles et scripts servis depuis la carte SD. Ils sont <b>independants du programme du module</b> : cette mise a jour ne touche pas au firmware, et une seule etape suffit.</p><div class="step on"><p>Le module redemarre en mode maintenance, telecharge chaque fichier, verifie son empreinte, puis bascule. <b>Si quoi que ce soit echoue, les pages actuelles sont conservees.</b></p><div id="wweb" class="why"></div><button id="b-web" class="pu" onclick="goWeb()">Mettre a jour les pages Web</button></div><h2>Details</h2><details id="dlast"><summary>Detail technique de la derniere operation</summary><div id="last"></div></details><details><summary>Diagnostics et depot direct depuis un ordinateur</summary><p>Ces deux outils ne mettent rien a jour. Ils servent a comprendre pourquoi une etape echoue.</p><button class="sec" onclick="go('probe')">Tester uniquement la connexion a la source</button><button class="sec" onclick="go('download')">Telecharger et verifier l'empreinte, sans rien ecrire</button><p><b>Depot direct.</b> Troisieme voie, pour le depannage : quand la mise a jour reseau des pages echoue, un ordinateur du meme reseau peut deposer les fichiers directement, sans redemarrage, via <span class="cv">/api/debug/deploy-begin</span>, <span class="cv">/api/debug/deploy-file?name=&lt;nom&gt;</span> puis <span class="cv">/api/debug/deploy-commit</span>. Le depot est transactionnel : les fichiers transitent par un repertoire separe et ne remplacent les pages en service qu'a la derniere etape.</p></details><a class="back" href="/index.html">Retour a AquaLook</a></main><script>
const esc=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
let LAST=null,WEB=null,timer=null,poll=null,BUSY=false;

// Les explications d'echec vivent sur la carte SD (/ota-help.json), pas ici.
// Deux raisons : la flash est comptee, et surtout ces textes sont ce qu'on
// corrigera le plus souvent -- les enrichir devient une mise a jour des pages
// Web, sans reflasher le module. Si le fichier manque, la page reste
// utilisable et affiche le code technique brut : aucune fonction ne depend
// de lui, seulement le confort de lecture.
let HELP=null;
// Le module accole parfois du contexte au code :
//   "content-length-mismatch version=5.9.15 fichiers=6/11"
// Le code est le premier mot, le reste est du contexte affichable. La
// recherche par egalite stricte echouait donc sur tout code enrichi, et
// renvoyait "pas encore documente" pour une entree pourtant presente.
function splitDetail(d){
  const s = String(d || '').trim();
  const i = s.indexOf(' ');
  return i < 0 ? { code: s, contexte: '' }
               : { code: s.slice(0, i), contexte: s.slice(i + 1) };
}

// Remplace les reperes du fichier d&rsquo;aide par les valeurs reelles.
// split/join et non une expression reguliere : les accolades y demandent
// un echappement, et un antislash perdu casserait la page en silence.
function subst(s, j){
  return String(s || '')
    .split('{cible}').join(j.target || 'inconnue')
    .split('{version}').join(j.installedVersion || 'inconnue');
}

function explain(j){
  const d = splitDetail(j.detail);
  let e = HELP && HELP.errors && HELP.errors[d.code];
  // Variante selon la cible du build, quand le fichier d&rsquo;aide en
  // propose une. C&rsquo;est a la page de trancher : elle connait la
  // cible, et faire choisir le lecteur entre deux paragraphes revenait a
  // lui refiler le travail.
  if(e && e.cas && j.target && e.cas[j.target]) e = e.cas[j.target];
  if(e) return { titre: e.titre || null, niveau: e.niveau || 'alerte',
                 quoi: subst(e.quoi, j), faire: subst(e.faire, j), contexte: d.contexte };

  const hp = HELP && HELP.http;
  if(hp){
    const m = String(j.httpLine || '').match(/ ([0-9][0-9][0-9])/);
    if(m){
      const k = hp[m[1]] || (m[1].charAt(0) === '5' ? hp['5xx'] : null);
      if(k) return { titre: null, niveau: 'alerte',
                     quoi: subst(k.quoi, j), faire: subst(k.faire, j), contexte: d.contexte };
    }
  }

  // Repli. Il doit encore dire une cause ET une action : un ecran qui
  // laisse deviner ce qui s&rsquo;est passe ne sert a rien.
  return { titre: null, niveau: 'alerte', contexte: d.contexte,
    quoi: d.code
      ? ('Le module a interrompu l&rsquo;operation en signalant : ' + d.code)
      : 'Le module a interrompu l&rsquo;operation sans en preciser la cause.',
    faire: HELP
      ? 'Rien n&rsquo;a ete installe : ce qui fonctionnait avant continue de fonctionner. Ce code n&rsquo;est pas encore traduit en clair ; le detail technique replie plus bas donne la reponse du serveur et l&rsquo;etat exact de l&rsquo;operation. Le bouton Tester la connexion, dans Diagnostics, dit en une fois si le probleme vient du reseau.'
      : 'Rien n&rsquo;a ete installe : ce qui fonctionnait avant continue de fonctionner. Les explications detaillees n&rsquo;ont pas pu etre lues sur la carte SD ; le detail technique replie plus bas reste disponible.' };
}
function whenTxt(j){
  if(!j.recordedEpoch)return "Date inconnue : l'horloge du module n'etait pas reglee au moment de l'operation.";
  const d=new Date(j.recordedEpoch*1000);
  const days=Math.floor((Date.now()-d.getTime())/86400000);
  const rel=days<=0?"aujourd'hui":days===1?'hier':'il y a '+days+' jours';
  return 'Constate le '+d.toLocaleString('fr-FR')+' ('+rel+').';
}
async function readLast(){const r=await fetch('/api/maintenance/last-result',{cache:'no-store'});if(!r.ok)throw new Error('http-'+r.status);return r.json()}
// Memes conditions que celles appliquees par le module (WebManager.h) : la
// page ne doit jamais proposer une etape qui sera refusee apres redemarrage.
const fwReady=j=>!!(j&&j.valid&&j.success&&j.updateAvailable&&j.firmwareUrl&&j.firmwareSize&&j.sha256);
const staged=j=>!!(j&&j.valid&&j.success&&j.command==='stage_update_test'&&j.calculatedSha256&&j.calculatedSha256===j.sha256);

function render(){
  const j=LAST,v=document.getElementById('verdict');
  const checked=j&&j.valid&&j.command==='check_version';
  const failed=j&&j.valid&&!j.success;
  const fwAv=!!(j&&j.updateAvailable),webAv=!!(j&&j.webAssetsUpdateAvailable);

  // -- Verdict : la reponse a "qu&rsquo;est-ce que je fais maintenant ?".
  //
  // Un echec ne disparait JAMAIS parce qu&rsquo;une autre nouvelle serait
  // plus interessante. Quand une mise a jour est disponible ET que la
  // verification a echoue, le verdict porte la nouvelle actionnable et
  // l&rsquo;echec descend dans son propre bloc juste dessous, avec son
  // explication complete et son bouton. Sans cela il ne restait de
  // l&rsquo;echec qu&rsquo;une pastille "non verifiable", sans un mot --
  // exactement ce qu&rsquo;on reprochait a la premiere version de cette page.
  // Le titre nomme l&rsquo;operation qui a echoue. Il annoncait
  // "la verification du programme" quoi qu&rsquo;il arrive, y compris
  // pour une mise a jour des pages Web -- ce qui envoyait chercher la
  // cause du mauvais cote.
  const TITRES = {
    check_version:        'La verification des mises a jour n&rsquo;a pas abouti',
    web_assets_update:    'La mise a jour des pages Web n&rsquo;a pas abouti',
    stage_update_test:    'La preparation du nouveau programme n&rsquo;a pas abouti',
    download_update_test: 'Le test de telechargement n&rsquo;a pas abouti',
    install_update:       'L&rsquo;installation du programme n&rsquo;a pas abouti',
    probe_github:         'Le test de connexion n&rsquo;a pas abouti'
  };
  const relancer = '<button onclick="go(&#39;check&#39;)">Relancer la verification</button>';
  const failBox = function(){
    const e = explain(j);
    // Un etat normal ne se presente pas comme une panne. "niveau": "info"
    // rend le bloc neutre et retire le bouton de relance : relancer ne
    // changerait rien, et le proposer laisserait croire le contraire.
    const info = e.niveau === 'info';
    alertBox.className = info ? 'info' : '';
    const titre = e.titre || TITRES[j.command] || 'L&rsquo;operation n&rsquo;a pas abouti';
    return '<p class="vt">' + titre + '</p>'
         + '<p>' + esc(e.quoi) + '</p>'
         + (e.contexte ? '<p class="when">Precision du module : ' + esc(e.contexte) + '</p>' : '')
         + '<p class="when">' + esc(whenTxt(j)) + '</p>'
         + '<div class="vw"><b>Que faire :</b> ' + esc(e.faire) + '</div>'
         + (info ? '' : relancer);
  };
  const alertBox = document.getElementById('alert');
  alertBox.innerHTML = '';

  if(!j || !j.valid){
    v.className = 'verdict un';
    v.innerHTML = '<p class="vt">Aucune verification effectuee</p>'
      + '<p>Le module n&rsquo;a encore jamais interroge la source. Il ne sait donc pas s&rsquo;il existe quelque chose a installer &mdash; ce qui n&rsquo;est pas la meme chose qu&rsquo;etre a jour.</p>'
      + '<button onclick="go(&#39;check&#39;)">Verifier maintenant</button>';
  } else if(fwAv || webAv){
    const quoi = (fwAv && webAv) ? 'Le programme du module et les pages Web'
               : fwAv ? 'Le programme du module' : 'Les pages Web';
    v.className = 'verdict av';
    v.innerHTML = '<p class="vt">&#8593; Une mise a jour est disponible</p>'
      + '<p>' + quoi + ' peuvent etre mis a jour. '
      + (fwAv ? 'Pour le programme, suivez les etapes 1 a 3 ci-dessous.' : 'Un seul bouton suffit, plus bas.') + '</p>'
      + '<p class="when">' + esc(whenTxt(j)) + '</p>';
    if(failed) alertBox.innerHTML = failBox();
  } else if(failed){
    // Le niveau vient du fichier d&rsquo;aide : un etat normal ne prend
    // pas les couleurs d&rsquo;une panne, meme quand c&rsquo;est le
    // verdict lui-meme qui le porte.
    const html = failBox();
    v.className = (alertBox.className === 'info') ? 'verdict un' : 'verdict ko';
    alertBox.className = '';
    v.innerHTML = html;
  } else {
    v.className='verdict ok';
    v.innerHTML='<p class="vt">&#10003; Tout est a jour</p><p>Ni le programme du module ni les pages Web n\'ont de version plus recente disponible.</p>'+
      '<p class="when">'+esc(whenTxt(j))+'</p>';
  }

  // ── Les deux canaux, avec des mots et non des chiffres nus.
  const wi=WEB?(WEB.version||WEB.gitSha):null;
  const chan=(n,inst,av,txt,cls,note)=>'<div class="chan"><span class="cn">'+esc(n)+'</span><span class="cv"><i>installee :</i> '+
    esc(inst||'inconnue')+(av?' &nbsp; <i>disponible :</i> '+esc(av):'')+(note?' &nbsp; <i>'+esc(note)+'</i>':'')+
    '</span><span class="chip '+cls+'">'+esc(txt)+'</span></div>';
  let h='';
  // Le programme se juge sur SA propre verification. La ligne affichait
  // "a jour" des que la derniere operation avait reussi, y compris quand
  // c'etait une mise a jour des pages Web -- qui ne dit rien du programme.
  // Le module ne gardant qu'un seul resultat pour deux canaux, la seule
  // reponse honnete hors verification est de ne pas se prononcer.
  const fwVerifie = !!(j && j.valid && j.command === 'check_version');
  if(!j || !j.valid) h += chan('Programme du module', j && j.installedVersion, null, 'jamais verifie', '', '');
  else if(fwAv) h += chan('Programme du module', j.installedVersion, j.availableVersion, 'a installer', 'av', '');
  else if(!fwVerifie) h += chan('Programme du module', j.installedVersion, null, 'non verifie', '', '');
  else if(failed) h += chan('Programme du module', j.installedVersion, null,
        j.target === 'unsupported' ? 'aucune version publiee' : 'non verifiable', 'ko', '');
  else h += chan('Programme du module', j.installedVersion, null, 'a jour', 'up', '');
  // Le canal Web se juge sur SA propre reponse, jamais sur j.success qui
  // est le resultat du canal firmware. Une version disponible renseignee
  // prouve que checkForUpdate() est alle au bout : il ne la remplit
  // qu&rsquo;apres avoir lu et valide le catalogue.
  // Deux preuves valent : une version disponible renseignee (une
  // verification est allee au bout), ou un deploiement reussi -- le
  // firmware efface la version disponible en le consommant, alors que
  // c'est justement le moment ou le canal a le plus travaille.
  const webRepondu = !!(j && (j.webAssetsAvailableVersion ||
        (j.command === 'web_assets_update' && j.success)));
  if(webAv) h += chan('Pages Web', wi || (j && j.webAssetsInstalledVersion), j.webAssetsAvailableVersion, 'a installer', 'av', '');
  else if(webRepondu) h += chan('Pages Web', wi, null, 'a jour', 'up', '');
  else h += chan('Pages Web', wi, null, 'jamais verifiees', '', 'sur le module');
  document.getElementById('chans').innerHTML=h;

  // ── Etapes verrouillees, avec la raison du verrouillage.
  const ok2=fwReady(j),ok3=staged(j);
  gate('s2','b-stage','w2',ok2, checked&&j.success&&!j.updateAvailable
    ? "Aucune mise a jour du programme n'est disponible : il n'y a rien a preparer."
    : failed ? "La verification n'a pas abouti (voir en haut de page). Corrigez la cause, puis relancez l'etape 1."
    : "Lancez d'abord l'etape 1. Attention : le module ne conserve qu'un seul resultat, donc une mise a jour des pages Web efface celui de la verification — il faut alors relancer l'etape 1.");
  gate('s3','b-install','w3',ok3,"Lancez d'abord l'etape 2 : rien n'a encore ete prepare et verifie dans la partition de reserve.");
  const wb=document.getElementById('b-web'),ww=document.getElementById('wweb');
  wb.textContent=webAv?('Installer les pages Web '+(j.webAssetsAvailableVersion||'')):'Mettre a jour les pages Web';
  ww.style.display=webAv?'none':'block';
  ww.textContent=webAv?'':"Aucune version plus recente n'a ete reperee. Le bouton reste utilisable : il reinstalle ce que la source publie actuellement, ce qui repare des pages abimees.";
  renderLast();
}
function gate(sid,bid,wid,ok,why){const s=document.getElementById(sid),b=document.getElementById(bid),w=document.getElementById(wid);
  s.className='step '+(ok?'on':'off');b.disabled=!ok||BUSY;w.style.display=ok?'none':'block';w.textContent=ok?'':why}
function renderLast(){const e=document.getElementById('last'),j=LAST;
  if(!j||!j.valid){e.innerHTML='<p class="sub">Aucune operation enregistree.</p>';return}
  const row=(a,b,c)=>'<div class="row"><span class="lb">'+esc(a)+'</span><span class="vl'+(c?' '+c:'')+'">'+esc(b)+'</span></div>';
  const r=[['Operation',j.command||'inconnue'],['Resultat',j.success?'reussie':'echouee',j.success?'v-ok':'v-ko']];
  if(j.recordedEpoch)r.push(['Date',new Date(j.recordedEpoch*1000).toLocaleString('fr-FR')]);
  if(j.httpLine)r.push(['Reponse HTTP',j.httpLine]);
  if(j.detail)r.push(['Code interne',j.detail,j.success?'v-ok':'v-ko']);
  if(j.target)r.push(['Cible attendue',j.target]);
  if(j.board)r.push(['Carte',j.board]);
  if(j.environment)r.push(['Environnement',j.environment]);
  if(j.channel)r.push(['Canal',j.channel]);
  r.push(['Programme installe',j.installedVersion||'-'],['Programme disponible',j.availableVersion||'-'],
         ['Pages installees',j.webAssetsInstalledVersion||'-'],['Pages disponibles',j.webAssetsAvailableVersion||'-']);
  if(j.command==='download_update_test'||j.command==='stage_update_test')
    r.push(['Octets telecharges',j.downloadedSize||0],['Empreinte attendue',j.sha256||'-'],
           ['Empreinte calculee',j.calculatedSha256||'-',j.calculatedSha256&&j.calculatedSha256===j.sha256?'v-ok':'v-ko']);
  r.push(['Duree TLS',(j.tlsDurationMs||0)+' ms'],['Memoire libre minimale',(j.minFreeHeap||0)+' octets']);
  e.innerHTML=r.map(x=>row(x[0],x[1],x[2])).join('')}

async function refresh(){
  if(HELP===null){try{const r=await fetch('/ota-help.json',{cache:'no-store'});HELP=r.ok?await r.json():false}catch(_){HELP=false}}
  try{LAST=await readLast()}catch(_){}
  try{const r=await fetch('/assets-version.json',{cache:'no-store'});WEB=r.ok?await r.json():WEB}catch(_){}
  render()}
// Rafraichissement autonome : apres une verification, l'etat et les etapes se
// mettent a jour seuls. Sans cela il fallait recharger la page a la main pour
// que l'etape suivante se debloque.
function startPoll(){clearInterval(poll);poll=setInterval(()=>{if(!BUSY)refresh()},10000)}

function startWait(k){const b=document.getElementById('wait'),t=document.getElementById('wt'),el=document.getElementById('el');b.classList.add('on');const t0=Date.now();
  const m=k==='web'?['Redemarrage en mode maintenance...','Lecture du catalogue publie...','Telechargement et verification des fichiers...','Bascule des nouvelles pages...','Retour au fonctionnement normal...']
   :k==='install'?['Redemarrage du module...','Activation du nouveau programme...','Verification du demarrage...','Retour au fonctionnement normal...']
   :['Redemarrage du module...','Connexion au reseau...','Operation en cours...','Retour au fonctionnement normal...'];
  let i=0;t.textContent=m[0];el.textContent='Temps ecoule : 0 s';clearInterval(timer);
  timer=setInterval(()=>{const s=Math.floor((Date.now()-t0)/1000);el.textContent='Temps ecoule : '+s+' s';
    const n=Math.min(Math.floor(s/12),m.length-1);if(n!==i){i=n;t.textContent=m[i]}},1000)}
function stopWait(){clearInterval(timer);timer=null;document.getElementById('wait').classList.remove('on')}
const sig=j=>j&&j.valid?[j.command||'',j.recordedUptimeMs||0,j.detail||''].join(':'):'none';
function busy(v){BUSY=v;['b-check','b-stage','b-install','b-web'].forEach(id=>{const e=document.getElementById(id);if(e)e.disabled=v})}

async function go(k){
  const uri={check:'/api/maintenance/check-version',download:'/api/maintenance/download-update-test',stage:'/api/maintenance/stage-update-test',install:'/api/maintenance/install-update',probe:'/api/maintenance/probe-github'}[k];
  const exp={check:'check_version',download:'download_update_test',stage:'stage_update_test',install:'install_update',probe:'probe_github'}[k];
  const q={check:'Le module va redemarrer environ 30 secondes pour verifier les versions disponibles. Rien ne sera installe. Continuer ?',
   download:'Le programme sera telecharge et son empreinte verifiee, sans rien ecrire. Continuer ?',
   stage:'Le programme sera telecharge, verifie, puis ecrit dans la partition de reserve. Il ne sera PAS active. Continuer ?',
   install:'Le programme prepare va etre active et le module redemarrera dessus. En cas de redemarrages repetes sans validation, le programme precedent est restaure automatiquement. Continuer ?',
   probe:'Le module va redemarrer pour tester la connexion a la source. Continuer ?'}[k];
  if(!confirm(q))return;
  const out=document.getElementById('act');busy(true);out.textContent='Preparation du redemarrage...';startWait(k);
  let base='none';try{base=sig(await readLast())}catch(_){}
  try{const x=await fetch(uri,{method:'POST'});const j=await x.json().catch(()=>({}));
    if(!x.ok){out.textContent=j.error==='watering-active'?'Refuse : un arrosage est en cours. Reessayez apres.'
      :j.error==='check-version-required'?"Refuse : relancez l'etape 1, le module n'a plus de verification valide en memoire."
      :j.error==='stage-required'?"Refuse : l'etape 2 n'a pas ete faite."
      :'Erreur : '+(j.error||x.status);
      stopWait();busy(false);await refresh();return}
    out.textContent='Demande acceptee. Redemarrage et traitement en cours...'}
  catch(_){out.textContent='Connexion interrompue : le module redemarre.'}
  await waitResult(exp,base,k==='download'?150000:90000,out)}

async function waitResult(exp,base,ms,out){const dl=Date.now()+ms;let off=false;
  while(Date.now()<dl){await sleep(2000);
    try{const j=await readLast();
      if(j.valid&&j.command===exp&&sig(j)!==base){out.textContent='Operation terminee.';stopWait();busy(false);await refresh();
        setTimeout(()=>{out.textContent=''},6000);return}
      out.textContent=off?'Le module est revenu. Finalisation...':'Operation en cours sur le module...'}
    catch(_){off=true;out.textContent='Le module redemarre...'}}
  stopWait();busy(false);out.textContent='Delai atteint. L\'etat ci-dessus se remet a jour tout seul.';await refresh()}

async function goWeb(){const out=document.getElementById('act');
  if(!confirm("Le module va redemarrer en mode maintenance pour mettre a jour les pages Web. L'arrosage n'est pas affecte. Continuer ?"))return;
  busy(true);out.textContent='Preparation du redemarrage...';startWait('web');
  try{const x=await fetch('/api/webassets/update',{method:'POST'});const j=await x.json().catch(()=>({}));
    if(!x.ok){out.textContent=(j.error||'').indexOf('arrosage')>=0?'Refuse : un arrosage est en cours. Reessayez apres.':'Erreur : '+(j.error||x.status);
      stopWait();busy(false);await refresh();return}
    out.textContent='Demande acceptee. Redemarrage et mise a jour en cours...'}
  catch(_){out.textContent='Connexion interrompue : le module redemarre.'}
  const dl=Date.now()+180000;
  while(Date.now()<dl){await sleep(3000);
    try{const r=await fetch('/assets-version.json',{cache:'no-store'});
      if(r.ok){out.textContent='Mise a jour terminee.';stopWait();busy(false);await refresh();
        setTimeout(()=>{out.textContent=''},6000);return}}catch(_){}}
  stopWait();busy(false);out.textContent="Delai atteint. L'etat ci-dessus se remet a jour tout seul.";await refresh()}

refresh();startPoll();
</script></body></html>
)rawliteral";
                WebManager::sendEmbeddedPage(req, PAGE, sizeof(PAGE) - 1U);
            }
        );

        _server.on("/api/maintenance/probe-github", HTTP_POST,
            [this](AsyncWebServerRequest* req) {
                if (!_config || !_relais) {
                    req->send(503, "application/json", "{\"ok\":false,\"error\":\"runtime-not-ready\"}");
                    return;
                }

                for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
                    if (_relais->getState(zone)) {
                        EventLog::log(
                            LOG_WARN,
                            "Maintenance Web: probe GitHub refuse, zone %u active",
                            static_cast<unsigned>(zone + 1U)
                        );
                        req->send(409, "application/json", "{\"ok\":false,\"error\":\"watering-active\"}");
                        return;
                    }
                }

                if (!MaintenanceRequestStore::save(MaintenanceRequest::PROBE_GITHUB)) {
                    EventLog::log(LOG_ERROR, "Maintenance Web: echec enregistrement demande NVS");
                    req->send(500, "application/json", "{\"ok\":false,\"error\":\"nvs-write-failed\"}");
                    return;
                }

                EventLog::log(LOG_WARN, "Maintenance Web: probe GitHub demande, redemarrage programme");
                _restartPending = true;
                _restartAtMs = millis() + 750U;
                req->send(202, "application/json", "{\"ok\":true,\"restart\":true,\"command\":\"probe_github\"}");
            }
        );

        _server.on("/api/maintenance/check-version", HTTP_POST,
            [this](AsyncWebServerRequest* req) {
                if (!_config || !_relais) {
                    req->send(503, "application/json", "{\"ok\":false,\"error\":\"runtime-not-ready\"}");
                    return;
                }

                for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
                    if (_relais->getState(zone)) {
                        EventLog::log(
                            LOG_WARN,
                            "Maintenance Web: verification version refusee, zone %u active",
                            static_cast<unsigned>(zone + 1U)
                        );
                        req->send(409, "application/json", "{\"ok\":false,\"error\":\"watering-active\"}");
                        return;
                    }
                }

                if (!MaintenanceRequestStore::save(MaintenanceRequest::CHECK_VERSION)) {
                    EventLog::log(LOG_ERROR, "Maintenance Web: echec enregistrement CHECK_VERSION NVS");
                    req->send(500, "application/json", "{\"ok\":false,\"error\":\"nvs-write-failed\"}");
                    return;
                }

                EventLog::log(LOG_WARN, "Maintenance Web: verification version demandee, redemarrage programme");
                _restartPending = true;
                _restartAtMs = millis() + 750U;
                req->send(202, "application/json", "{\"ok\":true,\"restart\":true,\"command\":\"check_version\"}");
            }
        );

        _server.on("/api/maintenance/download-update-test", HTTP_POST,
            [this](AsyncWebServerRequest* req) {
                if (!_config || !_relais) {
                    req->send(503, "application/json", "{\"ok\":false,\"error\":\"runtime-not-ready\"}");
                    return;
                }
                for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
                    if (_relais->getState(zone)) {
                        EventLog::log(LOG_WARN,
                                      "Maintenance Web: test telechargement refuse, zone %u active",
                                      static_cast<unsigned>(zone + 1U));
                        req->send(409, "application/json", "{\"ok\":false,\"error\":\"watering-active\"}");
                        return;
                    }
                }
                const MaintenanceResult previous = MaintenanceResultStore::load();
                if (!previous.valid || !previous.success || !previous.updateAvailable ||
                    previous.firmwareUrl[0] == '\0' || previous.firmwareSize == 0U ||
                    previous.sha256[0] == '\0') {
                    req->send(409, "application/json", "{\"ok\":false,\"error\":\"check-version-required\"}");
                    return;
                }
                if (!MaintenanceRequestStore::save(MaintenanceRequest::DOWNLOAD_UPDATE_TEST)) {
                    req->send(500, "application/json", "{\"ok\":false,\"error\":\"nvs-write-failed\"}");
                    return;
                }
                EventLog::log(LOG_WARN,
                              "Maintenance Web: test telechargement demande, redemarrage programme");
                _restartPending = true;
                _restartAtMs = millis() + 750U;
                req->send(202, "application/json", "{\"ok\":true,\"restart\":true,\"command\":\"download_update_test\"}");
            }
        );
        _server.on("/api/maintenance/stage-update-test", HTTP_POST,
            [this](AsyncWebServerRequest* req) {
                if (!_config || !_relais) {
                    req->send(503, "application/json", "{\"ok\":false,\"error\":\"runtime-not-ready\"}");
                    return;
                }
                for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
                    if (_relais->getState(zone)) {
                        EventLog::log(LOG_WARN,
                                      "Maintenance Web: staging refuse, zone %u active",
                                      static_cast<unsigned>(zone + 1U));
                        req->send(409, "application/json", "{\"ok\":false,\"error\":\"watering-active\"}");
                        return;
                    }
                }
                const MaintenanceResult previous = MaintenanceResultStore::load();
                if (!previous.valid || !previous.success || !previous.updateAvailable ||
                    previous.firmwareUrl[0] == '\0' || previous.firmwareSize == 0U ||
                    previous.sha256[0] == '\0') {
                    req->send(409, "application/json", "{\"ok\":false,\"error\":\"check-version-required\"}");
                    return;
                }
                if (!MaintenanceRequestStore::save(MaintenanceRequest::STAGE_UPDATE_TEST)) {
                    req->send(500, "application/json", "{\"ok\":false,\"error\":\"nvs-write-failed\"}");
                    return;
                }
                EventLog::log(LOG_WARN,
                              "Maintenance Web: staging partition inactive demande, redemarrage programme");
                _restartPending = true;
                _restartAtMs = millis() + 750U;
                req->send(202, "application/json",
                          "{\"ok\":true,\"restart\":true,\"command\":\"stage_update_test\"}");
            }
        );
        _server.on("/api/maintenance/install-update", HTTP_POST,
            [this](AsyncWebServerRequest* req) {
                if (!_config || !_relais) {
                    req->send(503, "application/json", "{\"ok\":false,\"error\":\"runtime-not-ready\"}");
                    return;
                }
                for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
                    if (_relais->getState(zone)) {
                        EventLog::log(LOG_WARN,
                                      "Maintenance Web: installation refusee, zone %u active",
                                      static_cast<unsigned>(zone + 1U));
                        req->send(409, "application/json", "{\"ok\":false,\"error\":\"watering-active\"}");
                        return;
                    }
                }
                const MaintenanceResult previous = MaintenanceResultStore::load();
                const bool stagedOk = previous.valid && previous.success &&
                    strcmp(previous.command, "stage_update_test") == 0 &&
                    previous.calculatedSha256[0] != '\0' &&
                    strcmp(previous.calculatedSha256, previous.sha256) == 0;
                if (!stagedOk) {
                    req->send(409, "application/json", "{\"ok\":false,\"error\":\"stage-required\"}");
                    return;
                }
                if (!MaintenanceRequestStore::save(MaintenanceRequest::INSTALL_UPDATE)) {
                    req->send(500, "application/json", "{\"ok\":false,\"error\":\"nvs-write-failed\"}");
                    return;
                }
                EventLog::log(LOG_WARN,
                              "Maintenance Web: installation du firmware verifie demandee, redemarrage programme");
                _restartPending = true;
                _restartAtMs = millis() + 750U;
                req->send(202, "application/json",
                          "{\"ok\":true,\"restart\":true,\"command\":\"install_update\"}");
            }
        );

        _server.on("/logs", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Journal</title></head><body><p>Interface de secours. Le fichier complet est servi depuis la carte SD.</p><a href="/api/logs.txt">Journal brut</a></body></html>
)rawliteral";
                WebManager::sendEmbeddedPage(req, PAGE, sizeof(PAGE) - 1U,
                                             "X-AquaLook-Storage",
                                             "Firmware-Fallback");
            }
        );
    }

private:
    struct OutputAwareRelayState {
        RelaisManager* relay = nullptr;
        AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs = nullptr;

        OutputAwareRelayState& operator=(RelaisManager* value) {
            relay = value;
            return *this;
        }
        explicit operator bool() const { return relay != nullptr; }
        OutputAwareRelayState* operator->() { return this; }
        const OutputAwareRelayState* operator->() const { return this; }

        bool getState(uint8_t zone) const {
            if (outputs) {
                const AquaLook::Domain::EquipmentStateValue state =
                    outputs->getZoneValveState(zone);
                if (state.validity == AquaLook::Domain::StateValidity::VALID &&
                    state.kind == AquaLook::Domain::StateValueKind::BINARY) {
                    return state.value != 0;
                }
            }
            return relay ? relay->getState(zone) : false;
        }
    };

    AsyncWebServer _server { 80 };
    NTPManager* _ntp = nullptr;
    WeatherManager* _weather = nullptr;
    OutputAwareRelayState _relais;
    ScheduleManager* _schedule = nullptr;
    ConfigManager* _config = nullptr;
    WiFiManager* _wifi = nullptr;
    AquaLook::Runtime::EquipmentOutputRuntimeAdapter* _outputs = nullptr;
    StorageManager* _storage = nullptr;
    UpdateCheckScheduler* _updateCheck = nullptr;
    CloudSyncScheduler* _cloudSync = nullptr;
    bool _sdStaticHandlerRegistered = false;
    bool _faultRoutesRegistered = false;

    portMUX_TYPE _pendingMux = portMUX_INITIALIZER_UNLOCKED;
    volatile bool _systemSavePending = false;
    volatile uint32_t _systemSaveAtMs = 0;
    CfgSystem _pendingSystem;
    uint16_t _pendingManualDuration = 10;
    bool _pendingManualDurationValid = false;
    bool _pendingSystemReboot = false;
    bool _restartPending = false;
    uint32_t _restartAtMs = 0;

    // Etape 4 (test) — voir ROADMAP.md, "constat du 16 aout 2026" : la
    // verification HTTPS doit s'executer depuis la boucle principale
    // (meme regle que _pendingSystem ci-dessus : "Ne jamais ecrire depuis
    // le callback AsyncTCP", ici etendue a "ne jamais toucher un TFT_eSprite
    // depuis ce callback"). Le POST depose la demande dans ces champs fixes
    // (pas de String — aucune allocation dans la section critique) puis
    // repond immediatement ; WebManager::update() (boucle principale)
    // l'execute, suspend/reprend le sprite autour, et range le resultat ici.
    DisplayManager* _display = nullptr;
    IoExpanderManager* _ioExpander = nullptr;
    static constexpr size_t VERIFY_URL_MAX = 200;
    volatile bool _verifyPending = false;
    volatile bool _verifyRunning = false;
    volatile bool _verifyResultReady = false;
    char _verifyUrl[VERIFY_URL_MAX] = {0};
    uint32_t _verifySize = 0;
    char _verifySha256[65] = {0};
    WebAssetVerifyResult _verifyResult;

    void runPendingVerify();

    void setupRoutes();
    void setupCaptiveRoutes();
    void handleStatus(AsyncWebServerRequest* request);
    void handleAdminStatus(AsyncWebServerRequest* request);
    void handleDiagnostics(AsyncWebServerRequest* request);
    void handleSetMode(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetInterval(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetDaySlot(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetIntervalSlot(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetRain(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleManual(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetManualDuration(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSaveSchedule(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetIntervalAnchor(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleDeleteIntervalProgramming(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetWifi(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetWifiKeepalive(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetTouch(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetNtp(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleGetIo(AsyncWebServerRequest* req);
    void handleSetIoConfig(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetIoOutput(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetOwm(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetSystem(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetZoneName(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleZoneIdentify(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetWebAssetsUrl(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetZoneNotifications(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetLogConfig(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleStartCaptive(AsyncWebServerRequest* req);
    void handleResetConfig(AsyncWebServerRequest* req);
    void handleWifiScan(AsyncWebServerRequest* req);
    void handleGetLogs(AsyncWebServerRequest* req);
    void handleGetDisplay(AsyncWebServerRequest* req);
    void handleSetDisplay(AsyncWebServerRequest* req, JsonDocument& doc);

    // Route de validation temporaire pour l'ecriture SD reseau (voir
    // ROADMAP.md, "Mise a jour distante des ressources Web") : depose un
    // seul fichier, nom simple uniquement (pas de sous-dossier), sous
    // /www. A remplacer par le flux manifeste + SHA-256 une fois celui-ci
    // en place ; ne pas laisser tel quel avant mise en production.
    struct DeployFileState {
        FsFile file;
        String tmpPath;
        String finalPath;
        bool openFailed = false;
    };
    void handleDeployFileBody(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void handleDeployFileComplete(AsyncWebServerRequest* req);
    // Vrai entre /api/debug/deploy-begin et /api/debug/deploy-commit : les
    // fichiers deposes vont alors dans le transit /www.new et non dans /www.
    bool _deployStagingOpen = false;

    // Route de validation temporaire pour WebAssetsUpdater::verifyOnly
    // (etape 4 du meme plan) : telecharge et verifie un fichier depuis une
    // URL fournie manuellement, n'ecrit jamais sur la SD. A remplacer par
    // un declenchement pilote par le manifeste une fois l'etape 3 exploitee
    // en conditions reelles (premier tag publie avec le manifeste Web).
    void handleVerifyWebAsset(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleVerifyWebAssetStatus(AsyncWebServerRequest* req);

    // Diagnostic temporaire, lecture seule — voir ROADMAP.md, "constat du
    // 16 aout 2026" (fragmentation memoire bloquant le handshake TLS).
    void handleHeapInfo(AsyncWebServerRequest* req);

    // Diagnostic temporaire de saturation NVS, lecture seule — voir ROADMAP.md.
    void handleNvsStats(AsyncWebServerRequest* req);

    void sendJson(AsyncWebServerRequest* req, const JsonDocument& doc, int code = 200);
    void sendOk(AsyncWebServerRequest* req);
    void sendError(AsyncWebServerRequest* req, const char* msg, int code = 400);
    void addJsonHandler(const char* uri, ArJsonRequestHandlerFunction handler);
};