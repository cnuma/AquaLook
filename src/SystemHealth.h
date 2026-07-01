#pragma once
#include <Arduino.h>

enum class SystemFault : uint8_t {
    LITTLEFS = 0,
    WIFI     = 1,
    NTP      = 2
};

class SystemHealth {
public:
    static void setFault(SystemFault fault, bool active);
    static void updateRuntime(bool wifiConnected, bool ntpSynced);

    static bool hasAny();
    static bool hasFault(SystemFault fault);

    static const char* title();
    static const char* adviceLine1();
    static const char* adviceLine2();
    static bool rebootRecommended();

private:
    static uint8_t _faults;
    static constexpr uint32_t RUNTIME_GRACE_MS = 30000UL;
};
