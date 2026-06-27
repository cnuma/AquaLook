#include "ConfigManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include <Preferences.h>
#include <cstring>
#include <cstddef>

namespace {
constexpr uint32_t NVS_MAGIC = 0x414C4F4BUL; // "ALOK"

struct PersistedConfig {
    uint32_t magic;
    uint16_t schema;
    uint16_t payloadSize;
    CfgWifi wifi;
    CfgTouch touch;
    CfgManual manual;
    CfgNtp ntp;
    CfgOwm owm;
    CfgSystem system;
    CfgDisplay display;
    CfgZone zones[MAX_ZONES];
    uint32_t crc32;
};

uint8_t normalizeActiveZones(uint8_t zones, uint8_t controller) {
    zones = constrain(zones, (uint8_t)1, (uint8_t)MAX_ACTIVE_ZONES);
    if (controller == RELAY_CONTROLLER_XL9535) {
        zones = constrain((uint8_t)((zones + 1U) & 0xFEU), (uint8_t)2, (uint8_t)8);
    }
    return zones;
}

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
    return ~crc;
}
}

// ═══════════════════════════════════════════════════════════════
//  Cycle de vie
// ═══════════════════════════════════════════════════════════════

void ConfigManager::begin() {
    // LittleFS ne contient plus que les ressources Web et le splash.
    // La configuration persistante est stockée séparément dans NVS.
    if (!LittleFS.begin(true)) {
        EventLog::log(LOG_ERROR, "Config: mount LittleFS ECHEC — ressources Web indisponibles");
    } else {
        EventLog::log(LOG_INFO, "Config: LittleFS monte (lecture ressources)");
    }

    {
        Preferences prefs;
        if (prefs.begin(CFG_NVS_NAMESPACE, true)) {
            _weatherVisualsEnabled = prefs.getBool("wxVisual", false);
            prefs.end();
        }
    }

    if (loadNvs()) return;

    // Migration unique depuis l'ancien /config.json, sans jamais le réécrire.
    if (loadLegacyJson()) {
        EventLog::log(LOG_INFO, "Config: migration LittleFS -> NVS");
        save();

        // Vérifier que le bloc NVS existe avant de retirer l'ancien JSON.
        Preferences check;
        bool migrated = false;
        if (check.begin(CFG_NVS_NAMESPACE, true)) {
            migrated = (check.getBytesLength(CFG_NVS_KEY) == sizeof(PersistedConfig));
            check.end();
        }
        if (migrated && LittleFS.exists(CFG_PATH)) {
            if (LittleFS.remove(CFG_PATH))
                EventLog::log(LOG_INFO, "Config: ancien JSON supprime apres migration");
            else
                EventLog::log(LOG_WARN, "Config: ancien JSON conserve (suppression impossible)");
        }
        return;
    }

    EventLog::log(LOG_WARN, "Config: NVS/JSON absents ou invalides — valeurs par defaut");
    defaults();
    save();
}

