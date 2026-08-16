#include "EventLog.h"
#include <Preferences.h>

// ── Définitions des membres statiques ─────────────────────────
// Une seule définition par programme (ODR) — ici dans EventLog.cpp.
// PlatformIO compile automatiquement tous les .cpp de src/.
LogEntry EventLog::_buf[LOG_CAPACITY];
uint8_t  EventLog::_head     = 0;
uint8_t  EventLog::_count    = 0;
bool     EventLog::_hasErrors = false;
bool     EventLog::_timingLogsEnabled = true;

// Namespace NVS dedie, separe de ConfigManager : une preference d'affichage
// diagnostique, pas une donnee de configuration deliberee — meme logique
// que WiFiManager::_keepaliveHost (aq_wifi_ka).
static constexpr const char* TIMING_LOGS_NVS_NAMESPACE = "aq_log_cfg";
static constexpr const char* TIMING_LOGS_NVS_KEY = "timingOn";

void EventLog::begin() {
    Preferences prefs;
    if (!prefs.begin(TIMING_LOGS_NVS_NAMESPACE, true)) return;
    _timingLogsEnabled = prefs.getBool(TIMING_LOGS_NVS_KEY, true);
    prefs.end();
}

void EventLog::setTimingLogsEnabled(bool enabled, bool persist) {
    _timingLogsEnabled = enabled;
    if (!persist) return;

    Preferences prefs;
    if (!prefs.begin(TIMING_LOGS_NVS_NAMESPACE, false)) return;
    prefs.putBool(TIMING_LOGS_NVS_KEY, enabled);
    prefs.end();
}
