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

// ═══════════════════════════════════════════════════════════════
//  WebManager — AsyncWebServer, toutes les routes API
//
//  Invariant I1  : LittleFS déjà monté par ConfigManager — ne pas remonter.
//  Invariant I2  : seul ConfigManager écrit en flash.
//  Invariant I3  : setters persistés → double appel RAM + Config.
//  Invariant I10 : handleSetWifi envoie sendOk() AVANT setWifi().
//  Invariant I18 : EventBus::displayDirty positionné après tout POST
//                  qui modifie l'état visible.
// ═══════════════════════════════════════════════════════════════
class WebManager {
public:
    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule,
               ConfigManager* config, WiFiManager* wifi = nullptr);

    // Exécute dans loop() les écritures flash différées par les callbacks AsyncTCP.
    // LittleFS ne doit jamais être écrit directement depuis _async_service_task.
    void update();

private:
    AsyncWebServer   _server  { 80 };
    NTPManager*      _ntp      = nullptr;
    WeatherManager*  _weather  = nullptr;
    RelaisManager*   _relais   = nullptr;
    ScheduleManager* _schedule = nullptr;
    ConfigManager*   _config   = nullptr;
    WiFiManager*     _wifi     = nullptr;

    // Requête système différée : le callback HTTP ne fait qu'enregistrer la
    // demande. La sérialisation LittleFS est exécutée ensuite dans loop().
    portMUX_TYPE      _pendingMux = portMUX_INITIALIZER_UNLOCKED;
    volatile bool     _systemSavePending = false;
    volatile uint32_t _systemSaveAtMs = 0;
    CfgSystem         _pendingSystem;
    uint16_t          _pendingManualDuration = 10;
    bool              _pendingManualDurationValid = false;
    bool              _pendingSystemReboot = false;

    void setupRoutes();
    void setupCaptiveRoutes();  // page de config portail captif

    // ── Handlers GET ──────────────────────────
    void handleStatus(AsyncWebServerRequest* request);
    void handleAdminStatus(AsyncWebServerRequest* request);
    void handleDiagnostics(AsyncWebServerRequest* request);

    // ── Handlers POST planning ────────────────
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

    // ── Handlers POST config (nouveau v2) ─────
    void handleSetWifi(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetTouch(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetNtp(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetOwm(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetSystem(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleSetZoneName(AsyncWebServerRequest* req, JsonDocument& doc);
    void handleStartCaptive(AsyncWebServerRequest* req);
    void handleResetConfig(AsyncWebServerRequest* req);
    // Scan réseau WiFi (portail captif)
    void handleWifiScan(AsyncWebServerRequest* req);
    // Journal d'événements (lien caché — diagnostic)
    void handleGetLogs(AsyncWebServerRequest* req);
    // Affichage LCD — tokens de design (hot-reload, pas de reboot)
    void handleGetDisplay(AsyncWebServerRequest* req);
    void handleSetDisplay(AsyncWebServerRequest* req, JsonDocument& doc);

    // ── Helpers ───────────────────────────────
    void sendJson(AsyncWebServerRequest* req, const JsonDocument& doc, int code = 200);
    void sendOk(AsyncWebServerRequest* req);
    void sendError(AsyncWebServerRequest* req, const char* msg, int code = 400);
    void addJsonHandler(const char* uri,
                        ArJsonRequestHandlerFunction handler);
};
