#pragma once
#include <Arduino.h>
#include <time.h>
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
    time_t epoch;
    LogLevel level;
    char msg[LOG_MSG_LEN];

    LogEntry() : ms(0), epoch(0), level(LOG_INFO) { msg[0] = '\0'; }
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
        const time_t nowEpoch = validWallClockEpoch();
        const uint8_t idx = (_head + _count) % LOG_CAPACITY;
        _buf[idx].ms = nowMs;
        _buf[idx].epoch = nowEpoch;
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
        char timestamp[24];
        formatTimestamp(nowMs, nowEpoch, timestamp, sizeof(timestamp));
        Serial.printf("[%s] %s%s\n", timestamp, prefix, buf);
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

    static void formatEntryTimestamp(const LogEntry& entry, char* buf, uint8_t len) {
        formatTimestamp(entry.ms, entry.epoch, buf, len);
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
    static time_t validWallClockEpoch() {
        const time_t now = time(nullptr);
        return now >= static_cast<time_t>(1704067200) ? now : 0;
    }

    static void formatTimestamp(uint32_t ms, time_t epoch, char* buf, uint8_t len) {
        if (epoch > 0) {
            struct tm localTime;
            if (localtime_r(&epoch, &localTime) != nullptr) {
                const uint32_t millisPart = ms % 1000UL;
                snprintf(
                    buf,
                    len,
                    "%02d:%02d:%02d.%03lu",
                    localTime.tm_hour,
                    localTime.tm_min,
                    localTime.tm_sec,
                    static_cast<unsigned long>(millisPart)
                );
                return;
            }
        }

        const uint32_t totalSeconds = ms / 1000UL;
        const uint32_t hours = totalSeconds / 3600UL;
        const uint32_t minutes = (totalSeconds % 3600UL) / 60UL;
        const uint32_t seconds = totalSeconds % 60UL;
        const uint32_t millisPart = ms % 1000UL;
        snprintf(
            buf,
            len,
            "%02lu:%02lu:%02lu.%03lu",
            static_cast<unsigned long>(hours),
            static_cast<unsigned long>(minutes),
            static_cast<unsigned long>(seconds),
            static_cast<unsigned long>(millisPart)
        );
    }

    static LogEntry _buf[LOG_CAPACITY];
    static uint8_t _head;
    static uint8_t _count;
    static bool _hasErrors;
};
