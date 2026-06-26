#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════
//  EventLog — journal d'événements en RAM (session courante)
//
//  Buffer circulaire de LOG_CAPACITY entrées — les plus anciennes
//  sont écrasées quand le buffer est plein.
//
//  Usage :
//    EventLog::log(LOG_ERROR, "Config corrompue — reset defauts");
//    EventLog::log(LOG_WARN,  "WiFi retry %d/%d", retry, max);
//    EventLog::log(LOG_INFO,  "NTP synced : %s", timeStr);
//
//  Règles :
//    - Zéro allocation dynamique (pas de String, pas de new)
//    - Thread-safe pour l'ESP32 (accès depuis loop() uniquement —
//      pas d'ISR, pas de tâche FreeRTOS concurrente)
//    - LOG_DEBUG exclu volontairement (trop verbeux, heap limité)
//    - Chaque message tronqué à LOG_MSG_LEN-1 caractères
//
//  Accès :
//    count()      → nombre d'entrées disponibles (0..LOG_CAPACITY)
//    get(i)       → entrée i (0=plus récente, count()-1=plus ancienne)
//    clear()      → vide le buffer
//    hasErrors()  → true si au moins une entrée ERROR non acquittée
// ═══════════════════════════════════════════════════════════════

// ── Niveaux de log ─────────────────────────────────────────────
enum LogLevel : uint8_t {
    LOG_INFO  = 0,
    LOG_WARN  = 1,
    LOG_ERROR = 2
};

// ── Constantes ─────────────────────────────────────────────────
static constexpr uint8_t  LOG_CAPACITY = 60;   // entrées max en RAM
static constexpr uint8_t  LOG_MSG_LEN  = 72;   // chars par message (+ null)

// ── Structure d'une entrée ─────────────────────────────────────
struct LogEntry {
    uint32_t ms;              // millis() à l'enregistrement
    LogLevel level;
    char     msg[LOG_MSG_LEN];

    LogEntry() : ms(0), level(LOG_INFO) { msg[0] = '\0'; }
};

// ═══════════════════════════════════════════════════════════════
//  EventLog — classe statique (singleton de facto)
// ═══════════════════════════════════════════════════════════════
class EventLog {
public:
    // ── Enregistrement ────────────────────────────────────────

    /// Enregistre un message avec formatage printf.
    /// Tronqué à LOG_MSG_LEN-1 chars. Copie aussi sur Serial.
    static void log(LogLevel level, const char* fmt, ...) {
        char buf[LOG_MSG_LEN];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        // Écriture dans le buffer circulaire
        uint8_t idx = (_head + _count) % LOG_CAPACITY;
        _buf[idx].ms    = millis();
        _buf[idx].level = level;
        strlcpy(_buf[idx].msg, buf, LOG_MSG_LEN);

        if (_count < LOG_CAPACITY) {
            _count++;
        } else {
            // Buffer plein : avancer la tête (écrase la plus ancienne)
            _head = (_head + 1) % LOG_CAPACITY;
        }

        if (level >= LOG_ERROR) _hasErrors = true;

        // Miroir Serial pour debug câblé
        const char* prefix = (level == LOG_ERROR) ? "[ERR] " :
                             (level == LOG_WARN)  ? "[WRN] " : "[INF] ";
        Serial.print(prefix);
        Serial.println(buf);
    }

    // ── Lecture ───────────────────────────────────────────────

    /// Nombre d'entrées disponibles (0..LOG_CAPACITY)
    static uint8_t count() { return _count; }

    /// Entrée i : 0 = plus récente, count()-1 = plus ancienne
    static const LogEntry& get(uint8_t i) {
        // i=0 → dernière écrite = (_head + _count - 1) % CAP
        uint8_t idx = (_head + _count - 1 - i) % LOG_CAPACITY;
        return _buf[idx];
    }

    /// Vide le buffer et réinitialise le flag erreur
    static void clear() {
        _count    = 0;
        _head     = 0;
        _hasErrors = false;
    }

    /// true si au moins une entrée ERROR depuis le dernier clear()
    static bool hasErrors() { return _hasErrors; }

    /// Acquitte le flag erreur sans vider le buffer
    static void ackErrors() { _hasErrors = false; }

    // ── Helpers formatage ─────────────────────────────────────

    /// Retourne "HH:MM:SS" depuis un millis()
    static void msToHms(uint32_t ms, char* buf, uint8_t len) {
        uint32_t s   = ms / 1000;
        uint32_t h   = s / 3600;
        uint32_t m   = (s % 3600) / 60;
        uint32_t sec = s % 60;
        snprintf(buf, len, "%02u:%02u:%02u", h, m, sec);
    }

    /// Libellé court du niveau (4 chars + null)
    static const char* levelStr(LogLevel l) {
        switch (l) {
            case LOG_INFO:  return "INFO";
            case LOG_WARN:  return "WARN";
            case LOG_ERROR: return "ERR ";
            default:        return "????";
        }
    }

    /// Couleur RGB565 associée au niveau (pour LCD)
    static uint16_t levelColor(LogLevel l) {
        switch (l) {
            case LOG_ERROR: return 0xF800;  // rouge
            case LOG_WARN:  return 0xFD20;  // orange
            default:        return 0x7BEF;  // gris clair
        }
    }

private:
    // Stockage statique — pas d'allocation dynamique
    static LogEntry _buf[LOG_CAPACITY];
    static uint8_t  _head;    // index de la plus ancienne entrée
    static uint8_t  _count;   // nombre d'entrées valides
    static bool     _hasErrors;
};

// ── Définitions des membres statiques (dans EventLog.h)
// Une seule unité de traduction inclut ce header avec EVENTLOG_IMPL défini.
// Dans tous les autres fichiers : include normal sans EVENTLOG_IMPL.
// EventLog.cpp définit EVENTLOG_IMPL et inclut ce header.
