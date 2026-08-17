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
#include "MaintenanceRequest.h"
#include "MaintenanceResult.h"

class DisplayManager;

class WebManager {
public:
    // Diffuse une page HTML embarquee en flash, en bornant le nombre de pages
    // servies en parallele. Voir la note detaillee dans WebManager.cpp : au-dela
    // de deux chargements simultanes, la bibliotheque perd des octets en cours
    // de route et livre une page trouee sous un Content-Length complet. Un refus
    // explicite vaut mieux qu'une page fausse.
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
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Mise a jour</title><style>body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:#101820;color:#eef;font-family:Arial,sans-serif}.card{box-sizing:border-box;width:92%;max-width:640px;padding:24px;background:#172532;border:1px solid #385064;border-radius:12px;box-shadow:0 10px 30px #0008}h1{margin:0 0 8px;font-size:24px}h2{margin:22px 0 8px;font-size:18px}p{line-height:1.5;color:#bed0dc}.warning,.result-box{padding:12px;border-radius:8px}.warning{border:1px solid #d59b35;background:#3a2b13;color:#ffd88b}.result-box{border:1px solid #385064;background:#101820;color:#d7e9f3}.result-box.ok{border-color:#41956b}.result-box.fail{border-color:#b75b5b}.result-box.update{border-color:#4fc3f7}.row{display:flex;justify-content:space-between;gap:12px;padding:4px 0}.label{color:#91aabd}.value{text-align:right;overflow-wrap:anywhere}.value.v-ok{color:#6fcf97;font-weight:700}.value.v-fail{color:#ff8a8a;font-weight:700}.value.v-info{color:#4fc3f7;font-weight:700}.value.v-muted{color:#91aabd}button,a{box-sizing:border-box;display:block;width:100%;margin-top:14px;padding:12px;border-radius:7px;text-align:center;font-size:16px;text-decoration:none}button{border:0;background:#4fc3f7;color:#06141b;font-weight:700;cursor:pointer}button.secondary{background:#526d80;color:#eef}button:disabled{opacity:.55;cursor:wait}a{border:1px solid #526d80;color:#d7e9f3}#action{min-height:24px;margin-top:14px;font-weight:700}.waitbox{display:none;margin-top:14px;padding:14px;border:1px solid #385064;border-radius:8px;background:#101820}.waitbox.active{display:block}.waitline{display:flex;align-items:center;gap:12px}.spinner{width:24px;height:24px;border:3px solid #385064;border-top-color:#4fc3f7;border-radius:50%;animation:spin .9s linear infinite}.waittext{flex:1}.elapsed{margin-top:8px;color:#91aabd;font-variant-numeric:tabular-nums}.progress{height:7px;margin-top:12px;overflow:hidden;border-radius:5px;background:#263b4b}.progress span{display:block;width:35%;height:100%;background:#4fc3f7;animation:travel 1.5s ease-in-out infinite}@keyframes spin{to{transform:rotate(360deg)}}@keyframes travel{0%{transform:translateX(-120%)}100%{transform:translateX(360%)}}</style></head><body><main class="card"><h1>Mise a jour logicielle</h1><p>La verification redemarre AquaLook en mode maintenance minimal, lit le manifeste GitHub puis revient automatiquement au fonctionnement normal.</p><div class="warning">Operation refusee pendant un arrosage. Le telechargement et l'ecriture de partition n'installent rien. Seule l'action Installer active le nouveau firmware et redemarre le module dessus.</div><h2>Derniere operation</h2><div id="last" class="result-box">Chargement...</div><button id="check" onclick="startMaintenance('check')">Verifier la version disponible</button><button id="download" onclick="startMaintenance('download')">Tester le telechargement et le SHA-256</button><button id="stage" onclick="startMaintenance('stage')">Ecrire et verifier la partition inactive</button><button id="install" onclick="startMaintenance('install')">Installer et redemarrer sur le nouveau firmware</button><button id="probe" class="secondary" onclick="startMaintenance('probe')">Tester uniquement la connexion GitHub</button><h2>Ressources Web (pages, styles, scripts)</h2><p>Mise a jour des pages servies depuis la carte SD, independante du firmware. Le module redemarre en mode maintenance, telecharge chaque fichier et verifie son empreinte avant de basculer. En cas d'echec, les pages actuelles sont conservees.</p><div id="webver" class="result-box">Chargement...</div><button id="webupd" onclick="startWebAssets()">Mettre a jour les ressources Web</button><div id="action"></div><div id="waitbox" class="waitbox"><div class="waitline"><span class="spinner"></span><span id="waittext" class="waittext">Operation en cours...</span></div><div id="elapsed" class="elapsed">Temps ecoule : 0 s</div><div class="progress"><span></span></div></div><a href="/index.html">Retour a AquaLook</a></main><script>const esc=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));const row=(a,b,c)=>'<div class="row"><span class="label">'+esc(a)+'</span><span class="value'+(c?(' '+c):'')+'">'+esc(b)+'</span></div>';async function loadLast(){const e=document.getElementById('last');try{const r=await fetch('/api/maintenance/last-result',{cache:'no-store'}),j=await r.json();if(!j.valid){e.className='result-box';e.textContent='Aucun resultat enregistre.';return}let cls=j.success?'ok':'fail';if(j.command==='check_version'&&j.success&&j.updateAvailable)cls='update';e.className='result-box '+cls;const rows=[['Operation',j.command||'Inconnue'],['Etat',j.success?'Succes':'Echec',j.success?'v-ok':'v-fail'],['HTTP',j.httpLine||'Non disponible'],['Duree TLS',j.tlsDurationMs+' ms'],['Heap minimale',j.minFreeHeap+' octets']];if(j.command==='check_version'||(j.command==='download_update_test'||j.command==='stage_update_test')){rows.push(['Version installee',j.installedVersion||'-'],['Version disponible',j.availableVersion||'-'],['Mise a jour',j.updateAvailable?'Disponible':'Non',j.updateAvailable?'v-info':'v-muted'],['Canal',j.channel||'-'],['Cible',j.target||'-'],['Environnement',j.environment||'-'],['Carte',j.board||'-'],['Taille manifeste',(j.manifestSize||0)+' octets'],['Taille firmware',(j.firmwareSize||0)+' octets']);}if((j.command==='download_update_test'||j.command==='stage_update_test')){rows.push(['Octets telecharges',(j.downloadedSize||0)+' octets'],['Duree telechargement',(j.downloadDurationMs||0)+' ms'],['SHA-256 calcule',j.calculatedSha256||'-']);}if(j.detail)rows.push(['Detail',j.detail,j.success?'v-ok':'v-fail']);e.innerHTML=rows.map(x=>row(x[0],x[1],x[2])).join('')}catch(_){e.className='result-box fail';e.textContent='Resultat indisponible.'}}const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));let waitTimer=null;function startWaitUi(kind){const box=document.getElementById('waitbox'),text=document.getElementById('waittext'),elapsed=document.getElementById('elapsed');box.classList.add('active');const started=Date.now();const messages=kind==='web'?['Redemarrage en mode maintenance...','Lecture du manifeste publie...','Telechargement et verification des fichiers...','Bascule des nouvelles pages...','Retour au fonctionnement normal...']:kind==='download'?['Redemarrage du module...','Connexion au reseau...','Telechargement du firmware...','Calcul et verification du SHA-256...','Finalisation et retour au fonctionnement normal...']:kind==='install'?['Redemarrage du module...','Activation du nouveau firmware...','Verification du demarrage...','Retour au fonctionnement normal...']:['Redemarrage du module...','Connexion au reseau...','Execution de la verification...','Finalisation et retour au fonctionnement normal...'];let index=0;text.textContent=messages[0];elapsed.textContent='Temps ecoule : 0 s';clearInterval(waitTimer);waitTimer=setInterval(()=>{const seconds=Math.floor((Date.now()-started)/1000);elapsed.textContent='Temps ecoule : '+seconds+' s';const next=Math.min(Math.floor(seconds/12),messages.length-1);if(next!==index){index=next;text.textContent=messages[index]}},1000)}function stopWaitUi(){clearInterval(waitTimer);waitTimer=null;document.getElementById('waitbox').classList.remove('active')}const resultSignature=j=>j&&j.valid?[j.command||'',j.recordedUptimeMs||0,j.detail||''].join(':'):'none';async function readLastResult(){const r=await fetch('/api/maintenance/last-result',{cache:'no-store'});if(!r.ok)throw new Error('http-'+r.status);return r.json()}async function waitForMaintenanceResult(expected,baseline,timeoutMs,out){const deadline=Date.now()+timeoutMs;let offlineSeen=false;while(Date.now()<deadline){await sleep(2000);try{const j=await readLastResult();if(j.valid&&j.command===expected&&resultSignature(j)!==baseline){out.textContent='Operation terminee. Actualisation du resultat...';await sleep(500);location.reload();return}out.textContent=offlineSeen?'AquaLook est revenu. Finalisation de l operation...':'Operation en cours sur AquaLook...'}catch(_){offlineSeen=true;out.textContent='AquaLook redemarre ou traite le firmware...'}}stopWaitUi();out.textContent='Delai d attente atteint. Rechargez la page pour consulter le resultat.';document.getElementById('check').disabled=false;document.getElementById('download').disabled=false;document.getElementById('stage').disabled=false;document.getElementById('install').disabled=false;document.getElementById('probe').disabled=false}async function startMaintenance(kind){const check=document.getElementById('check'),download=document.getElementById('download'),install=document.getElementById('install'),probe=document.getElementById('probe'),out=document.getElementById('action');const uri=kind==='check'?'/api/maintenance/check-version':kind==='download'?'/api/maintenance/download-update-test':kind==='stage'?'/api/maintenance/stage-update-test':kind==='install'?'/api/maintenance/install-update':'/api/maintenance/probe-github';const expected=kind==='check'?'check_version':kind==='download'?'download_update_test':kind==='stage'?'stage_update_test':kind==='install'?'install_update':'probe_github';const question=kind==='check'?'AquaLook va redemarrer pour verifier la version disponible. Continuer ?':kind==='download'?'Le firmware complet sera telecharge et verifie sans etre installe. Continuer ?':kind==='stage'?'Le firmware sera ecrit dans la partition inactive sans etre active. Continuer ?':kind==='install'?'Le firmware verifie sera active et le module redemarrera dessus. Une verification automatique suit le redemarrage ; en cas de redemarrages repetes sans validation, le firmware precedent est restaure automatiquement. Continuer ?':'AquaLook va redemarrer pour tester GitHub. Continuer ?';if(!confirm(question))return;check.disabled=true;download.disabled=true;document.getElementById('stage').disabled=true;install.disabled=true;probe.disabled=true;out.textContent='Preparation du redemarrage...';startWaitUi(kind);let baseline='none';try{baseline=resultSignature(await readLastResult())}catch(_){}try{const x=await fetch(uri,{method:'POST'});const j=await x.json().catch(()=>({}));if(!x.ok){out.textContent=j.error==='watering-active'?'Operation refusee : arrosage en cours.':'Erreur : '+(j.error||x.status);check.disabled=false;download.disabled=false;install.disabled=false;probe.disabled=false;stopWaitUi();return}out.textContent='Demande acceptee. Redemarrage et traitement en cours...'}catch(_){out.textContent='Connexion interrompue : AquaLook redemarre probablement.'}await waitForMaintenanceResult(expected,baseline,kind==='download'?150000:90000,out)}async function loadWebVersion(){const e=document.getElementById('webver');try{const r=await fetch('/assets-version.json',{cache:'no-store'});if(!r.ok)throw 0;const j=await r.json();e.innerHTML=row('Version des pages',j.version||j.gitSha||'inconnue','v-info')+row('Fichiers',(j.fileCount||0))+row('Origine',j.source==='manifest'?'mise a jour reseau':'synchronisation locale')}catch(_){e.className='result-box';e.textContent='Version des pages inconnue (jamais mise a jour par le reseau).'}}async function startWebAssets(){const b=document.getElementById('webupd'),out=document.getElementById('action');if(!confirm('AquaLook va redemarrer en mode maintenance pour mettre a jour les pages Web. L arrosage n est pas affecte. Continuer ?'))return;b.disabled=true;out.textContent='Preparation du redemarrage...';startWaitUi('web');try{const x=await fetch('/api/webassets/update',{method:'POST'});const j=await x.json().catch(()=>({}));if(!x.ok){out.textContent=(j.error||'').indexOf('arrosage')>=0?'Operation refusee : arrosage en cours.':'Erreur : '+(j.error||x.status);b.disabled=false;stopWaitUi();return}out.textContent='Demande acceptee. Redemarrage et mise a jour en cours...'}catch(_){out.textContent='Connexion interrompue : AquaLook redemarre probablement.'}const deadline=Date.now()+180000;while(Date.now()<deadline){await sleep(3000);try{const r=await fetch('/assets-version.json',{cache:'no-store'});if(r.ok){out.textContent='Mise a jour terminee. Actualisation...';await sleep(800);location.reload();return}}catch(_){}}stopWaitUi();out.textContent='Delai atteint. Rechargez la page pour verifier.';b.disabled=false}loadLast();loadWebVersion();</script></body></html>
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
    void handleSetOwm(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetSystem(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetZoneName(AsyncWebServerRequest* req, JsonDocument& doc);
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