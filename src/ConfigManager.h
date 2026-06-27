#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ScheduleManager.h"   // ZoneSchedule, TimeSlot

// ── Chemins fichiers ───────────────────────────────────────────
#define CFG_PATH      "/config.json"   // ancien format LittleFS, lecture migration uniquement
#define CFG_VERSION   2          // version JSON historique

#define CFG_NVS_NAMESPACE "aqualook"
#define CFG_NVS_KEY       "config"
#define CFG_NVS_SCHEMA    1

// ═══════════════════════════════════════════════════════════════
//  Structs de configuration persistée
//
//  Règle : JAMAIS de #define de config.h dans les default member
//  initializers — utiliser des valeurs littérales commentées
//  (invariant I11/I12 : constructeurs explicites).
// ═══════════════════════════════════════════════════════════════

// ── WiFi ──────────────────────────────────────────────────────
struct CfgWifi {
    char ssid[64]     = "";
    char password[64] = "";
    CfgWifi() { ssid[0] = '\0'; password[0] = '\0'; }
};

// ── Touch ─────────────────────────────────────────────────────
struct CfgTouch {
    int16_t xMin = 300;   // TOUCH_X_MIN
    int16_t xMax = 3758;  // TOUCH_X_MAX
    int16_t yMin = 324;   // TOUCH_Y_MIN
    int16_t yMax = 3790;  // TOUCH_Y_MAX
    CfgTouch() : xMin(300), xMax(3758), yMin(324), yMax(3790) {}
    CfgTouch(int16_t x0, int16_t x1, int16_t y0, int16_t y1)
        : xMin(x0), xMax(x1), yMin(y0), yMax(y1) {}
};

// ── Arrosage manuel ───────────────────────────────────────────
struct CfgManual {
    uint16_t durationMin = 10;  // MANUAL_WATERING_DURATION_MIN
    CfgManual() : durationMin(10) {}
    explicit CfgManual(uint16_t d) : durationMin(d) {}
};

// ── NTP (nouveau v2) ──────────────────────────────────────────
struct CfgNtp {
    char    server[64] = "pool.ntp.org";
    int32_t gmtOffset  = 3600;   // Europe Paris hiver (secondes)
    int32_t dstOffset  = 3600;   // heure d'été (secondes)
    CfgNtp() : gmtOffset(3600), dstOffset(3600) {
        strlcpy(server, "pool.ntp.org", sizeof(server));
    }
};

// ── OpenWeatherMap (nouveau v2) ───────────────────────────────
struct CfgOwm {
    char  apiKey[48]  = "";       // clé OWM — vide = météo désactivée
    float lat         = 0.0f;
    float lon         = 0.0f;
    char  units[8]    = "metric"; // "metric" | "imperial"
    char  city[32]    = "";       // ville (alternative aux coordonnées GPS)
    char  country[4]  = "FR";     // code pays ISO 2 lettres
    CfgOwm() : lat(0.0f), lon(0.0f) {
        apiKey[0] = '\0';
        city[0]   = '\0';
        strlcpy(units,   "metric", sizeof(units));
        strlcpy(country, "FR",     sizeof(country));
    }
};

// Limite fonctionnelle actuelle : affichages et sorties exposés de 1 à 8 zones.
// MAX_ZONES reste à 16 en interne pour préserver la structure des données.
static constexpr uint8_t MAX_ACTIVE_ZONES = 8;

// Contrôleur matériel des sorties relais
static constexpr uint8_t RELAY_CONTROLLER_XL9535   = 0;
static constexpr uint8_t RELAY_CONTROLLER_MCP23017 = 1;

// ── Système (nouveau v2) ──────────────────────────────────────
struct CfgSystem {
    uint16_t maxWateringMin    = 60;   // durée max arrosage (min)
    uint8_t  screenTimeoutMin  = 5;    // veille écran (0=jamais)
    uint8_t  ledMode           = 1;    // 0=off..4=arc-en-ciel
    uint8_t  nbZones           = 2;    // zones actives (1..MAX_ACTIVE_ZONES)
    uint8_t  nbRelaisPhysical  = 2;    // relais câblés (<=nbZones)
    uint8_t  relayLogic        = 1;    // 0=inverse (bit=0→ON), 1=direct (bit=1→ON)
    uint8_t  relayController   = RELAY_CONTROLLER_XL9535; // 0=XL9535, 1=MCP23017
    CfgSystem() : maxWateringMin(60), screenTimeoutMin(5),
                  ledMode(1), nbZones(NB_ZONES),
                  nbRelaisPhysical(NB_ZONES), relayLogic(1),
                  relayController(RELAY_CONTROLLER_XL9535) {}
};

