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

class WebManager {
public:
    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule,
               ConfigManager* config, WiFiManager* wifi = nullptr);

    void update();

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
                req->send(200, "application/json", body);
            }
        );

        _server.on("/logs", HTTP_GET,
            [](AsyncWebServerRequest* req) {
                static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AquaLook - Journal</title>
<style>
body{margin:0;background:#0a0a0a;color:#ddd;font-family:sans-serif}
header{display:flex;gap:.7rem;align-items:center;padding:.8rem 1rem;background:#151515}
button{border:0;border-radius:6px;padding:.6rem .9rem;background:#b71c1c;color:white;font-weight:600;cursor:pointer}
#state{font-size:.9rem;color:#aaa}
iframe{width:100%;height:calc(100vh - 65px);border:0}
</style></head><body>
<header>
<button onclick="ack()">Acquitter les erreurs</button>
<span id="state">Lecture...</span>
</header>
<iframe id="journal" src="/api/logs"></iframe>
<script>
async function refreshState(){
  try{
    const r=await fetch('/api/faults',{cache:'no-store'});
    const d=await r.json();
    document.getElementById('state').textContent=
      d.active
      ? (d.unacknowledged
         ? 'Probleme actif non acquitte'
         : 'Probleme actif acquitte, rappel rouge toutes les 5 s')
      : (d.unacknowledged
         ? 'Erreur memorisee non acquittee'
         : 'Aucune alarme');
  }catch(e){
    document.getElementById('state').textContent='Etat indisponible';
  }
}
async function ack(){
  await fetch('/api/logs/ack',{method:'POST'});
  document.getElementById('journal').contentWindow.location.reload();
  refreshState();
}
refreshState();
setInterval(refreshState,2000);
</script></body></html>
)rawliteral";
                req->send(200, "text/html; charset=utf-8", PAGE);
            }
        );
    }

private:
    AsyncWebServer _server { 80 };
    NTPManager* _ntp = nullptr;
    WeatherManager* _weather = nullptr;
    RelaisManager* _relais = nullptr;
    ScheduleManager* _schedule = nullptr;
    ConfigManager* _config = nullptr;
    WiFiManager* _wifi = nullptr;
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
