#include "ScheduleManager.h"
#include "EventBus.h"

void ScheduleManager::begin() {
    for (uint8_t z = 0; z < MAX_ZONES; z++) {
        _zones[z]      = ZoneSchedule{};
        _active[z]     = ActiveSlot{};
        _lastReason[z] = "En attente";
    }
    _lastCheckedMinute = 0xFFFFFFFF;
    _recoveryPending   = true;
    Serial.printf("[Schedule] Initialisé — %d zones max, %d actives\n",
                  MAX_ZONES, _nbZones);
}

void ScheduleManager::setNbZones(uint8_t nb) {
    _nbZones = constrain(nb, 1, MAX_ZONES);
    Serial.printf("[Schedule] Zones actives : %d\n", _nbZones);
}

void ScheduleManager::update(int hour, int minute, int weekday,
                             uint32_t epochDay, float rainMm) {
    if (hour < 0 || minute < 0 || weekday < 0 || weekday > 6 || epochDay == 0) return;

    const uint32_t currentMin = (uint32_t)hour * 60UL + (uint32_t)minute;

    for (uint8_t z = 0; z < _nbZones; z++) {
        checkSlotEnd(z);
    }

    // Une seule fois après le boot, dès que l'heure est valide :
    // rechercher un créneau déjà commencé et encore actif.
    if (_recoveryPending) {
        _recoveryPending = false;
        recoverInterruptedSlots(hour, minute, weekday, epochDay, rainMm);
    }

    if (currentMin == _lastCheckedMinute) return;
    _lastCheckedMinute = currentMin;

    const int dayIdx = weekdayToIdx(weekday);

    for (uint8_t z = 0; z < _nbZones; z++) {
        if (_active[z].running) continue;

        DaySchedule& ds = (_zones[z].mode == SCHEDULE_MODE_DAYS)
                          ? _zones[z].daySlots[dayIdx]
                          : _zones[z].intervalSlots;

        for (uint8_t s = 0; s < MAX_SLOTS; s++) {
            const TimeSlot& sl = ds.slots[s];
            if (!sl.enabled) continue;

            const uint32_t slotMin = (uint32_t)sl.hour * 60UL + sl.minute;
            if (slotMin != currentMin) continue;

            if (shouldWater(z, weekday, epochDay, rainMm)) {
                activateZone(z, sl.duration, false);
                _zones[z].lastWateredDay = epochDay;
            }
            break;
        }
    }
}

ZoneSchedule ScheduleManager::getZoneSchedule(uint8_t zone) const {
    if (zone >= MAX_ZONES) return ZoneSchedule{};
    return _zones[zone];
}

bool ScheduleManager::isZoneActive(uint8_t zone) const {
    if (zone >= MAX_ZONES) return false;
    return _active[zone].running;
}

String ScheduleManager::getLastReason(uint8_t zone) const {
    if (zone >= MAX_ZONES) return "";
    return _lastReason[zone];
}

uint32_t ScheduleManager::getElapsedMs(uint8_t zone) const {
    if (zone >= MAX_ZONES || !_active[zone].running) return 0;
    return millis() - _active[zone].startMs;
}

uint32_t ScheduleManager::getRemainingMs(uint8_t zone) const {
    if (zone >= MAX_ZONES || !_active[zone].running) return 0;
    const uint32_t elapsed = millis() - _active[zone].startMs;
    return (elapsed < _active[zone].durationMs)
           ? _active[zone].durationMs - elapsed
           : 0;
}

void ScheduleManager::setMode(uint8_t zone, uint8_t mode) {
    if (zone >= MAX_ZONES) return;
    _zones[zone].mode = mode;
}

void ScheduleManager::setIntervalDays(uint8_t zone, uint8_t days) {
    if (zone >= MAX_ZONES) return;
    _zones[zone].intervalDays = days;
}

void ScheduleManager::setDaySlot(uint8_t zone, uint8_t day, uint8_t slotIdx,
                                 uint8_t h, uint8_t m,
                                 uint16_t dur, bool enabled) {
    if (zone >= MAX_ZONES || day >= NB_DAYS || slotIdx >= MAX_SLOTS) return;
    _zones[zone].daySlots[day].slots[slotIdx] = TimeSlot(h, m, dur, enabled);
}

