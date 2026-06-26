#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "EventBus.h"
#include "WiFiManager.h"
#include "NTPManager.h"
#include "WeatherManager.h"
#include "RelaisManager.h"
#include "ScheduleManager.h"
#include "WebManager.h"
#include "DisplayManager.h"
#include "ConfigManager.h"

// ── Instances ─────────────────────────────────
WiFiManager     wifiMgr;
NTPManager      ntpMgr;
WeatherManager  weatherMgr;
RelaisManager   relaisMgr;
ScheduleManager scheduleMgr;
WebManager      webMgr;
DisplayManager  displayMgr;
ConfigManager   configMgr;

// ── Callback relais (ScheduleManager → RelaisManager) ──
// Invariant I6 : câblage dans main.cpp uniquement
static void onRelayRequest(uint8_t zone, bool state) {
    relaisMgr.setRelay(zone, state);
    EventBus::displayDirty = true;  // forcer redraw immédiat côté LCD
}

// ── Helper splash ──────────────────────────────
// Affiche une étape de boot sur le splash screen
static uint8_t _splashStep = 0;
static void splashStep(const char* label) {
    displayMgr.showSplash(_splashStep, label);
    _splashStep++;
}

// ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== AquaLook v2.0 ===");

    Wire.begin(SDA_PIN, SCL_PIN);

    // Scan I2C temporaire — à retirer après diagnostic
Serial.println("[I2C] Scan...");
for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0)
        Serial.printf("[I2C] Device trouve @ 0x%02X\n", addr);
}
Serial.println("[I2C] Scan termine");

    // ── Boot instrumenté ──────────────────────
    // Invariant I1 : ConfigManager est l'unique propriétaire du montage LittleFS.
    // Le splash est affiché juste après, lorsque le système de fichiers est prêt.
    configMgr.begin();

    // ── Splash screen ─────────────────────────
    displayMgr.initTft();
    displayMgr.showSplash(0, "Initialisation...");
    
    Serial.printf("[Debug] SSID='%s' PWD len=%d\n",
    configMgr.wifi().ssid,
    strlen(configMgr.wifi().password));

    splashStep("Configuration");

    relaisMgr.begin(&configMgr);
    splashStep("Relais");

    scheduleMgr.begin();
    scheduleMgr.setRelayCallback(onRelayRequest);
    configMgr.applyToSchedule(scheduleMgr);
    splashStep("Planning");

    // Invariant I9 : credentials depuis flash
    wifiMgr.begin(configMgr.wifi().ssid, configMgr.wifi().password);
    splashStep("WiFi");

    ntpMgr.begin(&configMgr);
    splashStep("NTP");

    webMgr.begin(&ntpMgr, &weatherMgr, &relaisMgr, &scheduleMgr, &configMgr, &wifiMgr);
    splashStep("Serveur web");

    weatherMgr.begin(&configMgr);
    splashStep("Meteo");

    // Pause courte pour que l'utilisateur voie "100%" avant le passage à HOME
    delay(800);

    // ── DisplayManager complet (touch, sprites, écran HOME) ──
    displayMgr.begin(&ntpMgr, &weatherMgr, &relaisMgr, &scheduleMgr, &configMgr);

    Serial.println("[Main] Setup terminé — boucle démarrée");
    Serial.printf("[HW] PSRAM : %u octets\n", ESP.getPsramSize());
}

// ─────────────────────────────────────────────
void loop() {
    // ── Réseau ────────────────────────────────
    wifiMgr.update();
    const bool connected = wifiMgr.isConnected();
    if (connected) {
        ntpMgr.update();
        weatherMgr.update(true);
    }

    // ── Planificateur — nécessite NTP synchronisé ──
    if (ntpMgr.isSynced()) {
        scheduleMgr.update(
            ntpMgr.getHour(),
            ntpMgr.getMinute(),
            ntpMgr.getWeekday(),
            ntpMgr.getEpochDay(),
            weatherMgr.getRainMm()
        );
    }

    // ── Sécurité durée max relais (invariant I7) ──
    relaisMgr.update();

    // ── Web — event-driven ────────────────────
    webMgr.update();

    // ── Affichage + touch ─────────────────────
    displayMgr.update();

    yield();
}
