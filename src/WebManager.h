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
#include "SdStaticHandler.h"
#include "EquipmentOutputRuntimeAdapter.h"

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

    // A appeler avant begin(), donc avant _server.begin().
    void registerSdStaticHandler(StorageManager* storage) {
        if (_sdStaticHandlerRegistered || !storage) return;
        _sdStaticHandlerRegistered = true;
        _server.addHandler(new SdStaticHandler(storage));
    }

    // A appeler avant begin(), donc avant _server.begin().
    void registerFaultRoutes() {
        if (_faultRoutesRegistered) return;
        _faultRoutesRegistered = true;

        _server.on("/api/logs/ack", HTTP_POST,
            [](AsyncWebServerRequest* req) {
                EventLog::ackErrors();
                EventLog::log(
                    LOG_INFO,
                    "Erreurs acquittees depuis l'interface Web"
                );
                req->send(
                    200,
                    "application/json",
                    "{\"ok\":true}"
                );
            }
        );

        _server.on("/api/faults", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                String body;
                body.reserve(96);
                body += F("{\"active\":");
                body += FaultManager::hasActiveFaults()
                    ? F("true") : F("false");
                body += F(",\"unacknowledged\":");
                body += FaultManager::hasUnacknowledgedErrors()
                    ? F("true") : F("false");
                body += F(",\"mask\":");
                body += FaultManager::activeMask();
                body += '}';

                AsyncWebServerResponse* response =
                    req->beginResponse(200, "application/json", body);
                response->addHeader(
                    "Cache-Control",
                    "no-store, no-cache, must-revalidate"
                );
                req->send(response);
            }
        );

        _server.on("/logs", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                // Fallback minimal conserve dans le firmware. La version
                // complete est servie par SdStaticHandler depuis
                // /www/logs.html lorsque la carte SD est disponible.
                static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - Journal</title><style>body{margin:0;background:#0a0a0a;color:#ddd;font-family:sans-serif}header{display:flex;gap:.7rem;align-items:center;padding:.7rem;background:#151515}button{padding:.6rem;border:0;border-radius:5px;background:#b71c1c;color:#fff;font-weight:700}a{margin-left:auto;color:#4fc3f7}iframe{width:100%;height:calc(100vh - 58px);border:0}</style></head><body><header><button onclick="fetch('/api/logs/ack',{method:'POST'}).then(()=>location.reload())">Acquitter</button><a href="/index.html">Retour</a></header><iframe src="/api/logs"></iframe></body></html>
)rawliteral";

                AsyncWebServerResponse* response =
                    req->beginResponse(
                        200,
                        "text/html; charset=utf-8",
                        PAGE
                    );
                response->addHeader(
                    "Cache-Control",
                    "no-store, no-cache, must-revalidate"
                );
                response->addHeader(
                    "X-AquaLook-Storage",
                    "Firmware-Fallback"
                );
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

        explicit operator bool() const {
            return relay != nullptr;
        }

        OutputAwareRelayState* operator->() {
            return this;
        }

        const OutputAwareRelayState* operator->() const {
            return this;
        }

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
    void handleStartCaptive(AsyncWebServerRequest* req);
    void handleResetConfig(AsyncWebServerRequest* req);
    void handleWifiScan(AsyncWebServerRequest* req);
    void handleGetLogs(AsyncWebServerRequest* req);
    void handleGetDisplay(AsyncWebServerRequest* req);
    void handleSetDisplay(AsyncWebServerRequest* req, JsonDocument& doc);

    void sendJson(AsyncWebServerRequest* req,
                  const JsonDocument& doc, int code = 200);
    void sendOk(AsyncWebServerRequest* req);
    void sendError(AsyncWebServerRequest* req,
                   const char* msg, int code = 400);
    void addJsonHandler(
        const char* uri,
        ArJsonRequestHandlerFunction handler
    );
};