void ScheduleManager::setIntervalSlot(uint8_t zone, uint8_t slotIdx,
                                      uint8_t h, uint8_t m,
                                      uint16_t dur, bool enabled) {
    if (zone >= MAX_ZONES || slotIdx >= MAX_SLOTS) return;
    _zones[zone].intervalSlots.slots[slotIdx] = TimeSlot(h, m, dur, enabled);
}

void ScheduleManager::setRainConfig(uint8_t zone,
                                    float threshMm, uint8_t hours) {
    if (zone >= MAX_ZONES) return;
    _zones[zone].rain = RainConfig(threshMm, hours);
}

void ScheduleManager::setManualDuration(uint16_t minutes) {
    if (minutes == 0 || minutes > 120) return;
    _manualDurationMin = minutes;
}

void ScheduleManager::startManualWatering(uint8_t zone) {
    if (zone >= MAX_ZONES) return;
    if (_active[zone].running) deactivateZone(zone);
    _lastReason[zone] = String("Manuel ") + _manualDurationMin + "min";
    activateZone(zone, _manualDurationMin, true);
    Serial.printf("[Schedule] Zone %d — manuel %dmin\n", zone+1, _manualDurationMin);
}

void ScheduleManager::stopManualWatering(uint8_t zone) {
    if (zone >= MAX_ZONES || !_active[zone].running) return;
    deactivateZone(zone);
    Serial.printf("[Schedule] Zone %d — manuel arrêté\n", zone+1);
}

void ScheduleManager::setRelayCallback(RelayCallback cb) {
    _relayCallback = cb;
}

int ScheduleManager::weekdayToIdx(int tmWday) {
    return (tmWday == 0) ? 6 : tmWday - 1;
}

bool ScheduleManager::shouldWater(uint8_t zone, int weekday,
                                  uint32_t epochDay, float rainMm) {
    ZoneSchedule& z   = _zones[zone];
    const int     idx = weekdayToIdx(weekday);

    bool dayOk = false;
    if (z.mode == SCHEDULE_MODE_DAYS) {
        for (uint8_t s = 0; s < MAX_SLOTS && !dayOk; s++) {
            dayOk = z.daySlots[idx].slots[s].enabled;
        }
    } else {
        dayOk = (z.lastWateredDay == 0)
             || ((epochDay - z.lastWateredDay) >= z.intervalDays);
    }

    if (!dayOk) {
        _lastReason[zone] = "Jour non prevu";
        return false;
    }

    if (rainMm >= z.rain.thresholdMm) {
        _lastReason[zone] = String("Pluie ") + String(rainMm, 1) + "mm";
        return false;
    }

    _lastReason[zone] = "Planifie";
    return true;
}

bool ScheduleManager::shouldRecover(uint8_t zone, int weekday,
                                    uint32_t scheduledEpochDay, float rainMm) {
    ZoneSchedule& z = _zones[zone];

    if (z.mode == SCHEDULE_MODE_DAYS) {
        const int idx = weekdayToIdx(weekday);
        bool dayOk = false;
        for (uint8_t s = 0; s < MAX_SLOTS && !dayOk; s++) {
            dayOk = z.daySlots[idx].slots[s].enabled;
        }
        if (!dayOk) return false;
    } else {
        // Si lastWateredDay vaut le jour du créneau, celui-ci avait déjà commencé
        // avant le reboot : sa reprise reste donc autorisée.
        const bool intervalOk = (z.lastWateredDay == scheduledEpochDay)
                             || (z.lastWateredDay == 0)
                             || ((scheduledEpochDay - z.lastWateredDay) >= z.intervalDays);
        if (!intervalOk) return false;
    }

    if (rainMm >= z.rain.thresholdMm) {
        _lastReason[zone] = String("Pluie ") + String(rainMm, 1) + "mm";
        return false;
    }

    return true;
}

