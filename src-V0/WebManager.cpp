#include "WebManager.h"

void WebManager::begin(NTPManager* ntp, WeatherManager* weather,
                       RelaisManager* relais, ScheduleManager* schedule) {
    _ntp      = ntp;
    _weather  = weather;
    _relais   = relais;
    _schedule = schedule;

    setupRoutes();
    _server.begin();
    Serial.println("[Web] Serveur démarré sur port 80");
}

void WebManager::update() { }

void WebManager::setupRoutes() {
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/index.html");
    });

    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStatus(request);
    });

    // ── Mode ──────────────────────────────────
    auto* modeHandler = new AsyncCallbackJsonWebHandler(
        "/api/mode",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            JsonDocument doc; doc.set(json);
            handleSetMode(request, doc);
        }
    );
    modeHandler->setMethod(HTTP_POST);
    _server.addHandler(modeHandler);

    // ── Intervalle ────────────────────────────
    auto* intervalHandler = new AsyncCallbackJsonWebHandler(
        "/api/interval",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            JsonDocument doc; doc.set(json);
            handleSetInterval(request, doc);
        }
    );
    intervalHandler->setMethod(HTTP_POST);
    _server.addHandler(intervalHandler);

    // ── Slot jour ─────────────────────────────
    auto* daySlotHandler = new AsyncCallbackJsonWebHandler(
        "/api/dayslot",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            JsonDocument doc; doc.set(json);
            handleSetDaySlot(request, doc);
        }
    );
    daySlotHandler->setMethod(HTTP_POST);
    _server.addHandler(daySlotHandler);

    // ── Slot intervalle ───────────────────────
    auto* intervalSlotHandler = new AsyncCallbackJsonWebHandler(
        "/api/intervalslot",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            JsonDocument doc; doc.set(json);
            handleSetIntervalSlot(request, doc);
        }
    );
    intervalSlotHandler->setMethod(HTTP_POST);
    _server.addHandler(intervalSlotHandler);

    // ── Pluie ─────────────────────────────────
    auto* rainHandler = new AsyncCallbackJsonWebHandler(
        "/api/rain",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            JsonDocument doc; doc.set(json);
            handleSetRain(request, doc);
        }
    );
    rainHandler->setMethod(HTTP_POST);
    _server.addHandler(rainHandler);

    // ── Forcer ───────────────────────────────
    auto* forceHandler = new AsyncCallbackJsonWebHandler(
        "/api/force",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            JsonDocument doc; doc.set(json);
            handleForce(request, doc);
        }
    );
    forceHandler->setMethod(HTTP_POST);
    _server.addHandler(forceHandler);

    // ── Manuel ───────────────────────────────
    auto* manualHandler = new AsyncCallbackJsonWebHandler(
        "/api/manual",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            JsonDocument doc; doc.set(json);
            handleManual(request, doc);
        }
    );
    manualHandler->setMethod(HTTP_POST);
    _server.addHandler(manualHandler);

    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    _server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });
}

void WebManager::handleStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;

    doc["time"]   = _ntp->getTimeStr();
    doc["synced"] = _ntp->isSynced();

    doc["rain"]["expected"] = _weather->isRainExpected();
    doc["rain"]["mm"]       = _weather->getRainMm();
    doc["rain"]["status"]   = _weather->getStatusStr();

    JsonArray zones = doc["zones"].to<JsonArray>();
    for (uint8_t z = 0; z < NB_ZONES; z++) {
        JsonObject zo   = zones.add<JsonObject>();
        ZoneSchedule s  = _schedule->getZoneSchedule(z);

        zo["id"]         = z;
        zo["active"]     = _relais->getState(z);
        zo["mode"]       = s.mode;
        zo["interval"]   = s.intervalDays;
        zo["rainThresh"] = s.rain.thresholdMm;
        zo["rainHours"]  = s.rain.forecastHours;
        zo["forceToday"] = s.forceToday;
        zo["lastReason"] = _schedule->getLastReason(z);

        // Slots par jour
        JsonArray days = zo["daySlots"].to<JsonArray>();
        for (uint8_t d = 0; d < NB_DAYS; d++) {
            JsonArray daySlots = days.add<JsonArray>();
            for (uint8_t sl = 0; sl < MAX_SLOTS; sl++) {
                TimeSlot& ts = s.daySlots[d].slots[sl];
                JsonObject slotObj = daySlots.add<JsonObject>();
                slotObj["hour"]     = ts.hour;
                slotObj["minute"]   = ts.minute;
                slotObj["duration"] = ts.duration;
                slotObj["enabled"]  = ts.enabled;
            }
        }

        // Slots intervalle
        JsonArray iSlots = zo["intervalSlots"].to<JsonArray>();
        for (uint8_t sl = 0; sl < MAX_SLOTS; sl++) {
            TimeSlot& ts = s.intervalSlots.slots[sl];
            JsonObject slotObj = iSlots.add<JsonObject>();
            slotObj["hour"]     = ts.hour;
            slotObj["minute"]   = ts.minute;
            slotObj["duration"] = ts.duration;
            slotObj["enabled"]  = ts.enabled;
        }
    }

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void WebManager::handleSetMode(AsyncWebServerRequest* request, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    if (zone >= NB_ZONES) {
        request->send(400, "application/json", "{\"error\":\"zone invalide\"}");
        return;
    }
    _schedule->setMode(zone, doc["mode"] | 0);
    request->send(200, "application/json", "{\"ok\":true}");
}

