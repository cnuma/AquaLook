#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"
#include "WiFiManager.h"
#include "NTPManager.h"
#include "WeatherManager.h"
#include "RelaisManager.h"
#include "ScheduleManager.h"
#include "WebManager.h"
#include "DisplayManager.h"
#include "DisplayPlanningDecor.h"
#include "ConfigManager.h"
#include "StorageManager.h"
#include "SystemDiagnostics.h"

WiFiManager wifiMgr;
NTPManager ntpMgr;
WeatherManager weatherMgr;
RelaisManager relaisMgr;
ScheduleManager scheduleMgr;
WebManager webMgr;
DisplayManager displayMgr;
ConfigManager configMgr;
StorageManager storageMgr;

static void onRelayRequest(uint8_t zone, bool state) {
    relaisMgr.setRelay(zone, state);
    EventBus::displayDirty = true;
}

static uint8_t _splashStep = 0;

static void splashStep(const char* label) {
    displayMgr.showSplash(_splashStep, label);
    _splashStep++;
}

void setup() {
    Serial.begin(115200);
    delay(300);

    FaultManager::begin();
    EventLog::log(LOG_INFO, "AquaLook v2.0 demarrage");
    SystemDiagnostics::begin();

    Wire.begin(SDA_PIN, SCL_PIN);

    EventLog::log(LOG_INFO, "I2C: scan demarre");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            EventLog::log(
                LOG_INFO,
                "I2C: peripherique trouve a 0x%02X",
                addr
            );
            found++;
        }
    }
    EventLog::log(
        found > 0 ? LOG_INFO : LOG_WARN,
        "I2C: scan termine, %u peripherique(s)",
        found
    );

    configMgr.begin();

    displayMgr.initTft();
    displayMgr.showSplash(0, "Initialisation...");

    storageMgr.begin();
    splashStep(storageMgr.isSdAvailable() ? "Carte SD" : "SD indisponible");

    EventLog::log(
        LOG_INFO,
        "Config: SSID='%s', mot de passe present=%s",
        configMgr.wifi().ssid,
        strlen(configMgr.wifi().password) > 0 ? "oui" : "non"
    );

    splashStep("Configuration");

    relaisMgr.begin(&configMgr);
    splashStep("Relais");

    scheduleMgr.begin();
    scheduleMgr.setRelayCallback(onRelayRequest);
    configMgr.applyToSchedule(scheduleMgr);
    splashStep("Planning");

    wifiMgr.begin(
        configMgr.wifi().ssid,
        configMgr.wifi().password
    );
    splashStep("WiFi");

    ntpMgr.begin(&configMgr);
    splashStep("NTP");

    webMgr.registerFaultRoutes();
    webMgr.begin(
        &ntpMgr,
        &weatherMgr,
        &relaisMgr,
        &scheduleMgr,
        &configMgr,
        &wifiMgr
    );
    splashStep("Serveur web");

    weatherMgr.begin(&configMgr);
    splashStep("Meteo");

    delay(800);

    displayMgr.begin(
        &ntpMgr,
        &weatherMgr,
        &relaisMgr,
        &scheduleMgr,
        &configMgr
    );

    EventLog::log("Main: setup termine, boucle demarree");
    EventLog::log(
        LOG_INFO,
        "HW: PSRAM %u octets",
        ESP.getPsramSize()
    );
}

void loop() {
    SystemDiagnostics::loopEnter();

    FaultManager::update();

    wifiMgr.update();
    const bool connected = wifiMgr.isConnected();

    if (connected) {
        ntpMgr.update();
        weatherMgr.update(true);
    }

    if (ntpMgr.isSynced()) {
        scheduleMgr.update(
            ntpMgr.getHour(),
            ntpMgr.getMinute(),
            ntpMgr.getWeekday(),
            ntpMgr.getEpochDay(),
            weatherMgr.getRainMm()
        );
    }

    relaisMgr.update();
    webMgr.update();
    displayMgr.update();
    displayPlanningDecorDraw(displayMgr);

    FaultManager::update();
    yield();
    SystemDiagnostics::loopExit();
}
