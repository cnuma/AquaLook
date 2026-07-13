#include "NTPManager.h"
#include "ConfigManager.h"
#include "EventBus.h"
#include "EventLog.h"

// ── Intervalles de poll (compile-time) ────────
static constexpr uint32_t POLL_BEFORE_SYNC_MS = 500;
static constexpr uint32_t POLL_AFTER_SYNC_MS  = 3600000UL;  // 1h

// ─────────────────────────────────────────────────────────────
void NTPManager::begin(ConfigManager* config) {
    _config = config;
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
        }
        _lastSync = now;
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
