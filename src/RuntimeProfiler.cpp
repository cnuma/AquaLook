#include "RuntimeProfiler.h"

#include "EventLog.h"
#include "TimeUtils.h"

portMUX_TYPE RuntimeProfiler::_mux = portMUX_INITIALIZER_UNLOCKED;
RuntimeProfiler::Metrics
RuntimeProfiler::_metrics[static_cast<uint8_t>(RuntimeProfiler::Component::COUNT)];

void RuntimeProfiler::begin() {
    portENTER_CRITICAL(&_mux);
    for (uint8_t i = 0;
         i < static_cast<uint8_t>(Component::COUNT);
         ++i) {
        _metrics[i] = Metrics{};
    }
    portEXIT_CRITICAL(&_mux);
}

uint32_t RuntimeProfiler::start() {
    return micros();
}

void RuntimeProfiler::stop(
    Component component,
    uint32_t startedUs
) {
    const uint32_t durationUs = micros() - startedUs;
    const uint32_t nowMs = millis();
    const uint8_t index = static_cast<uint8_t>(component);

    if (index >= static_cast<uint8_t>(Component::COUNT)) return;

    bool shouldLog = false;
    uint32_t slowCount = 0;

    portENTER_CRITICAL(&_mux);

    Metrics& metrics = _metrics[index];
    metrics.lastUs = durationUs;

    if (durationUs > metrics.maxUs) {
        metrics.maxUs = durationUs;
    }

    if (durationUs > SLOW_COMPONENT_THRESHOLD_US) {
        metrics.slowCount++;
        metrics.lastSlowUs = durationUs;
        metrics.lastSlowAtMs = nowMs;
        slowCount = metrics.slowCount;

        if (metrics.lastLogAtMs == 0U ||
            AquaLook::Time::elapsedAtLeast(
                nowMs,
                metrics.lastLogAtMs,
                LOG_INTERVAL_MS)) {
            metrics.lastLogAtMs = nowMs;
            shouldLog = true;
        }
    }

    portEXIT_CRITICAL(&_mux);

    if (shouldLog) {
        // Le timestamp relatif est deja ajoute par EventLog. Garder ce message
        // volontairement court afin que le nom, la duree, le compteur et le
        // coeur CPU restent visibles dans les sorties serie bornees.
        EventLog::log(
            LOG_WARN,
            "Timing: n=%s us=%lu count=%lu core=%d",
            componentName(component),
            static_cast<unsigned long>(durationUs),
            static_cast<unsigned long>(slowCount),
            xPortGetCoreID()
        );
    }
}

const char* RuntimeProfiler::componentName(Component component) {
    switch (component) {
        case Component::FAULTS_PRE:       return "faultsPre";
        case Component::STORAGE:          return "storage";
        case Component::WIFI:             return "wifi";
        case Component::NTP:              return "ntp";
        case Component::WEATHER:          return "weather";
        case Component::SCHEDULE:         return "schedule";
        case Component::EQUIPMENT_SHADOW: return "equipmentShadow";
        case Component::RELAY:            return "relay";
        case Component::WEB:              return "web";
        case Component::DISPLAY_MANAGER:  return "display";
        case Component::PLANNING_DECOR:   return "planningDecor";
        case Component::FAULTS_POST:      return "faultsPost";
        case Component::YIELD:            return "yield";
        case Component::COUNT:            return "count";
        default:                          return "unknown";
    }
}

void RuntimeProfiler::fillJson(JsonDocument& doc) {
    Metrics snapshot[static_cast<uint8_t>(Component::COUNT)];

    portENTER_CRITICAL(&_mux);
    for (uint8_t i = 0;
         i < static_cast<uint8_t>(Component::COUNT);
         ++i) {
        snapshot[i] = _metrics[i];
    }
    portEXIT_CRITICAL(&_mux);

    const uint32_t nowMs = millis();
    JsonObject root = doc["runtimeComponents"].to<JsonObject>();
    root["slowThresholdUs"] = SLOW_COMPONENT_THRESHOLD_US;

    for (uint8_t i = 0;
         i < static_cast<uint8_t>(Component::COUNT);
         ++i) {
        const Component component = static_cast<Component>(i);
        const Metrics& metrics = snapshot[i];

        JsonObject item =
            root[componentName(component)].to<JsonObject>();

        item["lastUs"] = metrics.lastUs;
        item["maxUs"] = metrics.maxUs;
        item["slowCount"] = metrics.slowCount;
        item["lastSlowUs"] = metrics.lastSlowUs;
        item["lastSlowAgeMs"] =
            metrics.lastSlowAtMs
                ? nowMs - metrics.lastSlowAtMs
                : 0U;
    }
}