bool ConfigManager::loadNvs() {
    Preferences prefs;
    if (!prefs.begin(CFG_NVS_NAMESPACE, true)) {
        EventLog::log(LOG_ERROR, "Config: ouverture NVS lecture impossible");
        return false;
    }

    const size_t len = prefs.getBytesLength(CFG_NVS_KEY);
    if (len != sizeof(PersistedConfig)) {
        prefs.end();
        if (len != 0) EventLog::log(LOG_WARN, "Config: taille NVS invalide (%u/%u)",
                                     (unsigned)len, (unsigned)sizeof(PersistedConfig));
        return false;
    }

    PersistedConfig* blob = static_cast<PersistedConfig*>(malloc(sizeof(PersistedConfig)));
    if (!blob) {
        prefs.end();
        EventLog::log(LOG_ERROR, "Config: allocation lecture NVS impossible");
        return false;
    }

    const size_t read = prefs.getBytes(CFG_NVS_KEY, blob, sizeof(PersistedConfig));
    prefs.end();

    bool valid = (read == sizeof(PersistedConfig)) &&
                 (blob->magic == NVS_MAGIC) &&
                 (blob->schema == CFG_NVS_SCHEMA) &&
                 (blob->payloadSize == sizeof(PersistedConfig));
    if (valid) {
        const uint32_t expected = crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                                             offsetof(PersistedConfig, crc32));
        valid = (expected == blob->crc32);
    }

    if (!valid) {
        EventLog::log(LOG_ERROR, "Config: bloc NVS invalide (entete/CRC)");
        free(blob);
        return false;
    }

    _wifi = blob->wifi;
    _touch = blob->touch;
    _manual = blob->manual;
    _ntp = blob->ntp;
    _owm = blob->owm;
    _system = blob->system;
    _display = blob->display;
    memcpy(_zones, blob->zones, sizeof(_zones));
    free(blob);

    _system.relayController = (_system.relayController <= RELAY_CONTROLLER_MCP23017)
                              ? _system.relayController : RELAY_CONTROLLER_XL9535;
    _system.nbZones = normalizeActiveZones(_system.nbZones, _system.relayController);
    _system.nbRelaisPhysical = _system.nbZones;
    _system.relayLogic = (_system.relayLogic <= 1) ? _system.relayLogic : 1;
    _loaded = true;
    EventLog::log(LOG_INFO, "Config: charge depuis NVS (schema %u)", CFG_NVS_SCHEMA);
    return true;
}

