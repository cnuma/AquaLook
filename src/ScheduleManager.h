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

struct TimeSlot {
    uint8_t  hour     = 6;
    uint8_t  minute   = 0;
    uint16_t duration = 5;
    bool     enabled  = false;
    TimeSlot() : hour(6), minute(0), duration(5), enabled(false) {}
    TimeSlot(uint8_t h, uint8_t m, uint16_t d, bool e)
        : hour(h), minute(m), duration(d), enabled(e) {}
};

struct DaySchedule {
    TimeSlot slots[MAX_SLOTS];
    DaySchedule() {}
};

struct RainConfig {
    float   thresholdMm   = 2.0f;
    uint8_t forecastHours = 24;
    RainConfig() : thresholdMm(2.0f), forecastHours(24) {}
    RainConfig(float t, uint8_t h) : thresholdMm(t), forecastHours(h) {}
};

struct ZoneSchedule {
    uint8_t     mode           = 0;
    uint8_t     intervalDays   = 2;
    uint32_t    lastWateredDay = 0;
    RainConfig  rain;
    DaySchedule daySlots[NB_DAYS];
    DaySchedule intervalSlots;
    ZoneSchedule() : mode(0), intervalDays(2), lastWateredDay(0) {}
};

struct ActiveSlot {
    bool     running    = false;
    uint32_t startMs    = 0;
    uint32_t durationMs = 0;
    bool     isManual   = false;
    ActiveSlot() : running(false), startMs(0), durationMs(0), isManual(false) {}
};

class ScheduleManager {
public:
    void begin();
    void update(int hour, int minute, int weekday,
                uint32_t epochDay, float rainMm);

    void    setNbZones(uint8_t nb);
    uint8_t getNbZones() const { return _nbZones; }

    ZoneSchedule getZoneSchedule(uint8_t zone) const;
    bool         isZoneActive(uint8_t zone) const;
    String       getLastReason(uint8_t zone) const;
    uint32_t     getElapsedMs(uint8_t zone) const;
    uint32_t     getRemainingMs(uint8_t zone) const;
    uint16_t     getManualDurationMin() const { return _manualDurationMin; }

    void setMode(uint8_t zone, uint8_t mode);
    void setIntervalDays(uint8_t zone, uint8_t days);
    void setDaySlot(uint8_t zone, uint8_t day, uint8_t slotIdx,
                    uint8_t h, uint8_t m, uint16_t dur, bool enabled);
    void setIntervalSlot(uint8_t zone, uint8_t slotIdx,
                         uint8_t h, uint8_t m, uint16_t dur, bool enabled);
    void setRainConfig(uint8_t zone, float threshMm, uint8_t hours);
    void setManualDuration(uint16_t minutes);

    void startManualWatering(uint8_t zone);
    void stopManualWatering(uint8_t zone);

    using RelayCallback = void(*)(uint8_t zone, bool state);
    void setRelayCallback(RelayCallback cb);

private:
    ZoneSchedule  _zones[MAX_ZONES];
    ActiveSlot    _active[MAX_ZONES];
    String        _lastReason[MAX_ZONES];

    uint8_t       _nbZones            = NB_ZONES;
    uint16_t      _manualDurationMin  = 10;
    uint32_t      _lastCheckedMinute  = 0xFFFFFFFF;
    bool          _recoveryPending    = true;
    RelayCallback _relayCallback      = nullptr;

    static int weekdayToIdx(int tmWday);
    bool shouldWater(uint8_t zone, int weekday,
                     uint32_t epochDay, float rainMm);
    bool shouldRecover(uint8_t zone, int weekday,
                       uint32_t scheduledEpochDay, float rainMm);
    void recoverInterruptedSlots(int hour, int minute, int weekday,
                                 uint32_t epochDay, float rainMm);
    void recoverFromSchedule(uint8_t zone, const DaySchedule& schedule,
                             int scheduledWeekday, uint32_t scheduledEpochDay,
                             uint32_t nowAbsoluteMin, float rainMm);
    void activateZone(uint8_t zone, uint16_t durationMin, bool manual);
    void activateZoneRemaining(uint8_t zone, uint32_t remainingMs);
    void deactivateZone(uint8_t zone);
    void checkSlotEnd(uint8_t zone);
};
