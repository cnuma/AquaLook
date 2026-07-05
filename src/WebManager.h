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
                static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AquaLook - Journal</title>
<style>
body{margin:0;background:#0a0a0a;color:#ddd;font-family:sans-serif}
header{display:flex;gap:.7rem;align-items:center;flex-wrap:wrap;padding:.8rem 1rem;background:#151515;position:sticky;top:0;z-index:2}
button{border:1px solid #ef5350;border-radius:6px;padding:.65rem 1rem;background:#b71c1c;color:white;font-weight:700;cursor:pointer}
button.acked{cursor:default;color:#d7ffd9;border-color:#43a047;background:repeating-linear-gradient(135deg,#1b5e20 0,#1b5e20 7px,#263238 7px,#263238 14px)}
button:disabled{opacity:1}
#state{font-size:.9rem;color:#aaa}
#state.active{color:#ff6b6b;font-weight:700}
#state.acked{color:#81c784;font-weight:700}
iframe{width:100%;height:calc(100vh - 72px);border:0}
</style></head><body>
<header>
<button id="ack-btn" onclick="ack()">Acquitter les erreurs</button>
<span id="state">Lecture...</span>
<a href="/index.html" style="margin-left:auto;color:#4fc3f7;text-decoration:none">Retour</a>
</header>
<iframe id="journal" src="/api/logs"></iframe>
<script>
async function refreshState(){
  const btn=document.getElementById('ack-btn');
  const state=document.getElementById('state');

  try{
    const r=await fetch('/api/faults',{cache:'no-store'});
    const d=await r.json();

    btn.classList.remove('acked');
    state.classList.remove('active','acked');

    if(d.unacknowledged){
      btn.disabled=false;
      btn.textContent='Acquitter les erreurs';
      state.textContent=d.active
        ? 'Erreur active non acquittee'
        : 'Erreur memorisee non acquittee';
      state.classList.add('active');
    }else{
      btn.disabled=true;
      btn.textContent='Erreurs acquittees';
      btn.classList.add('acked');

      if(d.active){
        state.textContent=
          'Defaut toujours present - rappel rouge toutes les 5 s';
        state.classList.add('acked');
      }else{
        state.textContent='Aucune alarme non acquittee';
        state.classList.add('acked');
      }
    }
  }catch(e){
    state.textContent='Etat indisponible';
  }
}

async function ack(){
  const btn=document.getElementById('ack-btn');
  if(btn.disabled) return;

  await fetch(
    '/api/logs/ack',
    {method:'POST',cache:'no-store'}
  );

  document.getElementById('journal')
    .contentWindow.location.reload();

  await refreshState();
}

refreshState();
setInterval(refreshState,2000);
</script></body></html>
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
                req->send(response);
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