// ─────────────────────────────────────────────────────────────
//  Chargement depuis flash
//  Migration v1 → v2 : sections ntp/owm/system absentes → defaults
// ─────────────────────────────────────────────────────────────
bool ConfigManager::loadLegacyJson() {
    if (!LittleFS.exists(CFG_PATH)) return false;

    File f = LittleFS.open(CFG_PATH, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        // Tout échec de parsing = config inutilisable — reset complet.
        // Un chargement partiel (IncompleteInput) laisse un état hybride
        // imprévisible (ex. nbZones=8 sans le reste de la config) — inacceptable.
        EventLog::log(LOG_ERROR, "Config: JSON invalide (%s) — reset defauts", err.c_str());
        return false;
    }

    int version = doc["version"] | 0;
    // v1 accepté — on migre en chargeant les sections manquantes avec defaults
    // version 0 = JSON minimal du portail captif (wifi seulement) — accepté
    if (version < 0) {
        EventLog::log(LOG_WARN, "Config: version inconnue (%d) — reset", version);
        return false;
    }
    bool needMigration = (version < CFG_VERSION);

    // ── WiFi ──────────────────────────────────────────────────
    JsonObjectConst w = doc["wifi"];
    if (w) {
        strlcpy(_wifi.ssid,     w["ssid"]     | "", sizeof(_wifi.ssid));
        strlcpy(_wifi.password, w["password"] | "", sizeof(_wifi.password));
    }

    // ── Touch ──────────────────────────────────────────────────
    JsonObjectConst t = doc["touch"];
    if (t) {
        _touch.xMin = t["xMin"] | 300;
        _touch.xMax = t["xMax"] | 3758;
        _touch.yMin = t["yMin"] | 324;
        _touch.yMax = t["yMax"] | 3790;
    }

    // ── Manuel ─────────────────────────────────────────────────
    JsonObjectConst man = doc["manual"];
    if (man) {
        _manual.durationMin = man["durationMin"] | (uint16_t)10;
    }

    // ── NTP (v2) ───────────────────────────────────────────────
    JsonObjectConst ntp = doc["ntp"];
    if (ntp) {
        strlcpy(_ntp.server, ntp["server"] | "pool.ntp.org", sizeof(_ntp.server));
        _ntp.gmtOffset = ntp["gmtOffset"] | (int32_t)3600;
        _ntp.dstOffset = ntp["dstOffset"] | (int32_t)3600;
    }
    // si absent (migration v1) : valeurs défaut déjà en place via CfgNtp()

    // ── OWM (v2) ───────────────────────────────────────────────
    JsonObjectConst owm = doc["owm"];
    if (owm) {
        strlcpy(_owm.apiKey,  owm["apiKey"]  | "",       sizeof(_owm.apiKey));
        _owm.lat = owm["lat"] | 0.0f;
        _owm.lon = owm["lon"] | 0.0f;
        strlcpy(_owm.units,   owm["units"]   | "metric", sizeof(_owm.units));
        strlcpy(_owm.city,    owm["city"]    | "",       sizeof(_owm.city));
        strlcpy(_owm.country, owm["country"] | "FR",     sizeof(_owm.country));
    }

    // ── Système (v2) ───────────────────────────────────────────
    JsonObjectConst sys = doc["system"];
    if (sys) {
        _system.maxWateringMin   = sys["maxWateringMin"]  | (uint16_t)60;
        _system.screenTimeoutMin = sys["screenTimeout"]   | (uint8_t)5;
        _system.ledMode          = sys["ledMode"]         | (uint8_t)1;
        // nbZones : clamp entre 1 et MAX_ZONES
        uint8_t nz = sys["nbZones"] | (uint8_t)NB_ZONES;
        _system.nbZones = constrain(nz, 1, MAX_ACTIVE_ZONES);
        _system.nbRelaisPhysical = _system.nbZones;
        // relayLogic : si absent du JSON (config anterieure), defaut=1 (direct)
        // Le champ | 255 distingue "absent" de "present a 0"
        uint8_t rl = sys["relayLogic"] | (uint8_t)255;
        _system.relayLogic = (rl <= 1) ? rl : 1;  // absent -> 1 (direct)
        uint8_t rc = sys["relayController"] | (uint8_t)RELAY_CONTROLLER_XL9535;
        _system.relayController = (rc <= RELAY_CONTROLLER_MCP23017) ? rc : RELAY_CONTROLLER_XL9535;
        _system.nbZones = normalizeActiveZones(_system.nbZones, _system.relayController);
        _system.nbRelaisPhysical = _system.nbZones;
    }

    // ── Display (tokens de design LCD) ────────────────────────
    JsonObjectConst disp = doc["display"];
    if (disp) {
        auto copyColor = [](const char* src, char* dst) {
            if (src && src[0] == '#' && strlen(src) == 7) strlcpy(dst, src, 8);
        };
        copyColor(disp["cBg"]       | "", _display.cBg);
        copyColor(disp["cSurface"]  | "", _display.cSurface);
        copyColor(disp["cSurface2"] | "", _display.cSurface2);
        copyColor(disp["cBorder"]   | "", _display.cBorder);
        copyColor(disp["cText"]     | "", _display.cText);
        copyColor(disp["cText2"]    | "", _display.cText2);
        copyColor(disp["cMuted"]    | "", _display.cMuted);
        copyColor(disp["cActiveBg"] | "", _display.cActiveBg);
        copyColor(disp["cZone0"]    | "", _display.cZone0);
        copyColor(disp["cZone1"]    | "", _display.cZone1);
        copyColor(disp["cZone2"]    | "", _display.cZone2);
        copyColor(disp["cZone3"]    | "", _display.cZone3);
        _display.rSm        = constrain((uint8_t)(disp["rSm"]        | 4),  1, 20);
        _display.rMd        = constrain((uint8_t)(disp["rMd"]        | 6),  1, 20);
        _display.rLg        = constrain((uint8_t)(disp["rLg"]        | 10), 1, 30);
        _display.accentBarW = constrain((uint8_t)(disp["accentBarW"] | 3),  1, 8);
        uint16_t rn = disp["refreshNomMs"] | (uint16_t)5000;
        uint16_t ra = disp["refreshActMs"] | (uint16_t)1000;
        _display.refreshNomMs = constrain(rn, (uint16_t)500,  (uint16_t)30000);
        _display.refreshActMs = constrain(ra, (uint16_t)200,  (uint16_t)5000);
        _display.planGap  = constrain((uint8_t)(disp["planGap"] | 6), (uint8_t)0, (uint8_t)20);
        _display.g2Gpad   = constrain((uint8_t)(disp["g2Gpad"]  | 1), (uint8_t)0, (uint8_t)8);
        _display.g4Gpad   = constrain((uint8_t)(disp["g4Gpad"]  | 1), (uint8_t)0, (uint8_t)8);
        // Options météo — absents en config antérieure → valeurs défaut (true/false)
        if (disp["showWeatherIcon"].is<bool>()) _display.showWeatherIcon = disp["showWeatherIcon"];
        if (disp["showWeatherTemp"].is<bool>()) _display.showWeatherTemp = disp["showWeatherTemp"];
        if (disp["weatherTipCondition"].is<bool>()) _display.weatherTipCondition = disp["weatherTipCondition"];
        if (disp["weatherTipTemp"].is<bool>())      _display.weatherTipTemp      = disp["weatherTipTemp"];
        if (disp["weatherTipRain"].is<bool>())      _display.weatherTipRain      = disp["weatherTipRain"];
        if (disp["weatherTipPop"].is<bool>())       _display.weatherTipPop       = disp["weatherTipPop"];
        if (disp["weatherTipHumidity"].is<bool>())  _display.weatherTipHumidity  = disp["weatherTipHumidity"];
        if (disp["weatherTipWind"].is<bool>())      _display.weatherTipWind      = disp["weatherTipWind"];
        if (disp["weatherTipGust"].is<bool>())      _display.weatherTipGust      = disp["weatherTipGust"];
        if (disp["weatherTipClouds"].is<bool>())    _display.weatherTipClouds    = disp["weatherTipClouds"];
        if (disp["weatherTipPressure"].is<bool>())  _display.weatherTipPressure  = disp["weatherTipPressure"];
    }
    // Si absent (config anterieure) : valeurs defaut CfgDisplay() deja en place

    // ── Zones ──────────────────────────────────────────────────
    JsonArrayConst zones = doc["zones"];
    if (zones) {
        uint8_t z = 0;
        for (JsonObjectConst zo : zones) {
            if (z >= MAX_ZONES) break;
            zoneFromJson(z, zo);
            z++;
        }
    }

    _loaded = true;

    if (needMigration)
        EventLog::log(LOG_INFO, "Config: JSON historique v%d charge pour migration", version);
    else
        EventLog::log(LOG_INFO, "Config: JSON historique v%d charge pour migration", version);

    return true;
}

