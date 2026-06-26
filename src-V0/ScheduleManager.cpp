#include "ScheduleManager.h"

void ScheduleManager::begin() {
    for (uint8_t z = 0; z < NB_ZONES; z++) {
        _zones[z].mode          = SCHEDULE_MODE_DAYS;
        _zones[z].intervalDays  = 2;
        _zones[z].lastWateredDay = 0;
        _zones[z].rain          = {DEFAULT_RAIN_THRESHOLD, DEFAULT_FORECAST_HOURS};
        _zones[z].forceToday    = false;

        // Init tous les slots à vide
        for (uint8_t d = 0; d < NB_DAYS; d++) {
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                _zones[z].daySlots[d].slots[s] = {6, 30, 5, false};
            }
        }
        for (uint8_t s = 0; s < MAX_SLOTS; s++) {
            _zones[z].intervalSlots.slots[s] = {6, 30, 5, false};
        }

        _lastReason[z] = "En attente";
    }

    // Zone 1 — lundi, mercredi, vendredi à 06:30 — 5min
    for (uint8_t d : {0, 2, 4}) {  // lun, mer, ven
        _zones[0].daySlots[d].slots[0] = {6, 30, 5, true};
    }

    // Zone 2 — tous les 3 jours à 06:35 — 5min
    _zones[1].mode         = SCHEDULE_MODE_INTERVAL;
    _zones[1].intervalDays = 3;
    _zones[1].intervalSlots.slots[0] = {6, 35, 5, true};

    Serial.println("[Schedule] Initialisé");
}

int ScheduleManager::weekdayToIdx(int weekday) {
    // tm_wday : 0=dimanche ... 6=samedi
    // on veut : 0=lundi ... 6=dimanche
    return (weekday == 0) ? 6 : weekday - 1;
}

TimeSlot ScheduleManager::getActiveSlots(uint8_t zone, uint8_t dayIdx,
                                          uint8_t slotIdx) {
    if (_zones[zone].mode == SCHEDULE_MODE_DAYS) {
        return _zones[zone].daySlots[dayIdx].slots[slotIdx];
    } else {
        return _zones[zone].intervalSlots.slots[slotIdx];
    }
}

void ScheduleManager::update(int hour, int minute, int weekday,
                              uint32_t epochDay, float rainMm) {
    if (hour < 0 || minute < 0) return;

    uint32_t now        = millis();
    uint32_t currentMin = (uint32_t)hour * 60 + minute;
    int      dayIdx     = weekdayToIdx(weekday);

    for (uint8_t z = 0; z < NB_ZONES; z++) {
        ZoneSchedule& zone = _zones[z];

        // ── Vérifier fin de slot actif ──
        if (_active[z].running && _active[z].startTime > 0) {
            TimeSlot slot = getActiveSlots(z, _active[z].day, _active[z].slotIdx);
            uint32_t elapsed = (now - _active[z].startTime) / 1000;
            if (elapsed >= (uint32_t)slot.duration * 60) {
                deactivateZone(z);
            }
            continue;  // une zone ne peut avoir qu'un slot actif à la fois
        }

        // ── Vérifier si on doit arroser ──
        if (!shouldWater(z, weekday, epochDay, rainMm)) continue;

        // ── Chercher un slot à déclencher ──
        DaySchedule* ds = (zone.mode == SCHEDULE_MODE_DAYS)
            ? &zone.daySlots[dayIdx]
            : &zone.intervalSlots;

        for (uint8_t s = 0; s < MAX_SLOTS; s++) {
            TimeSlot& slot = ds->slots[s];
            if (!slot.enabled) continue;

            uint32_t slotMin = (uint32_t)slot.hour * 60 + slot.minute;
            if (currentMin == slotMin && currentMin != _lastUpdateMinute) {
                activateZone(z, dayIdx, s);
                if (zone.mode == SCHEDULE_MODE_INTERVAL) {
                    zone.lastWateredDay = epochDay;
                }
                zone.forceToday = false;
                break;
            }
        }
    }
    _lastUpdateMinute = currentMin;
}

