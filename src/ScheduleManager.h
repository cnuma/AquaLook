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
    uint32_t    intervalAnchorDay = 0;     // epoch/86400, origine fixe du cycle
    RainConfig  rain;
    DaySchedule daySlots[NB_DAYS];         // slots par jour
    DaySchedule intervalSlots;            // slots mode intervalle
    ZoneSchedule() : mode(0), intervalDays(2), intervalAnchorDay(0) {}
};

// ── Slot actif ────────────────────────────────
struct ActiveSlot {
    bool     running   = false;
    uint32_t startMs   = 0;     // millis() de début
    uint32_t durationMs = 0;    // durée totale en ms
    bool     isManual  = false;

    // ── Suspension ────────────────────────────────────────────────────
    // Une regle peut INTERROMPRE un arrosage ou le SUSPENDRE. Les deux
    // existent parce qu'ils repondent a des besoins differents : couper une
    // zone parce qu'il pleut n'appelle pas de reprise, couper parce que la
    // cuve est vide en appelle une.
    //
    // Suspendre conserve le reliquat. Sans lui, "on rouvre le temps restant"
    // ne veut rien dire, et l'utilisateur devrait calculer la reprise
    // lui-meme dans son script -- c'est-a-dire ecrire la partie fragile.
    bool     paused    = false;
    uint32_t remainingMs = 0;   // reliquat conserve pendant la suspension
    uint32_t pausedAtMs  = 0;

    ActiveSlot() : running(false), startMs(0), durationMs(0), isManual(false),
                   paused(false), remainingMs(0), pausedAtMs(0) {}
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
    void setIntervalAnchorDay(uint8_t zone, uint32_t epochDay);
    void clearIntervalProgramming(uint8_t zone);
    void setDaySlot(uint8_t zone, uint8_t day, uint8_t slotIdx,
                    uint8_t h, uint8_t m, uint16_t dur, bool enabled);
    void setIntervalSlot(uint8_t zone, uint8_t slotIdx,
                         uint8_t h, uint8_t m, uint16_t dur, bool enabled);
    void setRainConfig(uint8_t zone, float threshMm, uint8_t hours);
    void setManualDuration(uint16_t minutes);

    // ── Arrosage manuel ───────────────────────
    void startManualWatering(uint8_t zone);
    void stopManualWatering(uint8_t zone);

    // ── Suspension et reprise ─────────────────
    // pauseZone conserve le reliquat et ferme la vanne ; resumeZone rouvre
    // pour ce reliquat. Une suspension qui dure trop longtemps est ABANDONNEE
    // et signalee : tenir une zone indefiniment parce qu'une cuve ne se
    // remplit jamais serait pire que de renoncer.
    // Demarrage pour une duree EXACTE, en secondes. Un script calcule des
    // durees ; les arrondir a la minute lui ferait mentir sur ce qu il a
    // demande.
    bool startZoneForSeconds(uint8_t zone, uint32_t seconds);
    bool pauseZone(uint8_t zone);
    bool resumeZone(uint8_t zone);
    bool isZonePaused(uint8_t zone) const;
    uint32_t getPausedRemainingMs(uint8_t zone) const;

    // Au-dela, la suspension est abandonnee. Deux heures : assez pour remplir
    // une cuve, trop court pour qu'un arrosage oublie reprenne le lendemain
    // en pleine chaleur.
    static constexpr uint32_t PAUSE_MAX_MS = 2UL * 3600UL * 1000UL;

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
    void       activateZoneMs(uint8_t zone, uint32_t durationMs, bool manual);
    void       expirePauses();
    void       deactivateZone(uint8_t zone);
    void       checkSlotEnd(uint8_t zone);
};