// ─────────────────────────────────────────────────────────────
//  Valeurs par défaut
// ─────────────────────────────────────────────────────────────
void ConfigManager::defaults() {
    _wifi   = CfgWifi{};
    _touch  = CfgTouch{};
    _manual = CfgManual{};
    _ntp    = CfgNtp{};
    _owm    = CfgOwm{};
    _system = CfgSystem{};
    _display = CfgDisplay{};

    // Initialiser toutes les zones jusqu'à MAX_ZONES avec des defaults vides
    // Les zones actives sont celles < _system.nbZones
    for (uint8_t z = 0; z < MAX_ZONES; z++) {
        _zones[z] = CfgZone{};

        // Nom par défaut
        snprintf(_zones[z].name, sizeof(_zones[z].name), "Zone %u", z + 1);

        // Zone 1 — lun/mer/ven à 06:30, 5 min
        if (z == 0) {
            const uint8_t days[] = {0, 2, 4};
            for (uint8_t d : days) {
                _zones[z].daySlots[d].slots[0] = CfgSlot(6, 30, 5, true);
            }
        }

        // Zone 2 — intervalle 3j à 06:35, 5 min
        if (z == 1) {
            _zones[z].mode                   = 1;  // SCHEDULE_MODE_INTERVAL
            _zones[z].intervalDays           = 3;
            _zones[z].intervalSlots.slots[0] = CfgSlot(6, 35, 5, true);
        }
    }

    _loaded = true;
    EventLog::log(LOG_INFO, "Config: valeurs par defaut appliquees");
}