void ScheduleManager::recoverInterruptedSlots(int hour, int minute, int weekday,
                                              uint32_t epochDay, float rainMm) {
    const uint32_t nowAbsoluteMin = epochDay * 1440UL
                                  + (uint32_t)hour * 60UL
                                  + (uint32_t)minute;

    Serial.println("[Schedule] Recherche de créneaux interrompus après reboot");

    for (uint8_t z = 0; z < _nbZones; z++) {
        if (_active[z].running) continue;

        if (_zones[z].mode == SCHEDULE_MODE_DAYS) {
            const int todayIdx = weekdayToIdx(weekday);
            recoverFromSchedule(z, _zones[z].daySlots[todayIdx],
                                weekday, epochDay, nowAbsoluteMin, rainMm);

            // Un créneau commencé la veille peut encore être actif après minuit.
            if (!_active[z].running && epochDay > 0) {
                const int previousWeekday = (weekday + 6) % 7;
                const int previousIdx = weekdayToIdx(previousWeekday);
                recoverFromSchedule(z, _zones[z].daySlots[previousIdx],
                                    previousWeekday, epochDay - 1,
                                    nowAbsoluteMin, rainMm);
            }
        } else {
            recoverFromSchedule(z, _zones[z].intervalSlots,
                                weekday, epochDay, nowAbsoluteMin, rainMm);

            if (!_active[z].running && epochDay > 0) {
                const int previousWeekday = (weekday + 6) % 7;
                recoverFromSchedule(z, _zones[z].intervalSlots,
                                    previousWeekday, epochDay - 1,
                                    nowAbsoluteMin, rainMm);
            }
        }
    }
}

void ScheduleManager::recoverFromSchedule(uint8_t zone,
                                          const DaySchedule& schedule,
                                          int scheduledWeekday,
                                          uint32_t scheduledEpochDay,
                                          uint32_t nowAbsoluteMin,
                                          float rainMm) {
    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
        const TimeSlot& sl = schedule.slots[s];
        if (!sl.enabled || sl.duration == 0) continue;

        const uint32_t startAbsoluteMin = scheduledEpochDay * 1440UL
                                        + (uint32_t)sl.hour * 60UL
                                        + (uint32_t)sl.minute;
        const uint32_t endAbsoluteMin = startAbsoluteMin + sl.duration;

        if (nowAbsoluteMin < startAbsoluteMin || nowAbsoluteMin >= endAbsoluteMin) {
            continue;
        }

        if (!shouldRecover(zone, scheduledWeekday, scheduledEpochDay, rainMm)) {
            continue;
        }

        const uint32_t remainingMin = endAbsoluteMin - nowAbsoluteMin;
        const uint32_t remainingMs  = remainingMin * 60000UL;

        _lastReason[zone] = "Reprise apres reboot";
        _zones[zone].lastWateredDay = scheduledEpochDay;
        activateZoneRemaining(zone, remainingMs);

        Serial.printf("[Schedule] Zone %d — REPRISE slot %02d:%02d, reste %lumin\n",
                      zone + 1, sl.hour, sl.minute,
                      (unsigned long)remainingMin);
        return;
    }
}

void ScheduleManager::checkSlotEnd(uint8_t zone) {
    if (!_active[zone].running) return;
    if (getRemainingMs(zone) == 0) {
        Serial.printf("[Schedule] Zone %d — fin %s\n",
                      zone+1, _active[zone].isManual ? "manuel" : "planifie");
        deactivateZone(zone);
    }
}

void ScheduleManager::activateZone(uint8_t zone,
                                   uint16_t durationMin, bool manual) {
    _active[zone].running    = true;
    _active[zone].startMs    = millis();
    _active[zone].durationMs = (uint32_t)durationMin * 60000UL;
    _active[zone].isManual   = manual;
    EventBus::displayDirty   = true;
    Serial.printf("[Schedule] Zone %d — START %dmin (%s) reason=%s\n",
                  zone+1, durationMin,
                  manual ? "manuel" : "planifie",
                  _lastReason[zone].c_str());
    if (_relayCallback) _relayCallback(zone, true);
}

void ScheduleManager::activateZoneRemaining(uint8_t zone, uint32_t remainingMs) {
    if (remainingMs == 0) return;

    _active[zone].running    = true;
    _active[zone].startMs    = millis();
    _active[zone].durationMs = remainingMs;
    _active[zone].isManual   = false;
    EventBus::displayDirty   = true;

    if (_relayCallback) _relayCallback(zone, true);
}

void ScheduleManager::deactivateZone(uint8_t zone) {
    _active[zone].running    = false;
    _active[zone].startMs    = 0;
    _active[zone].durationMs = 0;
    _active[zone].isManual   = false;
    EventBus::displayDirty   = true;
    Serial.printf("[Schedule] Zone %d — STOP\n", zone+1);
    if (_relayCallback) _relayCallback(zone, false);
}
