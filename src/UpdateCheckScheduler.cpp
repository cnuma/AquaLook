#include "UpdateCheckScheduler.h"

#include <Preferences.h>

#include "ConfigManager.h"
#include "EventLog.h"
#include "MaintenanceRequest.h"
#include "RelaisManager.h"
#include "WiFiManager.h"

// Namespace NVS dedie, hors du blob de configuration principal. Ce blob est
// une structure binaire versionnee et protegee par CRC : y ajouter un champ
// impose une migration de schema, et une migration ratee ne perd pas ce
// reglage-la mais TOUTE la configuration (WiFi, zones, planning). Vu ce que
// la saturation NVS du 16 aout 2026 a deja coute, le reglage d'une simple
// vérification periodique ne justifie pas ce risque.
//
// Contrepartie assumee : un namespace de plus a connaitre lors d'une
// restauration. Il est declare dans l'enumeration de /api/debug/nvs-stats
// (WebManager.cpp) pour qu'il ne puisse pas etre oublie, comme l'ont ete
// aq_notify et aq_log_cfg lors de la restauration du 16 aout.
static constexpr const char* NVS_NAMESPACE = "aq_upd_chk";
static constexpr const char* KEY_ENABLED   = "en";
static constexpr const char* KEY_HOUR      = "h";
static constexpr const char* KEY_MINUTE    = "m";
static constexpr const char* KEY_INTERVAL  = "days";
static constexpr const char* KEY_LAST_DAY  = "lastday";

// Un refus n'est journalise qu'une fois par heure : la boucle principale
// passe ici en permanence, et une condition durablement non remplie (pas de
// reseau la nuit, par exemple) noierait le journal.
static constexpr uint32_t BLOCKED_LOG_INTERVAL_MS = 3600000UL;

void UpdateCheckScheduler::begin() {
    load();
    _loaded = true;
    EventLog::log(LOG_INFO,
                  "Verif MAJ: %s a %02u:%02u tous les %u jour(s)",
                  _cfg.enabled ? "activee" : "desactivee",
                  static_cast<unsigned>(_cfg.hour),
                  static_cast<unsigned>(_cfg.minute),
                  static_cast<unsigned>(_cfg.intervalDays));
}

void UpdateCheckScheduler::load() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return;
    _cfg.enabled      = prefs.getBool(KEY_ENABLED, true);
    _cfg.hour         = prefs.getUChar(KEY_HOUR, 3U);
    _cfg.minute       = prefs.getUChar(KEY_MINUTE, 30U);
    _cfg.intervalDays = prefs.getUChar(KEY_INTERVAL, 1U);
    _lastCheckEpochDay = prefs.getULong(KEY_LAST_DAY, 0UL);
    prefs.end();

    if (_cfg.hour > 23U) _cfg.hour = 3U;
    if (_cfg.minute > 59U) _cfg.minute = 30U;
    if (_cfg.intervalDays == 0U || _cfg.intervalDays > 30U) _cfg.intervalDays = 1U;
}

void UpdateCheckScheduler::save() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "Verif MAJ: enregistrement du reglage impossible");
        return;
    }
    prefs.putBool(KEY_ENABLED, _cfg.enabled);
    prefs.putUChar(KEY_HOUR, _cfg.hour);
    prefs.putUChar(KEY_MINUTE, _cfg.minute);
    prefs.putUChar(KEY_INTERVAL, _cfg.intervalDays);
    prefs.end();
}

void UpdateCheckScheduler::saveLastCheckDay(uint32_t epochDay) {
    _lastCheckEpochDay = epochDay;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putULong(KEY_LAST_DAY, epochDay);
    prefs.end();
}

bool UpdateCheckScheduler::set(bool enabled, uint8_t hour, uint8_t minute,
                               uint8_t intervalDays) {
    if (hour > 23U || minute > 59U) return false;
    if (intervalDays == 0U || intervalDays > 30U) return false;

    _cfg.enabled = enabled;
    _cfg.hour = hour;
    _cfg.minute = minute;
    _cfg.intervalDays = intervalDays;
    save();
    EventLog::log(LOG_INFO,
                  "Verif MAJ: reglage change -> %s a %02u:%02u tous les %u jour(s)",
                  enabled ? "activee" : "desactivee",
                  static_cast<unsigned>(hour),
                  static_cast<unsigned>(minute),
                  static_cast<unsigned>(intervalDays));
    return true;
}