void WebManager::handleSetInterval(AsyncWebServerRequest* request, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    if (zone >= NB_ZONES) {
        request->send(400, "application/json", "{\"error\":\"zone invalide\"}");
        return;
    }
    _schedule->setIntervalDays(zone, doc["interval"] | 2);
    request->send(200, "application/json", "{\"ok\":true}");
}

void WebManager::handleSetDaySlot(AsyncWebServerRequest* request, JsonDocument& doc) {
    uint8_t zone    = doc["zone"]    | 255;
    uint8_t day     = doc["day"]     | 255;
    uint8_t slotIdx = doc["slotIdx"] | 255;
    if (zone >= NB_ZONES || day >= NB_DAYS || slotIdx >= MAX_SLOTS) {
        request->send(400, "application/json", "{\"error\":\"parametres invalides\"}");
        return;
    }
    _schedule->setDaySlot(zone, day, slotIdx,
        doc["hour"]     | 6,
        doc["minute"]   | 0,
        doc["duration"] | 5,
        doc["enabled"]  | false);
    request->send(200, "application/json", "{\"ok\":true}");
}

void WebManager::handleSetIntervalSlot(AsyncWebServerRequest* request, JsonDocument& doc) {
    uint8_t zone    = doc["zone"]    | 255;
    uint8_t slotIdx = doc["slotIdx"] | 255;
    if (zone >= NB_ZONES || slotIdx >= MAX_SLOTS) {
        request->send(400, "application/json", "{\"error\":\"parametres invalides\"}");
        return;
    }
    _schedule->setIntervalSlot(zone, slotIdx,
        doc["hour"]     | 6,
        doc["minute"]   | 0,
        doc["duration"] | 5,
        doc["enabled"]  | false);
    request->send(200, "application/json", "{\"ok\":true}");
}

void WebManager::handleSetRain(AsyncWebServerRequest* request, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    if (zone >= NB_ZONES) {
        request->send(400, "application/json", "{\"error\":\"zone invalide\"}");
        return;
    }
    _schedule->setRainConfig(zone,
        doc["threshold"] | DEFAULT_RAIN_THRESHOLD,
        doc["hours"]     | DEFAULT_FORECAST_HOURS);
    request->send(200, "application/json", "{\"ok\":true}");
}

void WebManager::handleForce(AsyncWebServerRequest* request, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    if (zone >= NB_ZONES) {
        request->send(400, "application/json", "{\"error\":\"zone invalide\"}");
        return;
    }
    _schedule->setForceToday(zone, doc["force"] | false);
    request->send(200, "application/json", "{\"ok\":true}");
}

void WebManager::handleManual(AsyncWebServerRequest* request, JsonDocument& doc) {
    uint8_t zone  = doc["zone"]  | 255;
    bool    state = doc["state"] | false;
    if (zone >= NB_ZONES) {
        request->send(400, "application/json", "{\"error\":\"zone invalide\"}");
        return;
    }
    _relais->setRelay(zone, state);
    request->send(200, "application/json", "{\"ok\":true}");
}