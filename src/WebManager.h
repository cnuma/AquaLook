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
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Mises a jour</title><style>body{margin:0;min-height:100vh;display:flex;justify-content:center;background:#101820;color:#eef;font-family:Arial,sans-serif}.card{box-sizing:border-box;width:94%;max-width:680px;padding:22px;margin:18px 0;background:#172532;border:1px solid #385064;border-radius:12px}h1{margin:0 0 4px;font-size:23px}h2{margin:26px 0 10px;font-size:17px;color:#bed0dc}p{line-height:1.5;color:#bed0dc;margin:6px 0}.sub{color:#91aabd;margin:0 0 16px}.chan{display:flex;align-items:center;gap:10px;padding:10px 12px;border:1px solid #385064;border-radius:8px;margin-bottom:8px;background:#101820}.chan-n{flex:1;font-weight:700}.chan-v{font-family:monospace;font-size:12px;color:#91aabd}.chip{font-size:11px;padding:2px 9px;border-radius:99px;border:1px solid #526d80;color:#91aabd;white-space:nowrap}.chip.up{color:#6fcf97;border-color:#41956b}.chip.av{color:#b388ff;border-color:#7e57c2;font-weight:700}.step{border:1px solid #385064;border-radius:9px;padding:14px;margin-bottom:10px;background:#101820}.step.on{border-color:#4fc3f7}.step.off{opacity:.62}.step-h{display:flex;align-items:center;gap:10px;margin-bottom:6px}.num{flex:none;width:24px;height:24px;border-radius:50%;background:#385064;color:#eef;display:flex;align-items:center;justify-content:center;font-weight:700;font-size:13px}.step.on .num{background:#4fc3f7;color:#06141b}.step-t{font-weight:700;font-size:15px}.why{margin:8px 0 0;padding:8px 10px;border-left:3px solid #d59b35;background:#2a2114;color:#ffd88b;font-size:13px;border-radius:0 6px 6px 0}button{box-sizing:border-box;display:block;width:100%;margin-top:10px;padding:11px;border:0;border-radius:7px;font-size:15px;font-weight:700;background:#4fc3f7;color:#06141b;cursor:pointer}button.sec{background:#526d80;color:#eef;font-weight:400}button.pu{background:#7e57c2;color:#fff}button:disabled{opacity:.45;cursor:not-allowed}a.back{display:block;margin-top:18px;padding:11px;border:1px solid #526d80;border-radius:7px;color:#d7e9f3;text-align:center;text-decoration:none}details{margin-top:10px;border:1px solid #385064;border-radius:8px;padding:10px 12px;background:#101820}summary{cursor:pointer;color:#91aabd}.row{display:flex;justify-content:space-between;gap:12px;padding:3px 0;font-size:13px}.label{color:#91aabd}.value{text-align:right;overflow-wrap:anywhere}.v-ok{color:#6fcf97;font-weight:700}.v-fail{color:#ff8a8a;font-weight:700}.v-info{color:#4fc3f7;font-weight:700}#action{min-height:22px;margin-top:12px;font-weight:700}.wait{display:none;margin-top:12px;padding:13px;border:1px solid #385064;border-radius:8px;background:#101820}.wait.on{display:block}.wl{display:flex;align-items:center;gap:12px}.sp{width:22px;height:22px;border:3px solid #385064;border-top-color:#4fc3f7;border-radius:50%;animation:s .9s linear infinite}.el{margin-top:8px;color:#91aabd}.pg{height:6px;margin-top:10px;overflow:hidden;border-radius:4px;background:#263b4b}.pg span{display:block;width:35%;height:100%;background:#4fc3f7;animation:t 1.5s ease-in-out infinite}@keyframes s{to{transform:rotate(360deg)}}@keyframes t{0%{transform:translateX(-120%)}100%{transform:translateX(360%)}}</style></head><body><main class="card"><h1>Mises a jour</h1><p class="sub">Tout ce qui met AquaLook a jour est reuni ici, dans l'ordre ou il faut le faire. Aucune operation n'est possible pendant un arrosage.</p><h2>Etat</h2><div id="state"><div class="chan"><span class="chan-n">Lecture...</span></div></div><h2>Programme du module</h2><p class="sub">Trois etapes, a suivre dans l'ordre. Chacune s'active quand la precedente a reussi.</p><div id="s1" class="step"><div class="step-h"><span class="num">1</span><span class="step-t">Verifier ce qui est disponible</span></div><p>Redemarre environ 30 secondes en mode maintenance, interroge les deux sources, puis revient tout seul. N'installe rien.</p><button id="b-check" onclick="go('check')">Verifier</button></div><div id="s2" class="step"><div class="step-h"><span class="num">2</span><span class="step-t">Preparer le nouveau programme</span></div><p>Telecharge le programme, verifie son empreinte SHA-256, et l'ecrit dans la partition de reserve. Le module continue de tourner sur l'ancien : rien n'est active a cette etape.</p><div id="w2" class="why"></div><button id="b-stage" onclick="go('stage')">Preparer</button></div><div id="s3" class="step"><div class="step-h"><span class="num">3</span><span class="step-t">Installer et redemarrer</span></div><p>Bascule sur le programme prepare et redemarre dessus. Si le module ne redemarre pas correctement, l'ancien programme est restaure automatiquement.</p><div id="w3" class="why"></div><button id="b-install" onclick="go('install')">Installer</button></div><h2>Pages Web</h2><p class="sub">Les pages, styles et scripts servis depuis la carte SD. Independants du programme du module : cette mise a jour ne touche pas au firmware, et une seule etape suffit.</p><div class="step on"><p>Le module redemarre en mode maintenance, telecharge chaque fichier, verifie son empreinte, puis bascule. Si quoi que ce soit echoue, les pages actuelles sont conservees.</p><button id="b-web" class="pu" onclick="goWeb()">Mettre a jour les pages Web</button></div><h2>Derniere operation</h2><div id="last" class="step">Lecture...</div><div id="action"></div><div id="wait" class="wait"><div class="wl"><span class="sp"></span><span id="wt">Operation en cours...</span></div><div id="el" class="el">Temps ecoule : 0 s</div><div class="pg"><span></span></div></div><details><summary>Diagnostics et depot direct</summary><p>Ces outils ne mettent rien a jour. Ils servent a comprendre pourquoi une etape echoue.</p><button class="sec" onclick="go('probe')">Tester uniquement la connexion a la source</button><button class="sec" onclick="go('download')">Telecharger et verifier l'empreinte, sans rien ecrire</button><p><b>Depot direct depuis un ordinateur.</b> Troisieme voie, reservee au depannage : quand la mise a jour reseau des pages echoue, un ordinateur du meme reseau peut deposer les fichiers directement, sans redemarrage, via <span class="chan-v">/api/debug/deploy-begin</span>, <span class="chan-v">/api/debug/deploy-file?name=&lt;nom&gt;</span> puis <span class="chan-v">/api/debug/deploy-commit</span>. Le depot est transactionnel : les fichiers transitent par un repertoire separe et ne remplacent les pages en service qu'a la derniere etape.</p></details><a class="back" href="/index.html">Retour a AquaLook</a></main><script>
const esc=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const row=(a,b,c)=>'<div class="row"><span class="label">'+esc(a)+'</span><span class="value'+(c?' '+c:'')+'">'+esc(b)+'</span></div>';
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
let LAST=null,WEB=null,timer=null;
async function readLast(){const r=await fetch('/api/maintenance/last-result',{cache:'no-store'});if(!r.ok)throw new Error('http-'+r.status);return r.json()}
// Memes conditions que celles appliquees par le module (WebManager.h) : la
// page ne doit jamais proposer une etape qui sera refusee. Les recopier ici
// est une duplication assumee -- l'alternative, tenter et afficher l'erreur,
// obligerait a redemarrer le module pour apprendre que c'etait impossible.
const fwReady=j=>!!(j&&j.valid&&j.success&&j.updateAvailable&&j.firmwareUrl&&j.firmwareSize&&j.sha256);
const staged=j=>!!(j&&j.valid&&j.success&&j.command==='stage_update_test'&&j.calculatedSha256&&j.calculatedSha256===j.sha256);
function chan(n,inst,av,txt,cls){return '<div class="chan"><span class="chan-n">'+esc(n)+'</span><span class="chan-v">'+esc(inst||'?')+(av?' &rarr; '+esc(av):'')+'</span><span class="chip '+cls+'">'+esc(txt)+'</span></div>'}
function renderState(){const e=document.getElementById('state');const j=LAST;const checked=j&&j.valid&&j.command==='check_version';let h='';
h+=checked?(j.updateAvailable?chan('Programme du module',j.installedVersion,j.availableVersion,'a installer','av'):chan('Programme du module',j.installedVersion,null,'a jour','up')):chan('Programme du module',(j&&j.installedVersion)||null,null,'jamais verifie','');
const wi=WEB?(WEB.version||WEB.gitSha):null;
h+=checked?(j.webAssetsUpdateAvailable?chan('Pages Web',wi||j.webAssetsInstalledVersion,j.webAssetsAvailableVersion,'a installer','av'):chan('Pages Web',wi,null,'a jour','up')):chan('Pages Web',wi,null,'jamais verifie','');
e.innerHTML=h;
const ok2=fwReady(LAST),ok3=staged(LAST);
step('s2','b-stage','w2',ok2,checked&&!LAST.updateAvailable?'Aucune mise a jour du programme n\'est disponible : il n\'y a rien a preparer.':'Lancez d\'abord l\'etape 1. Le module ne garde qu\'un seul resultat : une mise a jour des pages Web efface le resultat de la verification, il faut alors la relancer.');
step('s3','b-install','w3',ok3,'Lancez d\'abord l\'etape 2 : rien n\'a ete prepare et verifie dans la partition de reserve.');
document.getElementById('s1').className='step on';
const wb=document.getElementById('b-web');if(checked&&j.webAssetsUpdateAvailable){wb.textContent='Mettre a jour les pages Web ('+esc(j.webAssetsAvailableVersion)+')'}}
function step(sid,bid,wid,ok,why){const s=document.getElementById(sid),b=document.getElementById(bid),w=document.getElementById(wid);s.className='step '+(ok?'on':'off');b.disabled=!ok;w.style.display=ok?'none':'block';w.textContent=ok?'':why}
async function loadAll(){try{LAST=await readLast()}catch(_){LAST=null}
try{const r=await fetch('/assets-version.json',{cache:'no-store'});WEB=r.ok?await r.json():null}catch(_){WEB=null}
renderState();renderLast()}
function renderLast(){const e=document.getElementById('last'),j=LAST;if(!j||!j.valid){e.textContent='Aucune operation enregistree.';return}
const rows=[['Operation',j.command||'inconnue'],['Resultat',j.success?'reussie':'echouee',j.success?'v-ok':'v-fail']];
if(j.httpLine)rows.push(['HTTP',j.httpLine]);
if(j.command==='check_version'){rows.push(['Version installee',j.installedVersion||'-'],['Version disponible',j.availableVersion||'-'],['Pages installees',j.webAssetsInstalledVersion||'-'],['Pages disponibles',j.webAssetsAvailableVersion||'-'])}
if(j.command==='download_update_test'||j.command==='stage_update_test'){rows.push(['Octets telecharges',(j.downloadedSize||0)],['Empreinte attendue',j.sha256||'-'],['Empreinte calculee',j.calculatedSha256||'-',j.calculatedSha256&&j.calculatedSha256===j.sha256?'v-ok':'v-fail'])}
if(j.detail)rows.push(['Detail',j.detail,j.success?'v-ok':'v-fail']);
rows.push(['Memoire libre minimale',(j.minFreeHeap||0)+' octets']);
e.innerHTML=rows.map(x=>row(x[0],x[1],x[2])).join('')}
function startWait(k){const b=document.getElementById('wait'),t=document.getElementById('wt'),el=document.getElementById('el');b.classList.add('on');const t0=Date.now();
const m=k==='web'?['Redemarrage en mode maintenance...','Lecture du manifeste publie...','Telechargement et verification des fichiers...','Bascule des nouvelles pages...','Retour au fonctionnement normal...']:k==='install'?['Redemarrage du module...','Activation du nouveau programme...','Verification du demarrage...','Retour au fonctionnement normal...']:['Redemarrage du module...','Connexion au reseau...','Operation en cours...','Retour au fonctionnement normal...'];
let i=0;t.textContent=m[0];el.textContent='Temps ecoule : 0 s';clearInterval(timer);
timer=setInterval(()=>{const s=Math.floor((Date.now()-t0)/1000);el.textContent='Temps ecoule : '+s+' s';const n=Math.min(Math.floor(s/12),m.length-1);if(n!==i){i=n;t.textContent=m[i]}},1000)}
function stopWait(){clearInterval(timer);timer=null;document.getElementById('wait').classList.remove('on')}
const sig=j=>j&&j.valid?[j.command||'',j.recordedUptimeMs||0,j.detail||''].join(':'):'none';
function busy(v){['b-check','b-stage','b-install','b-web'].forEach(id=>{const e=document.getElementById(id);if(e)e.disabled=v})}
async function go(k){
const uri={check:'/api/maintenance/check-version',download:'/api/maintenance/download-update-test',stage:'/api/maintenance/stage-update-test',install:'/api/maintenance/install-update',probe:'/api/maintenance/probe-github'}[k];
const exp={check:'check_version',download:'download_update_test',stage:'stage_update_test',install:'install_update',probe:'probe_github'}[k];
const q={check:'AquaLook va redemarrer environ 30 secondes pour verifier les versions disponibles. Rien ne sera installe. Continuer ?',download:'Le programme sera telecharge et son empreinte verifiee, sans rien ecrire. Continuer ?',stage:'Le programme sera telecharge, verifie, puis ecrit dans la partition de reserve. Il ne sera PAS active. Continuer ?',install:'Le programme prepare va etre active et le module redemarrera dessus. En cas de redemarrages repetes sans validation, le programme precedent est restaure automatiquement. Continuer ?',probe:'AquaLook va redemarrer pour tester la connexion a la source. Continuer ?'}[k];
if(!confirm(q))return;
const out=document.getElementById('action');busy(true);out.textContent='Preparation du redemarrage...';startWait(k);
let base='none';try{base=sig(await readLast())}catch(_){}
try{const x=await fetch(uri,{method:'POST'});const j=await x.json().catch(()=>({}));
if(!x.ok){out.textContent=j.error==='watering-active'?'Refuse : un arrosage est en cours. Reessayez apres.':j.error==='check-version-required'?'Refuse : relancez l\'etape 1, le module n\'a plus de verification valide en memoire.':j.error==='stage-required'?'Refuse : l\'etape 2 n\'a pas ete faite.':'Erreur : '+(j.error||x.status);
stopWait();await loadAll();return}
out.textContent='Demande acceptee. Redemarrage et traitement en cours...'}catch(_){out.textContent='Connexion interrompue : le module redemarre.'}
await waitResult(exp,base,k==='download'?150000:90000,out)}
async function waitResult(exp,base,ms,out){const dl=Date.now()+ms;let off=false;
while(Date.now()<dl){await sleep(2000);
try{const j=await readLast();if(j.valid&&j.command===exp&&sig(j)!==base){out.textContent='Operation terminee.';await sleep(400);location.reload();return}
out.textContent=off?'Le module est revenu. Finalisation...':'Operation en cours sur le module...'}catch(_){off=true;out.textContent='Le module redemarre...'}}
stopWait();out.textContent='Delai atteint. Rechargez la page pour voir le resultat.';await loadAll()}
async function goWeb(){const out=document.getElementById('action');
if(!confirm('AquaLook va redemarrer en mode maintenance pour mettre a jour les pages Web. L\'arrosage n\'est pas affecte. Continuer ?'))return;
busy(true);out.textContent='Preparation du redemarrage...';startWait('web');
try{const x=await fetch('/api/webassets/update',{method:'POST'});const j=await x.json().catch(()=>({}));
if(!x.ok){out.textContent=(j.error||'').indexOf('arrosage')>=0?'Refuse : un arrosage est en cours. Reessayez apres.':'Erreur : '+(j.error||x.status);stopWait();await loadAll();return}
out.textContent='Demande acceptee. Redemarrage et mise a jour en cours...'}catch(_){out.textContent='Connexion interrompue : le module redemarre.'}
const dl=Date.now()+180000;
while(Date.now()<dl){await sleep(3000);try{const r=await fetch('/assets-version.json',{cache:'no-store'});if(r.ok){out.textContent='Mise a jour terminee.';await sleep(600);location.reload();return}}catch(_){}}
stopWait();out.textContent='Delai atteint. Rechargez la page pour verifier.';await loadAll()}
loadAll();
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