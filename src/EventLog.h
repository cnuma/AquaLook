#pragma once
#include <Arduino.h>
#include <time.h>
#include "FaultManager.h"
#include "OtaBuildIdentity.h"

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

        // Compatibilite transitoire : main.cpp contenait historiquement un
        // libelle "AquaLook v2.0 demarrage" ecrit en dur. Tant que ce point
        // d'appel n'est pas supprime, ne jamais publier cette fausse version.
        const bool legacyBootMessage =
            strcmp(buf, "AquaLook v2.0 demarrage") == 0;
        if (legacyBootMessage) {
            snprintf(
                buf,
                sizeof(buf),
                "%s %s demarrage target=%s build=%s sha=%s",
                OtaBuildIdentity::PRODUCT,
                OtaBuildIdentity::VERSION,
                OtaBuildIdentity::OTA_TARGET,
                OtaBuildIdentity::BUILD_NUMBER,
                OtaBuildIdentity::GIT_SHA
            );

            Serial.println();
            Serial.println("============================================================");
            Serial.println("AquaLook - demarrage firmware");
            Serial.printf("Version       : %s\n", OtaBuildIdentity::VERSION);
            Serial.printf("Cible OTA     : %s\n", OtaBuildIdentity::OTA_TARGET);
            Serial.printf("Environnement : %s\n", OtaBuildIdentity::PLATFORMIO_ENVIRONMENT);
            Serial.printf("Build         : %s\n", OtaBuildIdentity::BUILD_NUMBER);
            Serial.printf("Git SHA       : %s\n", OtaBuildIdentity::GIT_SHA);
            Serial.printf("Branche       : %s\n", OtaBuildIdentity::GIT_BRANCH);
            Serial.printf("Carte         : %s\n", OtaBuildIdentity::BOARD);
            Serial.println("============================================================");
        }

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

    // Compatibilite avec les vues historiques utilisant uniquement le temps
    // depuis boot. Apres synchronisation NTP, reconstruit l'heure locale de
    // l'entree a partir de l'horloge courante et de son age en millisecondes.
    static void msToHms(uint32_t ms, char* buf, uint8_t len) {
        const time_t nowEpoch = validWallClockEpoch();
        if (nowEpoch > 0) {
            const uint32_t nowMs = millis();
            const uint32_t ageMs = nowMs - ms;
            const time_t entryEpoch = nowEpoch - static_cast<time_t>(ageMs / 1000UL);
            struct tm localTime;
            if (localtime_r(&entryEpoch, &localTime) != nullptr) {
                snprintf(
                    buf,
                    len,
                    "%02d:%02d:%02d",
                    localTime.tm_hour,
                    localTime.tm_min,
                    localTime.tm_sec
                );
                return;
            }
        }

        const uint32_t s = ms / 1000UL;
        const uint32_t h = s / 3600UL;
        const uint32_t m = (s % 3600UL) / 60UL;
        const uint32_t sec = s % 60UL;
        snprintf(
            buf,
            len,
            "%02lu:%02lu:%02lu",
            static_cast<unsigned long>(h),
            static_cast<unsigned long>(m),
            static_cast<unsigned long>(sec)
        );
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
