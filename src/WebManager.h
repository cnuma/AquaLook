#pragma once
#include <Arduino.h>
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
#include "MaintenanceRequest.h"

class WebManager {
public:
    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule,
               ConfigManager* config, WiFiManager* wifi = nullptr);

    void update();

    void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
        _outputs = outputs;
        _relais.outputs = outputs;
    }

    void registerSdStaticHandler(StorageManager* storage) {
        if (_sdStaticHandlerRegistered || !storage) return;
        _sdStaticHandlerRegistered = true;
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

        _server.on("/ota", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Mise a jour</title><style>body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:#101820;color:#eef;font-family:Arial,sans-serif}.card{box-sizing:border-box;width:92%;max-width:520px;padding:24px;background:#172532;border:1px solid #385064;border-radius:12px;box-shadow:0 10px 30px #0008}h1{margin:0 0 8px;font-size:24px}p{line-height:1.5;color:#bed0dc}.warning{padding:12px;border:1px solid #d59b35;border-radius:8px;background:#3a2b13;color:#ffd88b}button,a{box-sizing:border-box;display:block;width:100%;margin-top:14px;padding:12px;border-radius:7px;text-align:center;font-size:16px;text-decoration:none}button{border:0;background:#4fc3f7;color:#06141b;font-weight:700;cursor:pointer}button:disabled{opacity:.55;cursor:wait}a{border:1px solid #526d80;color:#d7e9f3}#result{min-height:24px;margin-top:14px;font-weight:700}</style></head><body><main class="card"><h1>Mise a jour logicielle</h1><p>Ce test redemarre AquaLook en mode maintenance minimal, verifie l'acces HTTPS a GitHub, puis revient automatiquement au fonctionnement normal.</p><div class="warning">Le test est refuse si une zone d'arrosage est active. Aucune partition OTA n'est ecrite.</div><button id="probe" onclick="startProbe()">Tester GitHub maintenant</button><div id="result"></div><a href="/index.html">Retour a AquaLook</a></main><script>async function startProbe(){const b=document.getElementById('probe'),r=document.getElementById('result');if(!confirm('AquaLook va redemarrer pour tester GitHub. Continuer ?'))return;b.disabled=true;r.textContent='Preparation du redemarrage...';try{const x=await fetch('/api/maintenance/probe-github',{method:'POST'});const j=await x.json().catch(()=>({}));if(!x.ok){r.textContent=j.error==='watering-active'?'Test refuse : arrosage en cours.':'Erreur : '+(j.error||x.status);b.disabled=false;return}r.textContent='Demande acceptee. Redemarrage en cours...';setTimeout(()=>{r.textContent='Test en cours. AquaLook reviendra automatiquement.'},1500)}catch(e){r.textContent='Connexion interrompue : le module redemarre probablement.'}}</script></body></html>
)rawliteral";
                AsyncWebServerResponse* response = req->beginResponse(
                    200, "text/html; charset=utf-8", PAGE);
                response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                req->send(response);
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

        _server.on("/logs", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Journal</title></head><body><p>Interface de secours. Le fichier complet est servi depuis la carte SD.</p><a href="/api/logs.txt">Journal brut</a></body></html>
)rawliteral";
                AsyncWebServerResponse* response = req->beginResponse(
                    200, "text/html; charset=utf-8", PAGE);
                response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                response->addHeader("X-AquaLook-Storage", "Firmware-Fallback");
                req->send(response);
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
    void handleSetTouch(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetNtp(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetOwm(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetSystem(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetZoneName(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetZoneNotifications(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleStartCaptive(AsyncWebServerRequest* req);
    void handleResetConfig(AsyncWebServerRequest* req);
    void handleWifiScan(AsyncWebServerRequest* req);
    void handleGetLogs(AsyncWebServerRequest* req);
    void handleGetDisplay(AsyncWebServerRequest* req);
    void handleSetDisplay(AsyncWebServerRequest* req, JsonDocument& doc);
    void sendJson(AsyncWebServerRequest* req, const JsonDocument& doc, int code = 200);
    void sendOk(AsyncWebServerRequest* req);
    void sendError(AsyncWebServerRequest* req, const char* msg, int code = 400);
    void addJsonHandler(const char* uri, ArJsonRequestHandlerFunction handler);
};