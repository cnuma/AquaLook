#pragma once
#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════
//  ScheduleManager — planificateur multi-zones non bloquant
//
//  Capacité : MAX_ZONES zones en RAM
//  Zones actives : _nbZones (chargé depuis ConfigManager au begin())
//  Invariant I6 : activation relais via callback onRelayRequest
// ═══════════════════════════════════════════════════════════════

// ── Slot horaire ──────────────────────────────
struct TimeSlot {
    uint8_t  hour     = 6;
    uint8_t  minute   = 0;
    uint16_t duration = 5;    // minutes
    bool     enabled  = false;
    TimeSlot() : hour(6), minute(0), duration(5), enabled(false) {}
    TimeSlot(uint8_t h, uint8_t m, uint16_t d, bool e)
        : hour(h), minute(m), duration(d), enabled(e) {}
};

// ── Planning d'une journée ─────────────────────
struct DaySchedule {
    TimeSlot slots[MAX_SLOTS];
    DaySchedule() {}
};

// ── Config météo ──────────────────────────────
struct RainConfig {
    float   thresholdMm   = 2.0f;
    uint8_t forecastHours = 24;
    RainConfig() : thresholdMm(2.0f), forecastHours(24) {}
    RainConfig(float t, uint8_t h) : thresholdMm(t), forecastHours(h) {}
};

// ── Planning complet d'une zone ───────────────
struct ZoneSchedule {
    uint8_t     mode         = 0;         // SCHEDULE_MODE_DAYS / INTERVAL
    uint8_t     intervalDays = 2;
    uint32_t    lastWateredDay = 0;        // epoch/86400 dernier arrosage
    RainConfig  rain;
    DaySchedule daySlots[NB_DAYS];         // slots par jour
    DaySchedule intervalSlots;            // slots mode intervalle
    ZoneSchedule() : mode(0), intervalDays(2), lastWateredDay(0) {}
};

// ── Slot actif ────────────────────────────────
struct ActiveSlot {
    bool     running   = false;
    uint32_t startMs   = 0;     // millis() de début
    uint32_t durationMs = 0;    // durée totale en ms
    bool     isManual  = false;
    ActiveSlot() : running(false), startMs(0), durationMs(0), isManual(false) {}
};

// ═══════════════════════════════════════════════════════════════
class ScheduleManager {
public:
    // ── Cycle de vie ──────────────────────────
    void begin();
    void update(int hour, int minute, int weekday,
                uint32_t epochDay, float rainMm);

    // ── Nb zones runtime ──────────────────────
    void    setNbZones(uint8_t nb);   // appelé par ConfigManager::applyToSchedule
    uint8_t getNbZones() const { return _nbZones; }

    // ── Getters état ──────────────────────────
    ZoneSchedule getZoneSchedule(uint8_t zone) const;
    bool         isZoneActive(uint8_t zone)    const;
    String       getLastReason(uint8_t zone)   const;
    uint32_t     getElapsedMs(uint8_t zone)    const;
    uint32_t     getRemainingMs(uint8_t zone)  const;
    uint16_t     getManualDurationMin()        const { return _manualDurationMin; }

    // ── Setters planning ──────────────────────
    void setMode(uint8_t zone, uint8_t mode);
    void setIntervalDays(uint8_t zone, uint8_t days);
    void setDaySlot(uint8_t zone, uint8_t day, uint8_t slotIdx,
                    uint8_t h, uint8_t m, uint16_t dur, bool enabled);
    void setIntervalSlot(uint8_t zone, uint8_t slotIdx,
                         uint8_t h, uint8_t m, uint16_t dur, bool enabled);
    void setRainConfig(uint8_t zone, float threshMm, uint8_t hours);
    void setManualDuration(uint16_t minutes);

    // ── Arrosage manuel ───────────────────────
    void startManualWatering(uint8_t zone);
    void stopManualWatering(uint8_t zone);

    // ── Callback relais ───────────────────────
    // Invariant I6 : câblé dans main.cpp uniquement
    using RelayCallback = void(*)(uint8_t zone, bool state);
    void setRelayCallback(RelayCallback cb);

private:
    // Tableaux dimensionnés à MAX_ZONES — zones actives = _nbZones
    ZoneSchedule  _zones[MAX_ZONES];
    ActiveSlot    _active[MAX_ZONES];
    String        _lastReason[MAX_ZONES];

    uint8_t       _nbZones            = NB_ZONES;  // runtime
    uint16_t      _manualDurationMin  = 10;
    uint32_t      _lastCheckedMinute  = 0xFFFFFFFF;
    RelayCallback _relayCallback      = nullptr;

    static int weekdayToIdx(int tmWday);  // tm_wday → 0=lun..6=dim
    bool       shouldWater(uint8_t zone, int weekday,
                           uint32_t epochDay, float rainMm);
    void       activateZone(uint8_t zone, uint16_t durationMin, bool manual);
    void       deactivateZone(uint8_t zone);
    void       checkSlotEnd(uint8_t zone);
};
