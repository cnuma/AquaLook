#include "ScheduleManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "NotificationManager.h"

// ═══════════════════════════════════════════════════════════════
//  begin() — initialisation RAM uniquement
//  Le planning réel est chargé par ConfigManager::applyToSchedule()
// ═══════════════════════════════════════════════════════════════
void ScheduleManager::begin() {
    // Initialiser toutes les zones jusqu'à MAX_ZONES
    for (uint8_t z = 0; z < MAX_ZONES; z++) {
        _zones[z]      = ZoneSchedule{};
        _active[z]     = ActiveSlot{};
        _lastReason[z] = "En attente";
    }
    _lastCheckedMinute = 0xFFFFFFFF;
    Serial.printf("[Schedule] Initialisé — %d zones max, %d actives\n",
                  MAX_ZONES, _nbZones);
}

// ─────────────────────────────────────────────────────────────
//  setNbZones — propager le nombre de zones actives depuis Config
// ─────────────────────────────────────────────────────────────
void ScheduleManager::setNbZones(uint8_t nb) {
    _nbZones = constrain(nb, 1, MAX_ZONES);
    Serial.printf("[Schedule] Zones actives : %d\n", _nbZones);
}

// ═══════════════════════════════════════════════════════════════
//  update() — boucle principale non bloquante
//  Appelée dans loop() si NTP synced
// ═══════════════════════════════════════════════════════════════
void ScheduleManager::update(int hour, int minute, int weekday,
                              uint32_t epochDay, float rainMm) {
    // Avant toute decision : une suspension expiree ne doit pas peser sur
    // ce que le planificateur va decider ensuite.
    expirePauses();

    if (hour < 0 || minute < 0) return;

    const uint32_t now        = millis();
    const uint32_t currentMin = (uint32_t)hour * 60 + minute;

    // Vérifier fin de slots actifs
    for (uint8_t z = 0; z < _nbZones; z++) {
        checkSlotEnd(z);
    }

    // Nouvelle minute — vérifier déclenchements
    if (currentMin == _lastCheckedMinute) return;
    _lastCheckedMinute = currentMin;

    const int dayIdx = weekdayToIdx(weekday);

    for (uint8_t z = 0; z < _nbZones; z++) {
        if (_active[z].running) continue;  // déjà actif

        // Itérer sur tous les slots de la zone
        DaySchedule& ds = (_zones[z].mode == SCHEDULE_MODE_DAYS)
                          ? _zones[z].daySlots[dayIdx]
                          : _zones[z].intervalSlots;

        for (uint8_t s = 0; s < MAX_SLOTS; s++) {
            const TimeSlot& sl = ds.slots[s];
            if (!sl.enabled) continue;

            const uint32_t slotMin = (uint32_t)sl.hour * 60 + sl.minute;
            if (slotMin != currentMin) continue;

            // Heure atteinte — vérifier conditions
            if (shouldWater(z, weekday, epochDay, rainMm)) {
                activateZone(z, sl.duration, false);
            }
            break;  // un seul slot par minute par zone
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  Getters
// ═══════════════════════════════════════════════════════════════
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

// ═══════════════════════════════════════════════════════════════
//  Setters planning
// ═══════════════════════════════════════════════════════════════
void ScheduleManager::setMode(uint8_t zone, uint8_t mode) {
    if (zone >= MAX_ZONES) return;
    _zones[zone].mode = mode;
}

void ScheduleManager::setIntervalDays(uint8_t zone, uint8_t days) {
    if (zone >= MAX_ZONES) return;
    _zones[zone].intervalDays = constrain(days, (uint8_t)1, (uint8_t)30);
}

void ScheduleManager::setIntervalAnchorDay(uint8_t zone, uint32_t epochDay) {
    if (zone >= MAX_ZONES) return;
    _zones[zone].intervalAnchorDay = epochDay;
}

void ScheduleManager::clearIntervalProgramming(uint8_t zone) {
    if (zone >= MAX_ZONES) return;
    _zones[zone].mode = SCHEDULE_MODE_DAYS;
    _zones[zone].intervalDays = 2;
    _zones[zone].intervalAnchorDay = 0;
    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
        _zones[zone].intervalSlots.slots[s] = TimeSlot();
    }
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

// ═══════════════════════════════════════════════════════════════
//  Arrosage manuel
// ═══════════════════════════════════════════════════════════════
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

// ═══════════════════════════════════════════════════════════════
//  Callback relais
// ═══════════════════════════════════════════════════════════════
void ScheduleManager::setRelayCallback(RelayCallback cb) {
    _relayCallback = cb;
}

// ═══════════════════════════════════════════════════════════════
//  Privé
// ═══════════════════════════════════════════════════════════════
int ScheduleManager::weekdayToIdx(int tmWday) {
    // tm_wday : 0=dim..6=sam → 0=lun..6=dim
    return (tmWday == 0) ? 6 : tmWday - 1;
}

bool ScheduleManager::shouldWater(uint8_t zone, int weekday,
                                   uint32_t epochDay, float rainMm) {
    ZoneSchedule& z   = _zones[zone];
    const int     idx = weekdayToIdx(weekday);

    // ── Jour prévu ? ──────────────────────────
    bool dayOk = false;
    if (z.mode == SCHEDULE_MODE_DAYS) {
        // Vérifier si au moins un slot est activé ce jour
        for (uint8_t s = 0; s < MAX_SLOTS && !dayOk; s++)
            dayOk = z.daySlots[idx].slots[s].enabled;
    } else {
        const uint32_t anchor = z.intervalAnchorDay;
        const uint8_t interval = max((uint8_t)1, z.intervalDays);
        dayOk = (anchor > 0) &&
                (epochDay >= anchor) &&
                (((epochDay - anchor) % interval) == 0);
    }

    if (!dayOk) {
        _lastReason[zone] = "Jour non prevu";
        return false;
    }

    // ── Pluie prévue ? ────────────────────────
    if (rainMm >= z.rain.thresholdMm) {
        _lastReason[zone] = String("Pluie ") + String(rainMm, 1) + "mm";
        return false;
    }

    _lastReason[zone] = "Planifie";
    return true;
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
    activateZoneMs(zone, (uint32_t)durationMin * 60000UL, manual);
}

// Meme chose, mais en millisecondes : une reprise ne repart pas d'un nombre
// rond de minutes, elle repart du reliquat.
void ScheduleManager::activateZoneMs(uint8_t zone,
                                     uint32_t durationMs, bool manual) {
    const uint16_t durationMin = (uint16_t)(durationMs / 60000UL);
    _active[zone].running    = true;
    _active[zone].startMs    = millis();
    _active[zone].durationMs = durationMs;
    _active[zone].isManual   = manual;
    _active[zone].paused     = false;
    _active[zone].remainingMs = 0;
    EventBus::displayDirty   = true;
    Serial.printf("[Schedule] Zone %d — START %dmin (%s) reason=%s\n",
                  zone+1, durationMin,
                  manual ? "manuel" : "planifie",
                  _lastReason[zone].c_str());
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

// ═══════════════════════════════════════════════════════════════
//  Suspension et reprise
// ═══════════════════════════════════════════════════════════════

bool ScheduleManager::startZoneForSeconds(uint8_t zone, uint32_t seconds) {
    if (zone >= _nbZones || zone >= MAX_ZONES || seconds == 0U) return false;
    // Une reprise ou un demarrage decide par un script est traite comme
    // MANUEL : il ne vient pas du planning, et le confondre avec un
    // creneau planifie fausserait le journal comme l affichage.
    activateZoneMs(zone, seconds * 1000UL, true);
    return true;
}

bool ScheduleManager::pauseZone(uint8_t zone) {
    if (zone >= MAX_ZONES) return false;
    ActiveSlot& slot = _active[zone];
    if (!slot.running) return false;

    const uint32_t elapsed = millis() - slot.startMs;
    // Un reliquat nul n'a pas de sens : l'arrosage etait fini de toute facon.
    // Suspendre ici laisserait une zone en attente d'une reprise sans objet.
    if (elapsed >= slot.durationMs) {
        deactivateZone(zone);
        return false;
    }

    slot.remainingMs = slot.durationMs - elapsed;
    slot.running     = false;
    slot.paused      = true;
    slot.pausedAtMs  = millis();
    EventBus::displayDirty = true;

    EventLog::log(LOG_INFO, "Schedule: zone %u suspendue, reste %lus",
                  (unsigned)(zone + 1U),
                  (unsigned long)(slot.remainingMs / 1000UL));
    if (_relayCallback) _relayCallback(zone, false);
    if (_pauseObserver) _pauseObserver();
    return true;
}

bool ScheduleManager::restorePause(uint8_t zone, uint32_t remainingMs) {
    if (zone >= MAX_ZONES || remainingMs == 0U) return false;
    ActiveSlot& slot = _active[zone];
    slot.running     = false;
    slot.paused      = true;
    slot.remainingMs = remainingMs;
    slot.pausedAtMs  = millis();
    slot.isManual    = true;
    EventBus::displayDirty = true;
    return true;
}

bool ScheduleManager::resumeZone(uint8_t zone) {
    if (zone >= MAX_ZONES) return false;
    ActiveSlot& slot = _active[zone];
    if (!slot.paused || slot.remainingMs == 0U) return false;

    const uint32_t remaining = slot.remainingMs;
    const bool manual = slot.isManual;
    EventLog::log(LOG_INFO, "Schedule: zone %u reprise pour %lus",
                  (unsigned)(zone + 1U), (unsigned long)(remaining / 1000UL));
    activateZoneMs(zone, remaining, manual);
    if (_pauseObserver) _pauseObserver();
    return true;
}

bool ScheduleManager::isZonePaused(uint8_t zone) const {
    return zone < MAX_ZONES && _active[zone].paused;
}

uint32_t ScheduleManager::getPausedRemainingMs(uint8_t zone) const {
    return (zone < MAX_ZONES && _active[zone].paused) ? _active[zone].remainingMs : 0U;
}

// Une suspension qui s'eternise est abandonnee. Sans cela, une cuve qui ne
// se remplit jamais tiendrait la zone indefiniment -- et l'arrosage
// reprendrait au pire moment, des heures plus tard, sans que personne ne
// l'ait demande.
void ScheduleManager::expirePauses() {
    const uint32_t now = millis();
    for (uint8_t z = 0U; z < _nbZones && z < MAX_ZONES; ++z) {
        ActiveSlot& slot = _active[z];
        if (!slot.paused) continue;
        if ((now - slot.pausedAtMs) < PAUSE_MAX_MS) continue;

        EventLog::log(LOG_WARN,
                      "Schedule: zone %u abandonnee, suspendue plus de %lu min",
                      (unsigned)(z + 1U),
                      (unsigned long)(PAUSE_MAX_MS / 60000UL));
        slot.paused = false;
        slot.remainingMs = 0U;
        slot.running = false;
        NotificationManager::enqueueZoneEvent(z, false);
        EventBus::displayDirty = true;
        if (_pauseObserver) _pauseObserver();
    }
}