// ─────────────────────────────────────────────────────────────
//  Sauvegarde NVS binaire versionnée + CRC32
//  LittleFS reste strictement en lecture pour le Web et le splash.
// ─────────────────────────────────────────────────────────────
void ConfigManager::save() {
    PersistedConfig* blob = static_cast<PersistedConfig*>(malloc(sizeof(PersistedConfig)));
    if (!blob) {
        EventLog::log(LOG_ERROR, "Config: allocation sauvegarde NVS impossible");
        return;
    }

    memset(blob, 0, sizeof(PersistedConfig));
    blob->magic = NVS_MAGIC;
    blob->schema = CFG_NVS_SCHEMA;
    blob->payloadSize = sizeof(PersistedConfig);
    blob->wifi = _wifi;
    blob->touch = _touch;
    blob->manual = _manual;
    blob->ntp = _ntp;
    blob->owm = _owm;
    blob->system = _system;
    blob->display = _display;
    memcpy(blob->zones, _zones, sizeof(_zones));
    blob->crc32 = crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                             offsetof(PersistedConfig, crc32));

    Preferences prefs;
    if (!prefs.begin(CFG_NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "Config: ouverture NVS ecriture impossible");
        free(blob);
        return;
    }

    const size_t written = prefs.putBytes(CFG_NVS_KEY, blob, sizeof(PersistedConfig));
    prefs.end();
    free(blob);

    if (written != sizeof(PersistedConfig)) {
        EventLog::log(LOG_ERROR, "Config: ecriture NVS incomplete (%u/%u)",
                      (unsigned)written, (unsigned)sizeof(PersistedConfig));
        return;
    }

    EventLog::log(LOG_INFO, "Config: sauvegarde NVS OK (%u octets, schema %u)",
                  (unsigned)written, CFG_NVS_SCHEMA);
}

void ConfigManager::resetPersistent() {
    Preferences prefs;
    if (!prefs.begin(CFG_NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "Config: ouverture NVS reset impossible");
        return;
    }
    const bool ok = prefs.clear();
    prefs.end();
    EventLog::log(ok ? LOG_INFO : LOG_ERROR,
                  ok ? "Config: NVS efface" : "Config: echec effacement NVS");
}

// ═══════════════════════════════════════════════════════════════
//  Application vers les managers
// ═══════════════════════════════════════════════════════════════

