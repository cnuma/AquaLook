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

class WebManager {
public:
    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule);
    void update();

private:
    AsyncWebServer   _server{80};
    NTPManager*      _ntp      = nullptr;
    WeatherManager*  _weather  = nullptr;
    RelaisManager*   _relais   = nullptr;
    ScheduleManager* _schedule = nullptr;

    void setupRoutes();
    void handleStatus(AsyncWebServerRequest* request);
    void handleSetMode(AsyncWebServerRequest* request, JsonDocument& doc);
    void handleSetInterval(AsyncWebServerRequest* request, JsonDocument& doc);
    void handleSetDaySlot(AsyncWebServerRequest* request, JsonDocument& doc);
    void handleSetIntervalSlot(AsyncWebServerRequest* request, JsonDocument& doc);
    void handleSetRain(AsyncWebServerRequest* request, JsonDocument& doc);
    void handleForce(AsyncWebServerRequest* request, JsonDocument& doc);
    void handleManual(AsyncWebServerRequest* request, JsonDocument& doc);
};