#pragma once
#include <Arduino.h>
#include <cstring>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "InputSampler.h"
#include "ScriptRunner.h"
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

    // Lecture d'entree, injectee plutot qu'appelee directement : WebManager
    // n'a pas a connaitre le pilote V4, et le jour ou une entree viendra d'un
    // autre transport, seul l'injecteur changera.
    void setInputSampler(const InputSampler* sampler) { _inputs = sampler; }
    void setScriptRunner(ScriptRunner* runner) { _scripts = runner; }

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

        // La page complete de mise a jour vit sur la carte SD, servie comme
        // n'importe quelle autre ressource Web. Elle pesait 27 Ko dans le
        // programme du module -- pour 19,5 Ko de JavaScript.
        //
        // Elle n'est pourtant PAS une ressource comme les autres : c'est par
        // elle qu'on repare un module. Elle doit donc rester atteignable quand
        // la carte SD manque, ce qui interdit de simplement la deplacer.
        //
        // D'ou les deux chemins : la carte quand elle repond, et sinon une
        // page de secours integree qui garde les quatre operations
        // essentielles -- verifier, preparer, installer, mettre a jour les
        // pages. Ce qu'elle perd, ce sont les explications et le detail
        // technique, pas la capacite d'agir.
        _server.on("/ota", HTTP_GET,
            [this](AsyncWebServerRequest* req) {
                if (_storage && _storage->isSdAvailable() &&
                    _storage->existsOnSd("/www/ota.html")) {
                    req->redirect("/ota.html");
                    return;
                }
                static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Mises a jour (secours)</title><style>body{margin:0;background:#101820;color:#eef;font-family:Arial,sans-serif;display:flex;justify-content:center}.card{box-sizing:border-box;width:94%;max-width:640px;padding:20px;margin:16px 0;background:#172532;border:1px solid #385064;border-radius:12px}h1{margin:0 0 6px;font-size:21px}p{line-height:1.5;color:#bed0dc;margin:6px 0;font-size:14px}.warn{border:1px solid #d59b35;background:#2a2114;color:#ffd88b;border-radius:8px;padding:12px;margin:12px 0;font-size:13.5px}button{box-sizing:border-box;display:block;width:100%;margin-top:10px;padding:11px;border:0;border-radius:7px;font-size:15px;font-weight:700;background:#4fc3f7;color:#06141b;cursor:pointer}button.pu{background:#7e57c2;color:#fff}button:disabled{opacity:.42;cursor:not-allowed}#msg{margin-top:12px;padding:10px;border:1px solid #385064;border-radius:7px;background:#101820;font-size:13.5px;color:#d7e9f3;min-height:19px;overflow-wrap:anywhere}a.back{display:block;margin-top:16px;padding:11px;border:1px solid #526d80;border-radius:7px;color:#d7e9f3;text-align:center;text-decoration:none}</style></head><body><main class="card"><h1>Mises a jour</h1><p>Page de secours, integree au programme du module.</p><div class="warn">La page complete est servie depuis la carte SD et n'a pas pu etre lue. Les quatre operations essentielles restent disponibles ci-dessous ; les explications detaillees, elles, vivaient sur la carte.</div><p><b>Dans cet ordre.</b> Chaque etape redemarre le module une trentaine de secondes en mode maintenance, puis il revient seul. Aucune n'est possible pendant un arrosage.</p><button id="b1" onclick="go('/api/maintenance/check-version','Verification')">1. Verifier ce qui est disponible</button><button id="b2" onclick="go('/api/maintenance/stage-update-test','Preparation')">2. Preparer le nouveau programme</button><button id="b3" onclick="go('/api/maintenance/install-update','Installation')">3. Installer et redemarrer</button><button id="b4" class="pu" onclick="go('/api/webassets/update','Pages Web')">Mettre a jour les pages Web</button><div id="msg">Pret.</div><a class="back" href="/index.html">Retour a AquaLook</a></main><script>
function go(url,label){
  if(!confirm(label+' : le module va redemarrer en mode maintenance. Continuer ?'))return;
  for(const id of ['b1','b2','b3','b4'])document.getElementById(id).disabled=true;
  const m=document.getElementById('msg');
  m.textContent=label+' demandee, le module redemarre...';
  fetch(url,{method:'POST'}).then(r=>r.text()).then(t=>{
    m.textContent=label+' acceptee. Rechargez cette page dans une minute. Reponse : '+t.slice(0,200);
  }).catch(()=>{
    m.textContent=label+' envoyee. Le module a coupe la connexion, ce qui est normal : il redemarre. Rechargez dans une minute.';
  });
}
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
    const InputSampler* _inputs = nullptr;
    ScriptRunner* _scripts = nullptr;
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
    void handleRestart(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetApiSecret(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSaveScript(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleEraseScript(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleRunScript(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetZoneName(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleZoneIdentify(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetWebAssetsUrl(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetZoneNotifications(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetLogConfig(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleStartCaptive(AsyncWebServerRequest* req);
    void handleResetConfig(AsyncWebServerRequest* req);
    // Topologie relais : persister celle en vigueur, ou revenir au legacy.
    void handlePersistTopology(AsyncWebServerRequest* req);
    void handleResetTopology(AsyncWebServerRequest* req);
    void handleGetTopology(AsyncWebServerRequest* req);
    void handleSetTopology(AsyncWebServerRequest* req, JsonDocument& doc);
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