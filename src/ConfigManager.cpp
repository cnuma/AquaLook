#include "ConfigManager.h"
#include "WebAssetsUpdater.h"
#include "BootLoopGuard.h"
#include "EventBus.h"
#include "EventLog.h"
#include "TimeUtils.h"
#include "FaultManager.h"
#include <Preferences.h>
#include <cstring>
#include <cstddef>

namespace {
constexpr uint32_t NVS_MAGIC = 0x414C4F4BUL; // "ALOK"
// Delai de groupement des sauvegardes differees : assez long pour absorber
// une rafale de MAX_SLOTS requetes consecutives (meme jour, memes zone),
// assez court pour rester ecrit en NVS bien avant qu'on l'oublie.
constexpr uint32_t SAVE_DEBOUNCE_MS = 800UL;

struct PersistedConfigV1 {
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

struct PersistedConfigV2 {
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
    uint8_t zoneNotificationMasks[MAX_ZONES];
    uint32_t crc32;
};

// Schema 3 : source des previsions meteo.
//
// Le champ est ajoute EN QUEUE, jamais au milieu. Toute insertion decalerait
// les zones et ferait rejeter le bloc existant a la relecture -- c'est-a-dire
// effacerait le planning d'arrosage de quelqu'un.
//
// reserved existe pour que le prochain reglage n'impose pas une migration de
// plus : le compilateur inserait de toute facon ces trois octets de bourrage
// avant crc32, autant les nommer et s'en servir.
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
    uint8_t zoneNotificationMasks[MAX_ZONES];
    uint8_t weatherProvider;
    uint8_t reserved[3];
    uint32_t crc32;
};

static_assert(offsetof(PersistedConfigV2, zoneNotificationMasks) ==
                  offsetof(PersistedConfigV1, crc32),
              "Le prefixe NVS schema 1 doit rester strictement identique");
static_assert(sizeof(PersistedConfigV2) ==
                  sizeof(PersistedConfigV1) + MAX_ZONES,
              "Le schema 2 doit ajouter exactement un uint8_t par zone");
static_assert(offsetof(PersistedConfig, weatherProvider) ==
                  offsetof(PersistedConfigV2, crc32),
              "Le prefixe NVS schema 2 doit rester strictement identique");
static_assert(sizeof(PersistedConfig) == sizeof(PersistedConfigV2) + 4U,
              "Le schema 3 doit ajouter exactement quatre octets en queue");

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
        memset(_intervalAnchorDays, 0, sizeof(_intervalAnchorDays));
        Preferences prefs;
        if (prefs.begin(CFG_NVS_NAMESPACE, true)) {
            _weatherVisualsEnabled = prefs.getBool("wxVisual", false);
            if (prefs.getBytesLength(CFG_NVS_INTERVAL_ANCHORS_KEY) ==
                sizeof(_intervalAnchorDays)) {
                prefs.getBytes(CFG_NVS_INTERVAL_ANCHORS_KEY,
                               _intervalAnchorDays,
                               sizeof(_intervalAnchorDays));
            }
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

    if (_nvsRejected) {
        EventLog::log(LOG_ERROR,
                      "Config: NVS invalide conserve sans ecrasement; defauts RAM actifs");
        defaults();
        return;
    }

    EventLog::log(LOG_WARN, "Config: NVS/JSON absents — valeurs par defaut");
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
    // Toute taille historique encore migrable doit figurer ici, SANS EXCEPTION.
    //
    // Cette garde s'execute avant les branches de migration : une taille
    // oubliee ne produit pas une migration ratee mais un bloc rejete, donc une
    // configuration d'usine ecrite par-dessus. Le 3 septembre 2026, avoir
    // ajoute la branche schema 2 -> 3 sans toucher a cette ligne a efface le
    // WiFi et le planning du module d'essai.
    if (len != sizeof(PersistedConfig) &&
        len != sizeof(PersistedConfigV2) &&
        len != sizeof(PersistedConfigV1)) {
        _nvsRejected = len != 0U;
        prefs.end();
        if (len != 0) EventLog::log(LOG_WARN, "Config: taille NVS invalide (%u/%u)",
                                     (unsigned)len, (unsigned)sizeof(PersistedConfig));
        return false;
    }

    uint8_t* raw = static_cast<uint8_t*>(malloc(len));
    if (!raw) {
        prefs.end();
        EventLog::log(LOG_ERROR, "Config: allocation lecture NVS impossible");
        return false;
    }

    const size_t read = prefs.getBytes(CFG_NVS_KEY, raw, len);
    prefs.end();

    // ── Migration schema 2 -> 3 ──────────────────────────────────────────
    //
    // Le bloc de schema 2 est un prefixe exact du schema 3 : on le relit tel
    // quel et on choisit la valeur du champ ajoute.
    //
    // Choix de la source par defaut a la migration : on NE bascule PAS un
    // module qui marche. Une installation ayant deja une clef OpenWeatherMap
    // la garde, son comportement ne change pas d'un flash. Une installation
    // sans clef n'avait aucune meteo : Open-Meteo lui en donne une sans rien
    // demander. L'utilisateur reste libre de changer depuis l'interface.
    if (len == sizeof(PersistedConfigV2)) {
        PersistedConfigV2* v2 = reinterpret_cast<PersistedConfigV2*>(raw);
        bool valid = read == len && v2->magic == NVS_MAGIC &&
                     v2->schema == 2U && v2->payloadSize == len;
        if (valid) {
            valid = crc32Bytes(raw, offsetof(PersistedConfigV2, crc32)) == v2->crc32;
        }
        if (!valid) {
            _nvsRejected = true;
            EventLog::log(LOG_ERROR, "Config: bloc NVS schema 2 invalide");
            free(raw);
            return false;
        }
        _wifi = v2->wifi;
        _touch = v2->touch;
        _manual = v2->manual;
        _ntp = v2->ntp;
        _owm = v2->owm;
        _system = v2->system;
        _display = v2->display;
        memcpy(_zones, v2->zones, sizeof(_zones));
        memcpy(_zoneNotificationMasks, v2->zoneNotificationMasks,
               sizeof(_zoneNotificationMasks));
        _weatherProvider = (_owm.apiKey[0] != '\0')
                           ? WEATHER_PROVIDER_OWM : WEATHER_PROVIDER_OPEN_METEO;
        free(raw);
        for (uint8_t z = 0; z < MAX_ZONES; ++z) {
            _zoneNotificationMasks[z] &= ZONE_NOTIFY_MASK;
        }
        _system.relayController = (_system.relayController <= RELAY_CONTROLLER_MCP23017)
                                  ? _system.relayController : RELAY_CONTROLLER_XL9535;
        _system.nbZones = normalizeActiveZones(_system.nbZones, _system.relayController);
        _system.nbRelaisPhysical = _system.nbZones;
        _system.relayLogic = (_system.relayLogic <= 1) ? _system.relayLogic : 1;
        _loaded = true;
        save();
        Preferences check;
        bool migrated = false;
        if (check.begin(CFG_NVS_NAMESPACE, true)) {
            migrated = check.getBytesLength(CFG_NVS_KEY) == sizeof(PersistedConfig);
            check.end();
        }
        EventLog::log(migrated ? LOG_INFO : LOG_ERROR,
                      migrated ? "Config: migration NVS schema 2 -> 3 reussie, meteo=%s"
                               : "Config: migration NVS schema 2 -> 3 non confirmee, meteo=%s",
                      _weatherProvider == WEATHER_PROVIDER_OWM ? "owm" : "open-meteo");
        return true;
    }

    if (len == sizeof(PersistedConfigV1)) {
        PersistedConfigV1* legacy = reinterpret_cast<PersistedConfigV1*>(raw);
        bool valid = read == len && legacy->magic == NVS_MAGIC &&
                     legacy->schema == 1U && legacy->payloadSize == len;
        if (valid) {
            valid = crc32Bytes(raw, offsetof(PersistedConfigV1, crc32)) == legacy->crc32;
        }
        if (!valid) {
            _nvsRejected = true;
            EventLog::log(LOG_ERROR, "Config: bloc NVS schema 1 invalide");
            free(raw);
            return false;
        }
        _wifi = legacy->wifi;
        _touch = legacy->touch;
        _manual = legacy->manual;
        _ntp = legacy->ntp;
        _owm = legacy->owm;
        _system = legacy->system;
        _display = legacy->display;
        memcpy(_zones, legacy->zones, sizeof(_zones));
        memset(_zoneNotificationMasks, 0, sizeof(_zoneNotificationMasks));
        free(raw);
        _system.relayController = (_system.relayController <= RELAY_CONTROLLER_MCP23017)
                                  ? _system.relayController : RELAY_CONTROLLER_XL9535;
        _system.nbZones = normalizeActiveZones(_system.nbZones, _system.relayController);
        _system.nbRelaisPhysical = _system.nbZones;
        _system.relayLogic = (_system.relayLogic <= 1) ? _system.relayLogic : 1;
        _loaded = true;
        save();
        Preferences check;
        bool migrated = false;
        if (check.begin(CFG_NVS_NAMESPACE, true)) {
            migrated = check.getBytesLength(CFG_NVS_KEY) == sizeof(PersistedConfig);
            check.end();
        }
        EventLog::log(migrated ? LOG_INFO : LOG_ERROR,
                      migrated ? "Config: migration NVS schema 1 -> 2 reussie"
                               : "Config: migration NVS schema 1 -> 2 non confirmee");
        return true;
    }

    PersistedConfig* blob = reinterpret_cast<PersistedConfig*>(raw);

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
        _nvsRejected = true;
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
    memcpy(_zoneNotificationMasks, blob->zoneNotificationMasks,
           sizeof(_zoneNotificationMasks));
    _weatherProvider = (blob->weatherProvider <= WEATHER_PROVIDER_OPEN_METEO)
                       ? blob->weatherProvider : WEATHER_PROVIDER_OWM;
    for (uint8_t z = 0; z < MAX_ZONES; ++z) {
        _zoneNotificationMasks[z] &= ZONE_NOTIFY_MASK;
    }
    free(raw);

    _system.relayController = (_system.relayController <= RELAY_CONTROLLER_MCP23017)
                              ? _system.relayController : RELAY_CONTROLLER_XL9535;
    _system.nbZones = normalizeActiveZones(_system.nbZones, _system.relayController);
    _system.nbRelaisPhysical = _system.nbZones;
    _system.relayLogic = (_system.relayLogic <= 1) ? _system.relayLogic : 1;

    // Couleur "zone active" : bleu depuis le 30 aout 2026, pour que le
    // ruban WS2812, le LCD et l'interface Web decrivent une zone en cours
    // d'arrosage de la meme facon. L'ancien #382020 etait un brun rougeatre
    // sans rapport avec les deux autres surfaces.
    //
    // Migration CONDITIONNELLE : on ne remplace la valeur que si elle vaut
    // encore l'ancien defaut, c'est-a-dire si l'utilisateur ne l'a jamais
    // choisie lui-meme. Une couleur reellement personnalisee est conservee -
    // ecraser un reglage explicite serait une regression, pas une migration.
    if (strcmp(_display.cActiveBg, "#382020") == 0) {
        strlcpy(_display.cActiveBg, "#10283c", sizeof(_display.cActiveBg));
        EventLog::log(LOG_INFO,
                      "Config: couleur zone active passee au bleu (defaut historique)");
    }

    _loaded = true;
    loadWindAlert();
    loadWebAssetsUrl();
    {
        Preferences prefs;
        if (prefs.begin(CFG_NVS_NAMESPACE, true)) {
            _configRevision = prefs.getUInt(CFG_NVS_REVISION_KEY, 0);
            prefs.end();
        }
    }
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
    _weatherProvider = WEATHER_PROVIDER_OPEN_METEO;
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
// Incremente la version de configuration et la persiste. Appelee par save()
// et par les ecritures qui ne passent pas par le bloc principal (seuils
// d'alerte vent), pour qu'AUCUNE modification locale n'echappe au compteur -
// sinon une commande distante pourrait s'appliquer sur une base perimee en
// croyant etre a jour.
// Increment EN MEMOIRE seulement : la persistance suit dans save().
// Ecrire en NVS a chaque appel annulerait le benefice de deferSave(), qui
// regroupe une rafale de modifications en une seule ecriture.
//
// Le drapeau garantit UN increment par lot logique : deferSave() incremente
// tout de suite (pour que la version soit juste immediatement), save()
// n'incremente que s'il n'a pas ete precede d'un deferSave - sinon un
// enregistrement differe compterait deux fois, et un appel direct a save()
// comme setDisplay() ne compterait pas du tout.
void ConfigManager::bumpRevision() {
    if (_revisionBumped) return;
    _configRevision++;
    _revisionBumped = true;
}

// Persiste la version courante. Appelee depuis save(), ou l'ecriture NVS a
// deja lieu de toute facon.
void ConfigManager::persistRevision() {
    Preferences prefs;
    if (prefs.begin(CFG_NVS_NAMESPACE, false)) {
        prefs.putUInt(CFG_NVS_REVISION_KEY, _configRevision);
        prefs.end();
    }
}

void ConfigManager::save() {
    PersistedConfig* blob = static_cast<PersistedConfig*>(malloc(sizeof(PersistedConfig)));
    if (!blob) {
        EventLog::log(LOG_ERROR, "Config: allocation sauvegarde NVS impossible");
        return;
    }

    memset(blob, 0, sizeof(PersistedConfig));
    blob->magic = NVS_MAGIC;
    blob->schema = CFG_NVS_SCHEMA;
    blob->weatherProvider = _weatherProvider;
    memset(blob->reserved, 0, sizeof(blob->reserved));
    blob->payloadSize = sizeof(PersistedConfig);
    blob->wifi = _wifi;
    blob->touch = _touch;
    blob->manual = _manual;
    blob->ntp = _ntp;
    blob->owm = _owm;
    blob->system = _system;
    blob->display = _display;
    memcpy(blob->zones, _zones, sizeof(_zones));
    memcpy(blob->zoneNotificationMasks, _zoneNotificationMasks,
           sizeof(_zoneNotificationMasks));
    blob->crc32 = crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                             offsetof(PersistedConfig, crc32));

    Preferences prefs;
    if (!prefs.begin(CFG_NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "Config: ouverture NVS ecriture impossible");
        markSaveFailed();
        free(blob);
        return;
    }

    const size_t written = prefs.putBytes(CFG_NVS_KEY, blob, sizeof(PersistedConfig));
    prefs.end();
    free(blob);

    if (written != sizeof(PersistedConfig)) {
        EventLog::log(LOG_ERROR, "Config: ecriture NVS incomplete (%u/%u)",
                      (unsigned)written, (unsigned)sizeof(PersistedConfig));
        markSaveFailed();
        return;
    }

    markSaveOk();
    EventLog::log(LOG_INFO, "Config: sauvegarde NVS OK (%u octets, schema %u)",
                  (unsigned)written, CFG_NVS_SCHEMA);
    bumpRevision();        // sans effet si deferSave l'a deja fait
    persistRevision();
    _revisionBumped = false;   // lot suivant
}

// Un echec de sauvegarde doit devenir VISIBLE, pas rester au fond d'un
// journal. Sans cela, l'utilisateur modifie un reglage, l'interface confirme,
// et la perte n'est decouverte qu'au redemarrage suivant — le pire cas pour
// la confiance, car rien ne signale que quelque chose a mal tourne.
void ConfigManager::markSaveFailed() {
    _saveFailed = true;
    FaultManager::setActive(FaultId::CONFIG_PERSIST, true);
    FaultManager::notifyError();   // voyant rouge jusqu'a acquittement explicite
}

void ConfigManager::markSaveOk() {
    _saveFailed = false;
    _saveSucceededOnce = true;
    FaultManager::setActive(FaultId::CONFIG_PERSIST, false);
}

void ConfigManager::deferSave() {
    // La version suit le changement LOGIQUE, pas le moment de l'ecriture.
    //
    // Elle etait auparavant incrementee dans save(), donc jusqu'a 800 ms
    // plus tard. Consequence mesuree le 29 aout 2026 : l'accuse d'une
    // commande distante annoncait la version d'AVANT son application. Plus
    // grave, une modification locale suivie d'une commande dans cette
    // fenetre aurait vu une version non encore incrementee - et la commande
    // se serait appliquee sur une base perimee en se croyant a jour, ce que
    // le verrouillage optimiste existe precisement pour empecher.
    bumpRevision();
    _saveDirty = true;
    _saveDueMs = millis() + SAVE_DEBOUNCE_MS;
}

void ConfigManager::update() {
    if (!_saveDirty) return;
    if (!AquaLook::Time::deadlineReached(millis(), _saveDueMs)) return;
    _saveDirty = false;
    save();
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
        sched.setIntervalAnchorDay(z, _intervalAnchorDays[z]);
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

uint32_t ConfigManager::intervalAnchorDay(uint8_t z) const {
    if (z >= MAX_ZONES) return 0;
    return _intervalAnchorDays[z];
}

// ═══════════════════════════════════════════════════════════════
//  Setters
// ═══════════════════════════════════════════════════════════════

void ConfigManager::setWifi(const char* ssid, const char* pwd) {
    strlcpy(_wifi.ssid,     ssid, sizeof(_wifi.ssid));
    strlcpy(_wifi.password, pwd,  sizeof(_wifi.password));
    save();
    // Invariant I10 : l'appelant (WebManager) a déjà envoyé sendOk()
    BootLoopGuard::restartDeliberately("changement des identifiants WiFi");
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

void ConfigManager::setWeatherProvider(uint8_t provider) {
    if (provider > WEATHER_PROVIDER_OPEN_METEO) return;
    if (provider == _weatherProvider) return;
    _weatherProvider = provider;
    save();
    EventBus::configDirty = true;
}

void ConfigManager::setResolvedCoordinates(float lat, float lon) {
    if (lat == 0.0f && lon == 0.0f) return;
    if (_owm.lat == lat && _owm.lon == lon) return;
    _owm.lat = lat;
    _owm.lon = lon;
    save();
    // Pas de configDirty : la position n'a pas change, on vient seulement de
    // l'ecrire en clair. Relancer les managers pour cela declencherait un
    // second appel meteo immediat, juste apres celui qui a resolu la ville.
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
    BootLoopGuard::restartDeliberately("changement du nombre de zones");
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

void ConfigManager::setZoneNotificationMask(uint8_t z, uint8_t mask) {
    if (z >= MAX_ZONES) return;
    _zoneNotificationMasks[z] = mask & ZONE_NOTIFY_MASK;
    save();
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

// Seuils d'alerte vent : cle NVS distincte du blob principal (voir la note
// sur CFG_NVS_WIND_ALERT_KEY dans ConfigManager.h). Absence de la cle =
// valeurs par defaut, ce qui rend la lecture sure sur une installation qui
// n'a jamais enregistre ces reglages.
void ConfigManager::loadWindAlert() {
    Preferences prefs;
    if (!prefs.begin(CFG_NVS_NAMESPACE, true)) return;
    CfgWindAlert w;
    const size_t read = prefs.getBytes(CFG_NVS_WIND_ALERT_KEY, &w, sizeof(w));
    prefs.end();
    if (read == sizeof(w)) {
        _windAlert.gustKmh   = constrain(w.gustKmh,   (uint8_t)5, (uint8_t)150);
        _windAlert.severeKmh = constrain(w.severeKmh, (uint8_t)5, (uint8_t)200);
    }
}

void ConfigManager::setWindAlert(const CfgWindAlert& w) {
    _windAlert.gustKmh   = constrain(w.gustKmh,   (uint8_t)5, (uint8_t)150);
    _windAlert.severeKmh = constrain(w.severeKmh, (uint8_t)5, (uint8_t)200);

    Preferences prefs;
    if (prefs.begin(CFG_NVS_NAMESPACE, false)) {
        prefs.putBytes(CFG_NVS_WIND_ALERT_KEY, &_windAlert, sizeof(_windAlert));
        prefs.end();
    }
    bumpRevision();
    persistRevision();
    _revisionBumped = false;
    EventBus::displayDirty = true;   // le bandeau meteo relira au prochain rendu
}

// URL du manifeste des ressources Web. Cle NVS distincte du blob principal
// (voir la note sur CFG_NVS_WEBASSETS_URL_KEY dans ConfigManager.h).
// Absence de la cle = URL par defaut, donc sans effet sur une installation
// qui n'a jamais configure ce reglage.
void ConfigManager::loadWebAssetsUrl() {
    strlcpy(_webAssetsUrl, WebAssetsUpdater::DEFAULT_MANIFEST_URL,
            sizeof(_webAssetsUrl));

    Preferences prefs;
    if (!prefs.begin(CFG_NVS_NAMESPACE, true)) return;
    char buf[WEBASSETS_URL_MAX] = "";
    const size_t read = prefs.getBytes(CFG_NVS_WEBASSETS_URL_KEY, buf, sizeof(buf));
    prefs.end();

    // Relecture defensive : une valeur enregistree par une version anterieure,
    // ou corrompue, ne doit pas pouvoir degrader le canal en clair. On
    // repasse donc par la meme validation que l'ecriture.
    if (read > 0 && read <= sizeof(buf)) {
        buf[sizeof(buf) - 1] = '\0';
        if (strncmp(buf, "https://", 8) == 0) {
            strlcpy(_webAssetsUrl, buf, sizeof(_webAssetsUrl));
        } else {
            EventLog::log(LOG_WARN,
                          "WebAssets: URL enregistree non HTTPS, defaut retabli");
        }
    }

    // La configuration est l'autorite : elle pousse la source vers
    // WebAssetsUpdater, qui n'a ainsi aucune dependance vers elle.
    //
    // Cet appel manquait, et le defaut etait retors : le setter le faisait
    // deja, donc changer la source fonctionnait dans la session courante et
    // l'URL etait bien relue au demarrage suivant. Mais rien ne la
    // retransmettait a WebAssetsUpdater, dont la copie restait vide - et
    // manifestUrl() retombait sur GitHub. Or le telechargement a lieu en
    // mode maintenance, apres redemarrage : la source configuree etait donc
    // ignoree exactement la ou elle sert. Constate le 30 aout 2026 sur la
    // premiere publication reelle, le module ayant deploye la release
    // GitHub 5.9.7 au lieu du 5.9.8 publie sur l'hebergement.
    EventLog::log(LOG_INFO, "WebAssets: source = %s", _webAssetsUrl);
    WebAssetsUpdater::setManifestUrl(_webAssetsUrl);
}

bool ConfigManager::setWebAssetsUrl(const char* url) {
    if (url == nullptr) return false;

    // HTTPS obligatoire, et ce n'est pas une precaution de principe.
    //
    // Le manifeste porte les SHA-256 qui authentifient chaque fichier. Servi
    // en clair, n'importe qui sur le trajet peut en substituer un autre, avec
    // ses propres hashes et ses propres fichiers : la verification par hash
    // ne protege alors plus de rien, elle valide l'attaque. C'est la
    // confiance dans le manifeste qui fait tenir toute la chaine, et seul le
    // TLS l'etablit.
    if (strncmp(url, "https://", 8) != 0) {
        EventLog::log(LOG_ERROR, "WebAssets: URL refusee, HTTPS obligatoire");
        return false;
    }
    if (strlen(url) >= sizeof(_webAssetsUrl)) {
        EventLog::log(LOG_ERROR, "WebAssets: URL trop longue");
        return false;
    }

    strlcpy(_webAssetsUrl, url, sizeof(_webAssetsUrl));

    Preferences prefs;
    if (prefs.begin(CFG_NVS_NAMESPACE, false)) {
        prefs.putBytes(CFG_NVS_WEBASSETS_URL_KEY, _webAssetsUrl,
                       strlen(_webAssetsUrl) + 1);
        prefs.end();
    }
    WebAssetsUpdater::setManifestUrl(_webAssetsUrl);
    bumpRevision();
    persistRevision();
    _revisionBumped = false;
    EventLog::log(LOG_INFO, "WebAssets: source de mise a jour modifiee");
    return true;
}

void ConfigManager::setDisplay(const CfgDisplay& d) {
    _display = d;
    save();
    EventBus::displayDirty = true;  // DisplayManager relira au prochain update()
}

void ConfigManager::setZoneMode(uint8_t z, uint8_t mode, uint32_t anchorDay) {
    if (z >= MAX_ZONES || mode > SCHEDULE_MODE_INTERVAL) return;

    const bool modeChanged = (_zones[z].mode != mode);
    bool anchorChanged = false;

    // Une entrée en mode intervalle crée une origine fixe du cycle.
    // Une zone intervalle migrée avec ancre absente est initialisée une seule fois.
    if (mode == SCHEDULE_MODE_INTERVAL && anchorDay > 0 &&
        (modeChanged || _intervalAnchorDays[z] == 0)) {
        _intervalAnchorDays[z] = anchorDay;
        anchorChanged = true;
    }

    if (anchorChanged) {
        Preferences prefs;
        if (prefs.begin(CFG_NVS_NAMESPACE, false)) {
            prefs.putBytes(CFG_NVS_INTERVAL_ANCHORS_KEY,
                           _intervalAnchorDays,
                           sizeof(_intervalAnchorDays));
            prefs.end();
        }
    }

    // Ne pas reecrire le gros bloc de configuration si seul l'ancrage manquait.
    if (modeChanged) {
        _zones[z].mode = mode;
        save();
    }
}

void ConfigManager::setZoneIntervalDays(uint8_t z, uint8_t days) {
    if (z >= MAX_ZONES) return;
    _zones[z].intervalDays = constrain(days, (uint8_t)1, (uint8_t)30);
    save();
}

void ConfigManager::setZoneIntervalAnchorDay(uint8_t z, uint32_t epochDay) {
    if (z >= MAX_ZONES) return;
    _intervalAnchorDays[z] = epochDay;

    Preferences prefs;
    if (prefs.begin(CFG_NVS_NAMESPACE, false)) {
        prefs.putBytes(CFG_NVS_INTERVAL_ANCHORS_KEY,
                       _intervalAnchorDays,
                       sizeof(_intervalAnchorDays));
        prefs.end();
    }
}

void ConfigManager::clearZoneIntervalProgramming(uint8_t z) {
    if (z >= MAX_ZONES) return;

    _zones[z].mode = SCHEDULE_MODE_DAYS;
    _zones[z].intervalDays = 2;
    _intervalAnchorDays[z] = 0;
    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
        _zones[z].intervalSlots.slots[s] = CfgSlot();
    }

    save();

    Preferences prefs;
    if (prefs.begin(CFG_NVS_NAMESPACE, false)) {
        prefs.putBytes(CFG_NVS_INTERVAL_ANCHORS_KEY,
                       _intervalAnchorDays,
                       sizeof(_intervalAnchorDays));
        prefs.end();
    }
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
    deferSave();
}

void ConfigManager::setZoneIntervalSlot(uint8_t z, uint8_t slotIdx,
                                         uint8_t h, uint8_t m,
                                         uint16_t dur, bool enabled) {
    if (z >= MAX_ZONES || slotIdx >= MAX_SLOTS) return;
    _zones[z].intervalSlots.slots[slotIdx] = CfgSlot(h, m, dur, enabled);
    deferSave();
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
    obj["notificationMask"] = _zoneNotificationMasks[z];

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
    _zoneNotificationMasks[z] =
        (obj["notificationMask"] | (uint8_t)0) & ZONE_NOTIFY_MASK;

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
