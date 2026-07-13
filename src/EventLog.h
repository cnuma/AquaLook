#pragma once
#include <Arduino.h>
#include "FaultManager.h"

enum LogLevel : uint8_t {
    LOG_INFO  = 0,
    LOG_WARN  = 1,
    LOG_ERROR = 2
};

static constexpr uint8_t LOG_CAPACITY = 60;
static constexpr uint8_t LOG_MSG_LEN  = 72;

struct LogEntry {
    uint32_t ms;
    LogLevel level;
    char msg[LOG_MSG_LEN];

    LogEntry() : ms(0), level(LOG_INFO) { msg[0] = '\0'; }
};

class EventLog {
public:
    static void log(LogLevel level, const char* fmt, ...) {
        char buf[LOG_MSG_LEN];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        const uint32_t nowMs = millis();
        const uint8_t idx = (_head + _count) % LOG_CAPACITY;
        _buf[idx].ms = nowMs;
        _buf[idx].level = level;
        strlcpy(_buf[idx].msg, buf, LOG_MSG_LEN);

        if (_count < LOG_CAPACITY) {
            _count++;
        } else {
            _head = (_head + 1) % LOG_CAPACITY;
        }

        if (level >= LOG_ERROR) {
            _hasErrors = true;
            FaultManager::notifyError();
        }

        const char* prefix = (level == LOG_ERROR) ? "[ERR] " :
                             (level == LOG_WARN)  ? "[WRN] " : "[INF] ";
        const uint32_t totalSeconds = nowMs / 1000UL;
        const uint32_t hours = totalSeconds / 3600UL;
        const uint32_t minutes = (totalSeconds % 3600UL) / 60UL;
        const uint32_t seconds = totalSeconds % 60UL;
        const uint32_t millisPart = nowMs % 1000UL;
        Serial.printf(
            "[%02lu:%02lu:%02lu.%03lu] %s%s\n",
            static_cast<unsigned long>(hours),
            static_cast<unsigned long>(minutes),
            static_cast<unsigned long>(seconds),
            static_cast<unsigned long>(millisPart),
            prefix,
            buf
        );
    }

    static uint8_t count() { return _count; }

    static const LogEntry& get(uint8_t i) {
        const uint8_t idx =
            (_head + _count - 1U - i) % LOG_CAPACITY;
        return _buf[idx];
    }

    // Vide le journal et acquitte l'alarme historique.
    // Les defauts actifs restent actifs dans FaultManager.
    static void clear() {
        _count = 0;
        _head = 0;
        _hasErrors = false;
        FaultManager::acknowledge();
    }

    static bool hasErrors() { return _hasErrors; }

    // Acquitte sans effacer les entrees.
    static void ackErrors() {
        _hasErrors = false;
        FaultManager::acknowledge();
    }

    static void msToHms(uint32_t ms, char* buf, uint8_t len) {
        const uint32_t s = ms / 1000;
        const uint32_t h = s / 3600;
        const uint32_t m = (s % 3600) / 60;
        const uint32_t sec = s % 60;
        snprintf(buf, len, "%02u:%02u:%02u", h, m, sec);
    }

    static const char* levelStr(LogLevel l) {
        switch (l) {
            case LOG_INFO:  return "INFO";
            case LOG_WARN:  return "WARN";
            case LOG_ERROR: return "ERR ";
            default:        return "????";
        }
    }

    static uint16_t levelColor(LogLevel l) {
        switch (l) {
            case LOG_ERROR: return 0xF800;
            case LOG_WARN:  return 0xFD20;
            default:        return 0x7BEF;
        }
    }

private:
    static LogEntry _buf[LOG_CAPACITY];
    static uint8_t _head;
    static uint8_t _count;
    static bool _hasErrors;
};