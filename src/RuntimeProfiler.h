#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class RuntimeProfiler {
public:
    enum class Component : uint8_t {
        FAULTS_PRE = 0,
        STORAGE,
        WIFI,
        NTP,
        WEATHER,
        SCHEDULE,
        EQUIPMENT_SHADOW,
        RELAY,
        WEB,
        DISPLAY,
        PLANNING_DECOR,
        FAULTS_POST,
        YIELD,
        COUNT
    };

    static constexpr uint32_t SLOW_COMPONENT_THRESHOLD_US = 50000UL;
    static constexpr uint32_t LOG_INTERVAL_MS = 5000UL;

    static void begin();
    static uint32_t start();
    static void stop(Component component, uint32_t startedUs);
    static void fillJson(JsonDocument& doc);

    static const char* componentName(Component component);

private:
    struct Metrics {
        uint32_t lastUs = 0;
        uint32_t maxUs = 0;
        uint32_t slowCount = 0;
        uint32_t lastSlowUs = 0;
        uint32_t lastSlowAtMs = 0;
        uint32_t lastLogAtMs = 0;
    };

    static portMUX_TYPE _mux;
    static Metrics _metrics[static_cast<uint8_t>(Component::COUNT)];
};
