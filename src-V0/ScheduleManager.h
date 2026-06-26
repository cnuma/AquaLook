#pragma once
#include <Arduino.h>
#include "config.h"

// ── Slot horaire ───────────────────────────────
struct TimeSlot {
    uint8_t  hour;      // heure (0-23)
    uint8_t  minute;    // minute (0-59)
    uint16_t duration;  // durée en minutes
    bool     enabled;   // actif ou non
};

// ── Planning d'une journée ─────────────────────
struct DaySchedule {
    TimeSlot slots[MAX_SLOTS];
};

// ── Configuration météo ────────────────────────
struct RainConfig {
    float   thresholdMm;   // seuil pluie (mm)
    uint8_t forecastHours; // fenêtre prévision (h)
};

// ── Configuration complète d'une zone ──────────
struct ZoneSchedule {
    uint8_t      mode;                  // SCHEDULE_MODE_DAYS ou INTERVAL
    DaySchedule  daySlots[NB_DAYS];     // slots par jour (mode jours fixes)
    DaySchedule  intervalSlots;         // slots globaux (mode intervalle)
    uint8_t      intervalDays;          // tous les N jours
    uint32_t     lastWateredDay;        // epoch/86400 du dernier arrosage
    RainConfig   rain;
    bool         forceToday;
};

// ── Slot actif en cours ────────────────────────
struct ActiveSlot {
    bool     running   = false;
    uint32_t startTime = 0;
    uint8_t  day       = 0;
    uint8_t  slotIdx   = 0;
};

class ScheduleManager {
public:
    void begin();
    void update(int hour, int minute, int weekday,
                uint32_t epochDay, float rainMm);

    // Getters
    ZoneSchedule getZoneSchedule(uint8_t zone);
    bool         isZoneActive(uint8_t zone);
    String       getLastReason(uint8_t zone);

    // Setters mode
    void setMode(uint8_t zone, uint8_t mode);
    void setIntervalDays(uint8_t zone, uint8_t days);

    // Setters slots
    void setDaySlot(uint8_t zone, uint8_t day, uint8_t slotIdx,
                    uint8_t hour, uint8_t minute,
                    uint16_t duration, bool enabled);
    void setIntervalSlot(uint8_t zone, uint8_t slotIdx,
                         uint8_t hour, uint8_t minute,
                         uint16_t duration, bool enabled);

    // Météo
    void setRainConfig(uint8_t zone, float thresholdMm, uint8_t forecastHours);
    void setForceToday(uint8_t zone, bool force);

    // Callback relais
    using RelayCallback = void(*)(uint8_t zone, bool state);
    void setRelayCallback(RelayCallback cb);

private:
    ZoneSchedule  _zones[NB_ZONES];
    ActiveSlot    _active[NB_ZONES];
    String        _lastReason[NB_ZONES];
    uint32_t      _lastUpdateMinute = 0xFFFFFFFF;
    RelayCallback _relayCallback    = nullptr;

    bool     shouldWater(uint8_t zone, int weekday,
                         uint32_t epochDay, float rainMm);
    void     activateZone(uint8_t zone, uint8_t day, uint8_t slotIdx);
    void     deactivateZone(uint8_t zone);
    int      weekdayToIdx(int weekday);
    TimeSlot getActiveSlots(uint8_t zone, uint8_t dayIdx,
                            uint8_t slotIdx);
};