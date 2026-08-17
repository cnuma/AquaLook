#include "NTPManager.h"
#include "ConfigManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"

// ── Intervalles de poll (compile-time) ────────
static constexpr uint32_t POLL_BEFORE_SYNC_MS = 500;
static constexpr uint32_t POLL_AFTER_SYNC_MS  = 3600000UL;  // 1h

// -- Alerte "heure inconnue" ----------------------------------------------
//
// main.cpp n'appelle ScheduleManager::update() que si NTPManager est
// synchronise : sans heure, aucun arrosage programme ne demarre. Jusqu'ici
// cette situation etait totalement muette — le module cessait simplement
// d'arroser, sans defaut leve, sans voyant, sans ligne de journal.
//
// L'horloge interne survit a un redemarrage logiciel (verifie le 17 aout 2026 :
// apres esp_restart(), la premiere ligne de journal est deja horodatee), mais
// pas a une coupure d'alimentation. Le scenario reel est donc : coupure de
// courant, retour du courant sans reseau, et un arrosage qui ne repart jamais.
//
// Le seuil est volontairement large. Un demarrage normal synchronise en
// quelques secondes (WiFi connecte vers 5 s, heure valide dans la foulee).
// Cinq minutes ne peuvent pas etre atteintes par un simple demarrage un peu
// lent : une alerte a tort couterait plus cher que le silence qu'elle remplace.
static constexpr uint32_t UNSYNCED_ALERT_MS  = 300000UL;   // 5 min
// Tant que la situation dure, la rappeler : une alerte unique se perd dans le
// journal, et c'est justement le cas ou l'utilisateur cherche pourquoi son
// arrosage ne part pas.
static constexpr uint32_t UNSYNCED_REPEAT_MS = 1800000UL;  // 30 min

// ─────────────────────────────────────────────────────────────
void NTPManager::begin(ConfigManager* config) {
    _config = config;
    _beginMs = millis();
    applyConfig();
    EventLog::log(LOG_INFO, "NTP: synchronisation lancee");
}

// ─────────────────────────────────────────────────────────────
void NTPManager::update() {
    // Invariant I20 : relire les paramètres NTP si config modifiée
    if (EventBus::configDirty && _config) {
        applyConfig();
        _synced   = false;   // forcer resync avec les nouveaux paramètres
        _lastPoll = 0;
        EventLog::log(LOG_INFO, "NTP: reconfiguration suite a configDirty");
        // Ne pas remettre configDirty à false ici —
        // d'autres managers (WeatherManager) doivent aussi le lire
    }

    const uint32_t now      = millis();
    const uint32_t interval = _synced ? POLL_AFTER_SYNC_MS : POLL_BEFORE_SYNC_MS;
    if (now - _lastPoll < interval) return;
    _lastPoll = now;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {   // 0ms = strictement non bloquant
        if (!_synced) {
            _synced = true;
            // Ne pas positionner displayDirty ici : la synchronisation de l'heure
            // ne nécessite pas un fillScreen() ni un redraw complet. Le prochain
            // rafraîchissement dynamique nominal mettra à jour l'heure et les
            // informations temporelles sans blocage ni scintillement.
            const String timeStr = getTimeStr();
            EventLog::log(LOG_INFO, "NTP: synchronise %s", timeStr.c_str());
            if (_unsyncedFaultRaised) {
                _unsyncedFaultRaised = false;
                FaultManager::setActive(FaultId::TIME_UNSYNCED, false);
                EventLog::log(LOG_INFO,
                              "NTP: heure retrouvee %s — les arrosages "
                              "programmes reprennent",
                              timeStr.c_str());
            }
        }
        _lastSync = now;
        return;
    }

    // Pas d'heure. Tant qu'on est dans le delai de demarrage normal, c'est
    // attendu et on ne dit rien.
    if (now - _beginMs < UNSYNCED_ALERT_MS) return;

    if (!_unsyncedFaultRaised) {
        _unsyncedFaultRaised = true;
        _unsyncedLogAtMs = now;
        FaultManager::setActive(FaultId::TIME_UNSYNCED, true);
        FaultManager::notifyError();
        EventLog::log(LOG_ERROR,
                      "NTP: heure toujours inconnue apres %lu min — aucun "
                      "arrosage programme ne peut demarrer tant que l'heure "
                      "n'est pas connue (serveur=%s)",
                      static_cast<unsigned long>((now - _beginMs) / 60000UL),
                      (_config != nullptr) ? _config->ntp().server : "compile-time");
        return;
    }

    if (now - _unsyncedLogAtMs >= UNSYNCED_REPEAT_MS) {
        _unsyncedLogAtMs = now;
        EventLog::log(LOG_ERROR,
                      "NTP: heure toujours inconnue depuis %lu min — arrosage "
                      "programme toujours a l'arret",
                      static_cast<unsigned long>((now - _beginMs) / 60000UL));
    }
}

// ─────────────────────────────────────────────────────────────
//  Application des paramètres NTP depuis ConfigManager
// ─────────────────────────────────────────────────────────────
void NTPManager::applyConfig() {
    if (_config) {
        const CfgNtp& n = _config->ntp();
        configTime(n.gmtOffset, n.dstOffset, n.server);
        EventLog::log(
            LOG_INFO,
            "NTP: config serveur=%s gmt=%ld dst=%ld",
            n.server,
            n.gmtOffset,
            n.dstOffset
        );
    } else {
        // Fallback compile-time si pas de ConfigManager
        configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER1, NTP_SERVER2);
        EventLog::log(LOG_INFO, "NTP: config compile-time");
    }
}

// ─────────────────────────────────────────────────────────────
//  Getters
// ─────────────────────────────────────────────────────────────
bool NTPManager::isSynced() const { return _synced; }

bool NTPManager::fillTm(struct tm& out) const {
    return getLocalTime(&out, 0);
}

String NTPManager::getTimeStr() const {
    struct tm t;
    if (!fillTm(t)) return "--/--/---- --:--:--";
    char buf[24];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &t);
    return String(buf);
}

String NTPManager::getHHMM() const {
    struct tm t;
    if (!fillTm(t)) return "--:--";
    char buf[6];
    strftime(buf, sizeof(buf), "%H:%M", &t);
    return String(buf);
}

int NTPManager::getHour() const {
    struct tm t;
    return fillTm(t) ? t.tm_hour : -1;
}

int NTPManager::getMinute() const {
    struct tm t;
    return fillTm(t) ? t.tm_min : -1;
}

int NTPManager::getWeekday() const {
    struct tm t;
    return fillTm(t) ? t.tm_wday : -1;
}

int NTPManager::getDayOfMonth() const {
    struct tm t;
    return fillTm(t) ? t.tm_mday : -1;
}

uint32_t NTPManager::getEpochDay() const {
    time_t now;
    time(&now);
    return (now > 0) ? (uint32_t)(now / 86400UL) : 0;
}