bool ScheduleManager::shouldWater(uint8_t zone, int weekday,
                                   uint32_t epochDay, float rainMm) {
    ZoneSchedule& z   = _zones[zone];
    int           idx = weekdayToIdx(weekday);

    // ── Vérifier le jour ──
    bool dayOk = false;
    if (z.mode == SCHEDULE_MODE_DAYS) {
        // Au moins un slot actif ce jour ?
        for (uint8_t s = 0; s < MAX_SLOTS; s++) {
            if (z.daySlots[idx].slots[s].enabled) { dayOk = true; break; }
        }
    } else {
        if (z.lastWateredDay == 0) {
            dayOk = true;
        } else {
            dayOk = (epochDay - z.lastWateredDay >= z.intervalDays);
        }
    }

    if (!dayOk) {
        _lastReason[zone] = "Jour non prévu";
        return false;
    }

    // ── Vérifier météo ──
    if (!z.forceToday && rainMm >= z.rain.thresholdMm) {
        _lastReason[zone] = String("Pluie : ") + rainMm + "mm";
        Serial.printf("[Schedule] Zone %d — annulé pluie %.1fmm\n",
            zone + 1, rainMm);
        return false;
    }

    _lastReason[zone] = z.forceToday ? "Forcé" : "Planifié";
    return true;
}

void ScheduleManager::activateZone(uint8_t zone, uint8_t day, uint8_t slotIdx) {
    _active[zone].running   = true;
    _active[zone].startTime = millis();
    _active[zone].day       = day;
    _active[zone].slotIdx   = slotIdx;
    Serial.printf("[Schedule] Zone %d — slot %d démarré (%s)\n",
        zone + 1, slotIdx + 1, _lastReason[zone].c_str());
    if (_relayCallback) _relayCallback(zone, true);
}

void ScheduleManager::deactivateZone(uint8_t zone) {
    _active[zone].running   = false;
    _active[zone].startTime = 0;
    Serial.printf("[Schedule] Zone %d — arrêt\n", zone + 1);
    if (_relayCallback) _relayCallback(zone, false);
}

// ── Getters ────────────────────────────────────
ZoneSchedule ScheduleManager::getZoneSchedule(uint8_t zone) {
    if (zone >= NB_ZONES) return ZoneSchedule();
    return _zones[zone];
}

bool ScheduleManager::isZoneActive(uint8_t zone) {
    if (zone >= NB_ZONES) return false;
    return _active[zone].running;
}

String ScheduleManager::getLastReason(uint8_t zone) {
    if (zone >= NB_ZONES) return "";
    return _lastReason[zone];
}

// ── Setters ────────────────────────────────────
void ScheduleManager::setMode(uint8_t zone, uint8_t mode) {
    if (zone >= NB_ZONES) return;
    _zones[zone].mode = mode;
    Serial.printf("[Schedule] Zone %d — mode %s\n",
        zone + 1, mode == SCHEDULE_MODE_DAYS ? "jours" : "intervalle");
}

void ScheduleManager::setIntervalDays(uint8_t zone, uint8_t days) {
    if (zone >= NB_ZONES) return;
    _zones[zone].intervalDays = days;
    Serial.printf("[Schedule] Zone %d — intervalle %dj\n", zone + 1, days);
}

void ScheduleManager::setDaySlot(uint8_t zone, uint8_t day, uint8_t slotIdx,
                                  uint8_t hour, uint8_t minute,
                                  uint16_t duration, bool enabled) {
    if (zone >= NB_ZONES || day >= NB_DAYS || slotIdx >= MAX_SLOTS) return;
    _zones[zone].daySlots[day].slots[slotIdx] = {hour, minute, duration, enabled};
    Serial.printf("[Schedule] Zone %d — jour %d slot %d : %02d:%02d %dmin %s\n",
        zone+1, day, slotIdx+1, hour, minute, duration,
        enabled ? "ON" : "OFF");
}

void ScheduleManager::setIntervalSlot(uint8_t zone, uint8_t slotIdx,
                                       uint8_t hour, uint8_t minute,
                                       uint16_t duration, bool enabled) {
    if (zone >= NB_ZONES || slotIdx >= MAX_SLOTS) return;
    _zones[zone].intervalSlots.slots[slotIdx] = {hour, minute, duration, enabled};
    Serial.printf("[Schedule] Zone %d — interval slot %d : %02d:%02d %dmin %s\n",
        zone+1, slotIdx+1, hour, minute, duration, enabled ? "ON" : "OFF");
}

void ScheduleManager::setRainConfig(uint8_t zone, float thresholdMm,
                                     uint8_t forecastHours) {
    if (zone >= NB_ZONES) return;
    _zones[zone].rain = {thresholdMm, forecastHours};
    Serial.printf("[Schedule] Zone %d — pluie seuil %.1fmm/%dh\n",
        zone+1, thresholdMm, forecastHours);
}

void ScheduleManager::setForceToday(uint8_t zone, bool force) {
    if (zone >= NB_ZONES) return;
    _zones[zone].forceToday = force;
    Serial.printf("[Schedule] Zone %d — forcer : %s\n",
        zone+1, force ? "OUI" : "NON");
}

void ScheduleManager::setRelayCallback(RelayCallback cb) {
    _relayCallback = cb;
}