void UpdateCheckScheduler::logBlocked(const char* reason) {
    const uint32_t nowMs = millis();
    if (nowMs - _blockedLogAtMs < BLOCKED_LOG_INTERVAL_MS) return;
    _blockedLogAtMs = nowMs;
    EventLog::log(LOG_INFO,
                  "Verif MAJ: echeance atteinte mais report — %s", reason);
}

void UpdateCheckScheduler::update(bool ntpSynced,
                                  int hour,
                                  int minute,
                                  uint32_t epochDay,
                                  const WiFiManager* wifi,
                                  const RelaisManager* relais,
                                  const ConfigManager* config) {
    if (!_loaded || _triggered || !_cfg.enabled) return;

    // Le WiFi doit etre stable, pas seulement associe. La duree est mesuree
    // ici plutot que dans WiFiManager : c'est une exigence propre a cette
    // decision, pas un etat que le reste du systeme aurait a connaitre.
    const uint32_t nowMs = millis();
    if (wifi == nullptr || !wifi->isConnected()) {
        _wifiConnectedSinceMs = 0U;
    } else if (_wifiConnectedSinceMs == 0U) {
        _wifiConnectedSinceMs = nowMs;
    }

    // Sans heure, pas de decision possible — et surtout, redemarrer dans cet
    // etat suspendrait l'arrosage programme (voir FaultId::TIME_UNSYNCED).
    if (!ntpSynced) return;

    // Premiere execution : on ne verifie pas immediatement. Sans cela, un
    // module installe en fin de journee redemarrerait dans la minute qui
    // suit, ce qui est inattendu et donne l'impression d'un defaut.
    if (_lastCheckEpochDay == 0U) {
        saveLastCheckDay(epochDay);
        EventLog::log(LOG_INFO,
                      "Verif MAJ: premiere echeance fixee au %02u:%02u dans %u jour(s)",
                      static_cast<unsigned>(_cfg.hour),
                      static_cast<unsigned>(_cfg.minute),
                      static_cast<unsigned>(_cfg.intervalDays));
        return;
    }

    if (epochDay < _lastCheckEpochDay) {
        // L'horloge a recule (correction NTP apres un demarrage sans reseau).
        // On repart de la date courante plutot que de bloquer indefiniment.
        saveLastCheckDay(epochDay);
        return;
    }
    if ((epochDay - _lastCheckEpochDay) < _cfg.intervalDays) return;

    const int nowMinutes = (hour * 60) + minute;
    const int dueMinutes = (static_cast<int>(_cfg.hour) * 60) +
                            static_cast<int>(_cfg.minute);
    if (nowMinutes < dueMinutes) return;

    // A partir d'ici l'echeance est atteinte : tout refus est journalise, pour
    // que l'absence de verification ne soit jamais silencieuse.
    if (_wifiConnectedSinceMs == 0U) {
        logBlocked("pas de connexion WiFi");
        return;
    }
    if ((nowMs - _wifiConnectedSinceMs) < WIFI_STABLE_MS) {
        logBlocked("connexion WiFi trop recente pour etre jugee stable");
        return;
    }
    if (config != nullptr && relais != nullptr) {
        for (uint8_t zone = 0U; zone < config->nbZones(); ++zone) {
            if (relais->getState(zone)) {
                logBlocked("arrosage en cours");
                return;
            }
        }
    }

    // Le jour est enregistre AVANT la demande de redemarrage. Si la
    // verification echoue, ou si le module ne revient pas comme prevu, il ne
    // peut pas repartir en boucle : la prochaine tentative attendra
    // l'echeance suivante.
    saveLastCheckDay(epochDay);

    if (!MaintenanceRequestStore::save(MaintenanceRequest::CHECK_VERSION)) {
        EventLog::log(LOG_ERROR,
                      "Verif MAJ: demande de maintenance non enregistree, "
                      "verification abandonnee pour cette echeance");
        return;
    }

    _triggered = true;
    EventLog::log(LOG_WARN,
                  "Verif MAJ: echeance %02u:%02u atteinte, redemarrage en mode "
                  "maintenance pour verifier les mises a jour",
                  static_cast<unsigned>(_cfg.hour),
                  static_cast<unsigned>(_cfg.minute));
    delay(200);
    ESP.restart();
}