// ── Planning zone ─────────────────────────────────────────────
struct CfgRain {
    float   thresholdMm   = 2.0f;  // DEFAULT_RAIN_THRESHOLD
    uint8_t forecastHours = 24;    // DEFAULT_FORECAST_HOURS
    CfgRain() : thresholdMm(2.0f), forecastHours(24) {}
    CfgRain(float t, uint8_t h) : thresholdMm(t), forecastHours(h) {}
};

struct CfgSlot {
    uint8_t  hour     = 6;
    uint8_t  minute   = 0;
    uint16_t duration = 5;
    bool     enabled  = false;
    CfgSlot() : hour(6), minute(0), duration(5), enabled(false) {}
    CfgSlot(uint8_t h, uint8_t m, uint16_t d, bool e)
        : hour(h), minute(m), duration(d), enabled(e) {}
};

struct CfgDaySchedule {
    CfgSlot slots[MAX_SLOTS];
    CfgDaySchedule() {}
};

struct CfgZone {
    char            name[24]     = "";  // "Zone 1" / "Zone 2" — nouveau v2
    uint8_t         mode         = 0;   // SCHEDULE_MODE_DAYS
    uint8_t         intervalDays = 2;
    CfgRain         rain;
    CfgDaySchedule  daySlots[NB_DAYS];
    CfgDaySchedule  intervalSlots;
    CfgZone() : mode(0), intervalDays(2) { name[0] = '\0'; }
};

// ── Affichage LCD — tokens de design ──────────────────────────
//
//  Couleurs stockées en #rrggbb (7 chars + null) — converties en
//  RGB565 à l'utilisation via hexToRgb565() dans DisplayManager.
//
//  Hot-reload : toutes les valeurs prennent effet au prochain cycle
//  DisplayManager::update() sans reboot (EventBus::displayDirty).
struct CfgDisplay {
    // Couleurs de fond / surfaces
    char cBg[8]        = "#101818";  // Theme::BG
    char cSurface[8]   = "#182420";  // Theme::SURFACE
    char cSurface2[8]  = "#283028";  // Theme::SURFACE2
    char cBorder[8]    = "#384c40";  // Theme::BORDER
    // Couleurs de texte
    char cText[8]      = "#f8fcf8";  // Theme::TEXT
    char cText2[8]     = "#c0d0c8";  // Theme::TEXT2
    char cMuted[8]     = "#789c80";  // Theme::MUTED
    // Etat actif
    char cActiveBg[8]  = "#382020";  // Theme::ACTIVE_BG
    // Accents de zone (identite couleur par zone)
    char cZone0[8]     = "#00fc00";  // vert  (Zone 1)
    char cZone1[8]     = "#0090f8";  // bleu  (Zone 2)
    char cZone2[8]     = "#f8a400";  // amber (Zone 3)
    char cZone3[8]     = "#780078";  // violet (Zone 4)
    // Formes
    uint8_t rSm        = 4;
    uint8_t rMd        = 6;
    uint8_t rLg        = 10;
    uint8_t accentBarW = 3;
    // Timing refresh LCD (ms) — invariant I21
    uint16_t refreshNomMs = 5000;
    uint16_t refreshActMs = 1000;
    // Layout independant (hot-reload, touch resync auto dans update())
    uint8_t planGap    = 6;
    uint8_t g2Gpad     = 1;
    uint8_t g4Gpad     = 1;
    // Options météo LCD
    bool showWeatherIcon = true;   // afficher icône pluie/soleil dans le planning
    bool showWeatherTemp = false;  // afficher température max du jour

    // Info-bulles météo de la page Web — valeurs par défaut recommandées
    bool weatherTipCondition = true;
    bool weatherTipTemp      = true;
    bool weatherTipRain      = true;
    bool weatherTipPop       = true;
    bool weatherTipHumidity  = true;
    bool weatherTipWind      = true;
    bool weatherTipGust      = true;
    bool weatherTipClouds    = false;
    bool weatherTipPressure  = false;

    CfgDisplay() {
        strlcpy(cBg,       "#101818", 8); strlcpy(cSurface,  "#182420", 8);
        strlcpy(cSurface2, "#283028", 8); strlcpy(cBorder,   "#384c40", 8);
        strlcpy(cText,     "#f8fcf8", 8); strlcpy(cText2,    "#c0d0c8", 8);
        strlcpy(cMuted,    "#789c80", 8); strlcpy(cActiveBg, "#382020", 8);
        strlcpy(cZone0,    "#00fc00", 8); strlcpy(cZone1,    "#0090f8", 8);
        strlcpy(cZone2,    "#f8a400", 8); strlcpy(cZone3,    "#780078", 8);
    }
};