void ConfigManager::applyToSchedule(ScheduleManager& sched) const {
    sched.setManualDuration(_manual.durationMin);
    sched.setNbZones(_system.nbZones);  // propager le nb de zones actives

    for (uint8_t z = 0; z < _system.nbZones; z++) {
        const CfgZone& cz = _zones[z];
        sched.setMode(z, cz.mode);
        sched.setIntervalDays(z, cz.intervalDays);
        sched.setRainConfig(z, cz.rain.thresholdMm, cz.rain.forecastHours);

        for (uint8_t d = 0; d < NB_DAYS; d++) {
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const CfgSlot& sl = cz.daySlots[d].slots[s];
                sched.setDaySlot(z, d, s, sl.hour, sl.minute,
                                 sl.duration, sl.enabled);
            }
        }
        for (uint8_t s = 0; s < MAX_SLOTS; s++) {
            const CfgSlot& sl = cz.intervalSlots.slots[s];
            sched.setIntervalSlot(z, s, sl.hour, sl.minute,
                                  sl.duration, sl.enabled);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  Getters avec vérification de borne
// ═══════════════════════════════════════════════════════════════

const CfgZone& ConfigManager::zone(uint8_t z) const {
    static const CfgZone empty{};
    if (z >= MAX_ZONES) return empty;
    return _zones[z];
}

// ═══════════════════════════════════════════════════════════════
//  Setters
// ═══════════════════════════════════════════════════════════════

void ConfigManager::setWifi(const char* ssid, const char* pwd) {
    strlcpy(_wifi.ssid,     ssid, sizeof(_wifi.ssid));
    strlcpy(_wifi.password, pwd,  sizeof(_wifi.password));
    save();
    // Invariant I10 : l'appelant (WebManager) a déjà envoyé sendOk()
    ESP.restart();
}

void ConfigManager::setTouchCalib(int16_t xMin, int16_t xMax,
                                   int16_t yMin, int16_t yMax) {
    _touch = CfgTouch(xMin, xMax, yMin, yMax);
    save();
    EventBus::displayDirty = true;
}

void ConfigManager::setManualDuration(uint16_t minutes) {
    _manual.durationMin = constrain(minutes, (uint16_t)1, (uint16_t)120);
    save();
}

void ConfigManager::setSystemAndManualDuration(const CfgSystem& cfg,
                                                uint16_t minutes) {
    // Mise à jour groupée : une seule sérialisation LittleFS pour éviter
    // deux écritures successives lors de la validation de la page Zones.
    _manual.durationMin       = constrain(minutes, (uint16_t)1, (uint16_t)120);
    _system.maxWateringMin   = cfg.maxWateringMin;
    _system.screenTimeoutMin = cfg.screenTimeoutMin;
    _system.ledMode          = constrain(cfg.ledMode, (uint8_t)0, (uint8_t)4);
    _system.relayController  = (cfg.relayController <= RELAY_CONTROLLER_MCP23017)
                               ? cfg.relayController : RELAY_CONTROLLER_XL9535;
    _system.nbZones          = normalizeActiveZones(cfg.nbZones, _system.relayController);
    _system.nbRelaisPhysical = _system.nbZones;
    _system.relayLogic       = (cfg.relayLogic <= 1) ? cfg.relayLogic : 1;

    save();
    EventBus::configDirty = true;
}

void ConfigManager::setNtp(const char* server,
                            int32_t gmtOffset, int32_t dstOffset) {
    strlcpy(_ntp.server, server, sizeof(_ntp.server));
    _ntp.gmtOffset = gmtOffset;
    _ntp.dstOffset = dstOffset;
    save();
    EventBus::configDirty = true;   // NTPManager relira au prochain tick
}

void ConfigManager::setOwm(const char* apiKey, float lat, float lon,
                            const char* units,
                            const char* city, const char* country) {
    if (apiKey && apiKey[0]) strlcpy(_owm.apiKey, apiKey, sizeof(_owm.apiKey));
    _owm.lat = lat;
    _owm.lon = lon;
    strlcpy(_owm.units,   units   && units[0]   ? units   : "metric", sizeof(_owm.units));
    strlcpy(_owm.city,    city    ? city    : "",  sizeof(_owm.city));
    strlcpy(_owm.country, country && country[0] ? country : "FR",     sizeof(_owm.country));
    save();
    EventBus::configDirty = true;
}

void ConfigManager::setSystem(const CfgSystem& cfg) {
    _system.maxWateringMin   = cfg.maxWateringMin;
    _system.screenTimeoutMin = cfg.screenTimeoutMin;
    _system.ledMode          = constrain(cfg.ledMode, (uint8_t)0, (uint8_t)4);
    _system.relayController  = (cfg.relayController <= RELAY_CONTROLLER_MCP23017)
                               ? cfg.relayController : RELAY_CONTROLLER_XL9535;
    _system.nbZones          = normalizeActiveZones(cfg.nbZones, _system.relayController);
    _system.nbRelaisPhysical = _system.nbZones;
    _system.relayLogic       = (cfg.relayLogic <= 1) ? cfg.relayLogic : 1;

    save();
    EventBus::configDirty = true;
}

void ConfigManager::setSystemMaxWatering(uint16_t minutes) {
    _system.maxWateringMin = minutes;
    save();
    EventBus::configDirty = true;
}

void ConfigManager::setSystemScreenTimeout(uint8_t minutes) {
    _system.screenTimeoutMin = minutes;
    save();
    EventBus::configDirty = true;
}

void ConfigManager::setSystemLedMode(uint8_t mode) {
    _system.ledMode = mode;
    save();
    EventBus::configDirty = true;
}

void ConfigManager::setSystemNbZones(uint8_t nb) {
    _system.nbZones = constrain(nb, 1, MAX_ACTIVE_ZONES);

    // AquaLook utilise actuellement une relation 1 zone = 1 relais.
    // La valeur doit être mise à jour dans la même sauvegarde que nbZones,
    // avant le reboot. Lors d'une augmentation (2 -> 4/8/16), l'ancien code
    // conservait nbRelaisPhysical à 2 car il ne corrigeait que les dépassements.
    _system.nbRelaisPhysical = _system.nbZones;

    save();
    // Reboot requis — les tableaux RAM sont redimensionnés au boot
    // L'appelant (WebManager) envoie sendOk() AVANT d'appeler ce setter
    ESP.restart();
}

void ConfigManager::setSystemNbRelais(uint8_t nb) {
    _system.nbRelaisPhysical = constrain(nb, 1, _system.nbZones);
    save();
    EventBus::configDirty = true;
}

void ConfigManager::setSystemRelayLogic(uint8_t logic) {
    _system.relayLogic = (logic <= 1) ? logic : 0;
    save();
    EventBus::configDirty = true;
}

void ConfigManager::setZoneName(uint8_t z, const char* name) {
    if (z >= MAX_ZONES) return;
    strlcpy(_zones[z].name, name, sizeof(_zones[z].name));
    save();
    EventBus::displayDirty = true;
}

// ─────────────────────────────────────────────────────────────
//  Affichage LCD — hot-reload, pas de reboot
// ─────────────────────────────────────────────────────────────

void ConfigManager::setWeatherVisualsEnabled(bool enabled) {
    if (_weatherVisualsEnabled == enabled) return;
    _weatherVisualsEnabled = enabled;
    Preferences prefs;
    if (prefs.begin(CFG_NVS_NAMESPACE, false)) {
        prefs.putBool("wxVisual", enabled);
        prefs.end();
    }
    EventBus::displayDirty = true;
}

void ConfigManager::setDisplay(const CfgDisplay& d) {
    _display = d;
    save();
    EventBus::displayDirty = true;  // DisplayManager relira au prochain update()
}

void ConfigManager::setZoneMode(uint8_t z, uint8_t mode) {
    if (z >= MAX_ZONES) return;
    _zones[z].mode = mode;
    save();
}

void ConfigManager::setZoneIntervalDays(uint8_t z, uint8_t days) {
    if (z >= MAX_ZONES) return;
    _zones[z].intervalDays = days;
    save();
}

void ConfigManager::setZoneRain(uint8_t z, float threshMm, uint8_t hours) {
    if (z >= MAX_ZONES) return;
    _zones[z].rain = CfgRain(threshMm, hours);
    save();
}

void ConfigManager::setZoneDaySlot(uint8_t z, uint8_t day, uint8_t slotIdx,
                                    uint8_t h, uint8_t m,
                                    uint16_t dur, bool enabled) {
    if (z >= MAX_ZONES || day >= NB_DAYS || slotIdx >= MAX_SLOTS) return;
    _zones[z].daySlots[day].slots[slotIdx] = CfgSlot(h, m, dur, enabled);
    save();
}

void ConfigManager::setZoneIntervalSlot(uint8_t z, uint8_t slotIdx,
                                         uint8_t h, uint8_t m,
                                         uint16_t dur, bool enabled) {
    if (z >= MAX_ZONES || slotIdx >= MAX_SLOTS) return;
    _zones[z].intervalSlots.slots[slotIdx] = CfgSlot(h, m, dur, enabled);
    save();
}

void ConfigManager::syncZoneFromSchedule(uint8_t z, const ZoneSchedule& zs) {
    if (z >= MAX_ZONES) return;
    _zones[z].mode         = zs.mode;
    _zones[z].intervalDays = zs.intervalDays;
    _zones[z].rain         = CfgRain(zs.rain.thresholdMm, zs.rain.forecastHours);

    for (uint8_t d = 0; d < NB_DAYS; d++) {
        for (uint8_t s = 0; s < MAX_SLOTS; s++) {
            const TimeSlot& ts = zs.daySlots[d].slots[s];
            _zones[z].daySlots[d].slots[s] =
                CfgSlot(ts.hour, ts.minute, ts.duration, ts.enabled);
        }
    }
    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
        const TimeSlot& ts = zs.intervalSlots.slots[s];
        _zones[z].intervalSlots.slots[s] =
            CfgSlot(ts.hour, ts.minute, ts.duration, ts.enabled);
    }
    save();
}

// ═══════════════════════════════════════════════════════════════
//  Helpers JSON ↔ structs (privés)
// ═══════════════════════════════════════════════════════════════

void ConfigManager::zoneToJson(uint8_t z, JsonObject& obj) const {
    const CfgZone& cz = _zones[z];
    obj["name"]         = cz.name;
    obj["mode"]         = cz.mode;
    obj["intervalDays"] = cz.intervalDays;

    JsonObject rain = obj["rain"].to<JsonObject>();
    rain["threshMm"] = cz.rain.thresholdMm;
    rain["hours"]    = cz.rain.forecastHours;

    JsonArray dayArr = obj["daySlots"].to<JsonArray>();
    for (uint8_t d = 0; d < NB_DAYS; d++) {
        JsonArray dayRow = dayArr.add<JsonArray>();
        for (uint8_t s = 0; s < MAX_SLOTS; s++) {
            const CfgSlot& sl = cz.daySlots[d].slots[s];
            JsonObject so = dayRow.add<JsonObject>();
            so["h"] = sl.hour;
            so["m"] = sl.minute;
            so["d"] = sl.duration;
            so["e"] = sl.enabled;
        }
    }

    JsonArray intArr = obj["intervalSlots"].to<JsonArray>();
    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
        const CfgSlot& sl = cz.intervalSlots.slots[s];
        JsonObject so = intArr.add<JsonObject>();
        so["h"] = sl.hour;
        so["m"] = sl.minute;
        so["d"] = sl.duration;
        so["e"] = sl.enabled;
    }
}

