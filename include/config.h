#pragma once

// ═══════════════════════════════════════════════════════════════
//  config.h — constantes hardware et compile-time UNIQUEMENT
//
//  RÈGLE : ce fichier ne contient QUE des valeurs qui ne peuvent
//  pas changer sans recompiler (pins, bus, protocoles).
//  Tout paramètre modifiable en production → ConfigManager.
//
//  Invariant I12 : ce fichier est dupliqué dans src/ ET include/
//  pour la résolution des includes PlatformIO.
// ═══════════════════════════════════════════════════════════════

// ── WiFi — fallback compile-time ──────────────
// Utilisé si config.json absent ou SSID vide
// En production : credentials dans config.json via ConfigManager
#define WIFI_SSID               ""
#define WIFI_PASSWORD           ""
#define WIFI_RETRY_INTERVAL     30000UL   // ms entre tentatives

// ── NTP — fallback compile-time ───────────────
// Surchargé par CfgNtp depuis ConfigManager
#define NTP_SERVER1             "pool.ntp.org"
#define NTP_SERVER2             "time.nist.gov"
#define GMT_OFFSET              3600L     // Europe Paris hiver
#define DST_OFFSET              3600L     // heure d'été
#define NTP_SYNC_INTERVAL       3600000UL // resync toutes les 1h

// ── Météo OWM — fallback compile-time ────────
// Surchargé par CfgOwm depuis ConfigManager
#define OWM_API_KEY             ""
#define OWM_CITY                ""
#define OWM_COUNTRY             "FR"
#define OWM_CHECK_INTERVAL_MS   7200000UL // fetch toutes les 2h

// ── Hardware I2C ──────────────────────────────
#define SDA_PIN                 27
#define SCL_PIN                 22
#define XL9535_ADDR             0x20      // adresse I2C expandeur relais

// ── Hardware Touch XPT2046 (bus VSPI séparé) ──
#define TOUCH_IRQ               36
#define TOUCH_MOSI              32
#define TOUCH_MISO              39
#define TOUCH_CLK               25
#define TOUCH_CS                33
// Calibration par défaut — surchargée par CfgTouch depuis ConfigManager
#define TOUCH_X_MIN             300
#define TOUCH_X_MAX             3758
#define TOUCH_Y_MIN             324
#define TOUCH_Y_MAX             3790

// ── Zones — capacité maximale compile-time ────
// MAX_ZONES  : taille des tableaux en RAM — ne pas dépasser
// MAX_RELAIS : relais physiques max supportés par le hardware
// NB_ZONES   : valeur par défaut si config.json absent
//              Le nombre actif est ConfigManager::system().nbZones
// IMPORTANT  : NB_ZONES <= MAX_ZONES obligatoire
#define MAX_ZONES               16        // capacité tableaux RAM
#define MAX_RELAIS              16        // relais physiques max
#define NB_ZONES                2         // défaut si pas de config

// ── Planning — taille des tableaux ───────────
#define NB_DAYS                 7         // jours semaine (lun→dim)
#define MAX_SLOTS               5         // slots max par jour/zone

// ── Seuils météo — fallback compile-time ─────
#define DEFAULT_RAIN_THRESHOLD  2.0f      // mm → blocage arrosage
#define DEFAULT_FORECAST_HOURS  24        // fenêtre prévision (h)
#define MAX_FORECAST_HOURS      48

// ── Sécurité arrosage — fallback compile-time ─
// Surchargé par CfgSystem::maxWateringMin depuis ConfigManager
#define MAX_WATERING_DURATION_MS (3600000UL) // 1h en ms

// ── Modes de planification ────────────────────
#define SCHEDULE_MODE_DAYS      0         // jours fixes (lun, mer...)
#define SCHEDULE_MODE_INTERVAL  1         // toutes les N jours

// ── Manuel arrosage — fallback ────────────────
#define MANUAL_WATERING_DURATION_MIN 10   // durée défaut (min)