// ═══════════════════════════════════════════════════════════════
//  ConfigManager
//
//  Invariant I1  : LittleFS.begin() ici uniquement.
//  Invariant I2  : LittleFS est en lecture seule pour les ressources Web.
//  Invariant I20 : NTP/OWM relus par les managers après configDirty.
// ═══════════════════════════════════════════════════════════════
class ConfigManager {
public:
    // ── Cycle de vie ──────────────────────────────────────────
    void begin();   // mount LittleFS + charge NVS (migration JSON si nécessaire)
    void save();    // écriture binaire versionnée dans NVS
    void resetPersistent(); // efface uniquement la configuration NVS

    // ── Application vers les managers ─────────────────────────
    void applyToSchedule(ScheduleManager& sched) const;

    // ── Getters ───────────────────────────────────────────────
    const CfgWifi&   wifi()   const { return _wifi;   }
    const CfgTouch&  touch()  const { return _touch;  }
    const CfgManual& manual() const { return _manual; }
    const CfgNtp&    ntp()    const { return _ntp;    }
    const CfgOwm&    owm()    const { return _owm;    }
    const CfgSystem& system() const { return _system; }
    const CfgZone&   zone(uint8_t z) const;
    const CfgDisplay& display() const { return _display; }
    bool weatherVisualsEnabled() const { return _weatherVisualsEnabled; }
    uint8_t          nbZones()        const { return _system.nbZones; }
    uint8_t          nbRelais()       const { return _system.nbRelaisPhysical; }
    uint8_t          relayLogic()     const { return _system.relayLogic; }
    uint8_t          relayController() const { return _system.relayController; }

    bool isLoaded() const { return _loaded; }

    // ── Setters — écrivent en flash immédiatement ──────────────

    // WiFi — save() + reboot (invariant I10)
    void setWifi(const char* ssid, const char* pwd);

    // Touch
    void setTouchCalib(int16_t xMin, int16_t xMax,
                       int16_t yMin, int16_t yMax);

    // Manuel
    void setManualDuration(uint16_t minutes);
    void setSystemAndManualDuration(const CfgSystem& cfg, uint16_t minutes);

    // NTP (nouveau v2)
    void setNtp(const char* server, int32_t gmtOffset, int32_t dstOffset);

    // OWM (nouveau v2)
    void setOwm(const char* apiKey, float lat, float lon,
               const char* units,
               const char* city = "", const char* country = "FR");

    // Système (nouveau v2)
    void setSystemMaxWatering(uint16_t minutes);
    void setSystemScreenTimeout(uint8_t minutes);
    void setSystemLedMode(uint8_t mode);
    void setSystemNbZones(uint8_t nb);           // 1..MAX_ACTIVE_ZONES, reboot requis
    void setSystemNbRelais(uint8_t nb);          // 1..nb_zones
    void setSystemRelayLogic(uint8_t logic);     // 0=inverse, 1=direct — SENSIBLE
    void setSystem(const CfgSystem& cfg);          // mise à jour groupée, un seul save()

    // Nom de zone (nouveau v2)
    void setZoneName(uint8_t zone, const char* name);

    // Affichage LCD (display tokens)
    void setDisplay(const CfgDisplay& d);  // hot-reload, pas de reboot
    void setWeatherVisualsEnabled(bool enabled);

    // Planning
    void setZoneMode(uint8_t zone, uint8_t mode);
    void setZoneIntervalDays(uint8_t zone, uint8_t days);
    void setZoneRain(uint8_t zone, float threshMm, uint8_t hours);
    void setZoneDaySlot(uint8_t zone, uint8_t day, uint8_t slotIdx,
                        uint8_t h, uint8_t m, uint16_t dur, bool enabled);
    void setZoneIntervalSlot(uint8_t zone, uint8_t slotIdx,
                             uint8_t h, uint8_t m, uint16_t dur, bool enabled);

    // Sync planning complet depuis ScheduleManager
    void syncZoneFromSchedule(uint8_t zone, const ZoneSchedule& zs);

private:
    CfgWifi   _wifi;
    CfgTouch  _touch;
    CfgManual _manual;
    CfgNtp    _ntp;
    CfgOwm    _owm;
    CfgSystem _system;
    CfgDisplay _display;
    CfgZone   _zones[MAX_ZONES];  // capacité max — actif = system().nbZones
    bool      _loaded = false;
    bool      _weatherVisualsEnabled = false;

    bool loadNvs();
    bool loadLegacyJson();
    void defaults();

    // Helpers JSON ↔ structs
    void zoneToJson(uint8_t z, JsonObject& obj) const;
    bool zoneFromJson(uint8_t z, JsonObjectConst obj);
};