bool ConfigManager::zoneFromJson(uint8_t z, JsonObjectConst obj) {
    if (!obj) return false;
    CfgZone& cz = _zones[z];

    // Nom (v2 — peut être absent en v1)
    const char* nm = obj["name"] | "";
    if (nm[0] != '\0') {
        strlcpy(cz.name, nm, sizeof(cz.name));
    } else {
        snprintf(cz.name, sizeof(cz.name), "Zone %u", z + 1);
    }

    cz.mode         = obj["mode"]         | (uint8_t)0;
    cz.intervalDays = obj["intervalDays"] | (uint8_t)2;

    JsonObjectConst rain = obj["rain"];
    if (rain) {
        cz.rain.thresholdMm   = rain["threshMm"] | 2.0f;
        cz.rain.forecastHours = rain["hours"]    | (uint8_t)24;
    }

    JsonArrayConst dayArr = obj["daySlots"];
    if (dayArr) {
        uint8_t d = 0;
        for (JsonArrayConst dayRow : dayArr) {
            if (d >= NB_DAYS) break;
            uint8_t s = 0;
            for (JsonObjectConst so : dayRow) {
                if (s >= MAX_SLOTS) break;
                cz.daySlots[d].slots[s] = CfgSlot(
                    so["h"] | (uint8_t)6,
                    so["m"] | (uint8_t)0,
                    so["d"] | (uint16_t)5,
                    so["e"] | false
                );
                s++;
            }
            d++;
        }
    }

    JsonArrayConst intArr = obj["intervalSlots"];
    if (intArr) {
        uint8_t s = 0;
        for (JsonObjectConst so : intArr) {
            if (s >= MAX_SLOTS) break;
            cz.intervalSlots.slots[s] = CfgSlot(
                so["h"] | (uint8_t)6,
                so["m"] | (uint8_t)0,
                so["d"] | (uint16_t)5,
                so["e"] | false
            );
            s++;
        }
    }

    return true;
}