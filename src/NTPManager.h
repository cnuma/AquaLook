#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

// Forward declaration — évite l'include circulaire
class ConfigManager;

// ═══════════════════════════════════════════════════════════════
//  NTPManager — synchronisation heure non bloquante
//
//  Paramètres runtime (invariant I20) :
//    server, gmtOffset, dstOffset lus depuis ConfigManager.
//    Sur EventBus::configDirty → reconfigure configTime() sans reboot.
//
//  Politique de poll :
//    Avant sync  → toutes les NTP_POLL_INTERVAL_MS (500ms)
//    Après sync  → toutes les NTP_SYNC_INTERVAL_MS  (1h)
// ═══════════════════════════════════════════════════════════════
class NTPManager {
public:
    void begin(ConfigManager* config = nullptr);
    void update();

    bool     isSynced()      const;
    String   getTimeStr()    const;  // "DD/MM/YYYY HH:MM:SS"
    String   getHHMM()       const;  // "HH:MM"
    int      getHour()       const;
    int      getMinute()     const;
    int      getWeekday()    const;  // 0=dim..6=sam (tm_wday)
    int      getDayOfMonth() const;
    uint32_t getEpochDay()   const;  // epoch / 86400

private:
    ConfigManager* _config    = nullptr;
    bool           _synced    = false;
    uint32_t       _lastPoll  = 0;
    uint32_t       _lastSync  = 0;

    void applyConfig();
    bool fillTm(struct tm& out) const;
};
