#include "SystemDiagnostics.h"
#include <WiFi.h>
#include "EventLog.h"
#include "TimeUtils.h"
#include "RuntimeProfiler.h"

#ifndef AQUALOOK_VERSION
#define AQUALOOK_VERSION "unknown"
#endif
#ifndef AQUALOOK_BUILD_NUMBER
#define AQUALOOK_BUILD_NUMBER "unknown"
#endif
#ifndef AQUALOOK_GIT_SHA
#define AQUALOOK_GIT_SHA "unknown"
#endif
#ifndef AQUALOOK_GIT_BRANCH
#define AQUALOOK_GIT_BRANCH "unknown"
#endif


portMUX_TYPE SystemDiagnostics::_mux = portMUX_INITIALIZER_UNLOCKED;

uint32_t SystemDiagnostics::_bootMs = 0;
uint32_t SystemDiagnostics::_loopStartedUs = 0;
uint32_t SystemDiagnostics::_lastLoopMs = 0;
uint32_t SystemDiagnostics::_loopCount = 0;
uint32_t SystemDiagnostics::_loopDurationUs = 0;
uint32_t SystemDiagnostics::_loopDurationMaxUs = 0;
uint32_t SystemDiagnostics::_loopPeriodUs = 0;
uint32_t SystemDiagnostics::_loopPeriodMaxUs = 0;
uint64_t SystemDiagnostics::_loopDurationTotalUs = 0;
uint32_t SystemDiagnostics::_loopOverrunCount = 0;
uint32_t SystemDiagnostics::_lastLoopOverrunUs = 0;
uint32_t SystemDiagnostics::_lastLoopOverrunAtMs = 0;
uint32_t SystemDiagnostics::_lastLoopOverrunLogAtMs = 0;

uint32_t SystemDiagnostics::_webResponses = 0;
uint32_t SystemDiagnostics::_webErrors = 0;
uint32_t SystemDiagnostics::_lastWebGenerationUs = 0;
uint32_t SystemDiagnostics::_maxWebGenerationUs = 0;
uint32_t SystemDiagnostics::_lastWebAtMs = 0;
uint16_t SystemDiagnostics::_lastWebStatus = 0;
size_t   SystemDiagnostics::_lastWebBytes = 0;
char     SystemDiagnostics::_lastWebUri[64] = "";

void SystemDiagnostics::begin() {
    RuntimeProfiler::begin();
    portENTER_CRITICAL(&_mux);
    _bootMs = millis();
    _lastLoopMs = millis();
    _loopCount = 0;
    _loopDurationUs = 0;
    _loopDurationMaxUs = 0;
    _loopPeriodUs = 0;
    _loopPeriodMaxUs = 0;
    _loopDurationTotalUs = 0;
    _loopOverrunCount = 0;
    _lastLoopOverrunUs = 0;
    _lastLoopOverrunAtMs = 0;
    _lastLoopOverrunLogAtMs = 0;
    _webResponses = 0;
    _webErrors = 0;
    _lastWebGenerationUs = 0;
    _maxWebGenerationUs = 0;
    _lastWebAtMs = 0;
    _lastWebStatus = 0;
    _lastWebBytes = 0;
    _lastWebUri[0] = '\0';
    portEXIT_CRITICAL(&_mux);
}

void SystemDiagnostics::loopEnter() {
    const uint32_t nowUs = micros();
    const uint32_t nowMs = millis();

    portENTER_CRITICAL(&_mux);
    if (_loopCount > 0) {
        const uint32_t previousUs = _loopStartedUs;
        _loopPeriodUs = nowUs - previousUs;
        if (_loopPeriodUs > _loopPeriodMaxUs) {
            _loopPeriodMaxUs = _loopPeriodUs;
        }
    }
    _loopStartedUs = nowUs;
    _lastLoopMs = nowMs;
    portEXIT_CRITICAL(&_mux);
}

void SystemDiagnostics::loopExit() {
    const uint32_t durationUs = micros() - _loopStartedUs;
    const uint32_t nowMs = millis();
    bool shouldLogOverrun = false;
    uint32_t overrunCount = 0;

    portENTER_CRITICAL(&_mux);
    _loopDurationUs = durationUs;
    if (durationUs > _loopDurationMaxUs) {
        _loopDurationMaxUs = durationUs;
    }
    _loopDurationTotalUs += durationUs;
    _loopCount++;

    if (durationUs > LOOP_OVERRUN_THRESHOLD_US) {
        _loopOverrunCount++;
        _lastLoopOverrunUs = durationUs;
        _lastLoopOverrunAtMs = nowMs;
        overrunCount = _loopOverrunCount;

        if (_lastLoopOverrunLogAtMs == 0U ||
            AquaLook::Time::elapsedAtLeast(
                nowMs,
                _lastLoopOverrunLogAtMs,
                LOOP_OVERRUN_LOG_INTERVAL_MS)) {
            _lastLoopOverrunLogAtMs = nowMs;
            shouldLogOverrun = true;
        }
    }
    portEXIT_CRITICAL(&_mux);

    // Le journal est emis hors section critique. La limitation evite une
    // tempete de logs qui aggraverait elle-meme le ralentissement mesure.
    if (shouldLogOverrun) {
        EventLog::log(
            LOG_WARN,
            "Timing: boucle lente duration=%lu us threshold=%lu us count=%lu",
            static_cast<unsigned long>(durationUs),
            static_cast<unsigned long>(LOOP_OVERRUN_THRESHOLD_US),
            static_cast<unsigned long>(overrunCount)
        );
    }
}

void SystemDiagnostics::noteWebResponse(const char* uri,
                                        uint16_t statusCode,
                                        size_t responseBytes,
                                        uint32_t generationUs) {
    portENTER_CRITICAL(&_mux);
    _webResponses++;
    if (statusCode >= 400) _webErrors++;
    _lastWebGenerationUs = generationUs;
    if (generationUs > _maxWebGenerationUs) {
        _maxWebGenerationUs = generationUs;
    }
    _lastWebAtMs = millis();
    _lastWebStatus = statusCode;
    _lastWebBytes = responseBytes;
    strlcpy(_lastWebUri, uri ? uri : "", sizeof(_lastWebUri));
    portEXIT_CRITICAL(&_mux);
}

const char* SystemDiagnostics::resetReasonStr(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:   return "mise sous tension";
        case ESP_RST_EXT:       return "reset externe";
        case ESP_RST_SW:        return "redemarrage logiciel";
        case ESP_RST_PANIC:     return "panic / exception";
        case ESP_RST_INT_WDT:   return "watchdog interruption";
        case ESP_RST_TASK_WDT:  return "watchdog tache";
        case ESP_RST_WDT:       return "watchdog";
        case ESP_RST_DEEPSLEEP: return "sortie veille profonde";
        case ESP_RST_BROWNOUT:  return "brownout alimentation";
        case ESP_RST_SDIO:      return "reset SDIO";
        default:                return "inconnue";
    }
}

void SystemDiagnostics::fillJson(JsonDocument& doc, const WiFiManager* wifi) {
    uint32_t loopCount;
    uint32_t loopDurationUs;
    uint32_t loopDurationMaxUs;
    uint32_t loopPeriodUs;
    uint32_t loopPeriodMaxUs;
    uint64_t loopDurationTotalUs;
    uint32_t loopOverrunCount;
    uint32_t lastLoopOverrunUs;
    uint32_t lastLoopOverrunAtMs;
    uint32_t lastLoopMs;
    uint32_t webResponses;
    uint32_t webErrors;
    uint32_t lastWebGenerationUs;
    uint32_t maxWebGenerationUs;
    uint32_t lastWebAtMs;
    uint16_t lastWebStatus;
    size_t lastWebBytes;
    char lastWebUri[64];

    portENTER_CRITICAL(&_mux);
    loopCount = _loopCount;
    loopDurationUs = _loopDurationUs;
    loopDurationMaxUs = _loopDurationMaxUs;
    loopPeriodUs = _loopPeriodUs;
    loopPeriodMaxUs = _loopPeriodMaxUs;
    loopDurationTotalUs = _loopDurationTotalUs;
    loopOverrunCount = _loopOverrunCount;
    lastLoopOverrunUs = _lastLoopOverrunUs;
    lastLoopOverrunAtMs = _lastLoopOverrunAtMs;
    lastLoopMs = _lastLoopMs;
    webResponses = _webResponses;
    webErrors = _webErrors;
    lastWebGenerationUs = _lastWebGenerationUs;
    maxWebGenerationUs = _maxWebGenerationUs;
    lastWebAtMs = _lastWebAtMs;
    lastWebStatus = _lastWebStatus;
    lastWebBytes = _lastWebBytes;
    strlcpy(lastWebUri, _lastWebUri, sizeof(lastWebUri));
    portEXIT_CRITICAL(&_mux);

    JsonObject system = doc["system"].to<JsonObject>();
    system["uptimeSec"] = millis() / 1000UL;
    system["cpuMhz"] = ESP.getCpuFreqMHz();
    system["sdk"] = ESP.getSdkVersion();
    system["chipRevision"] = ESP.getChipRevision();
    system["resetReason"] = resetReasonStr(esp_reset_reason());
    system["loopCore"] = xPortGetCoreID();

    JsonObject build = doc["build"].to<JsonObject>();
    build["version"] = AQUALOOK_VERSION;
    build["number"] = AQUALOOK_BUILD_NUMBER;
    build["gitSha"] = AQUALOOK_GIT_SHA;
    build["gitBranch"] = AQUALOOK_GIT_BRANCH;
#if AQUALOOK_RELAY_BACKEND_V4
    build["relayBackend"] = "v4";
#else
    build["relayBackend"] = "legacy";
#endif
    build["compiledDate"] = __DATE__;
    build["compiledTime"] = __TIME__;

    JsonObject memory = doc["memory"].to<JsonObject>();
    memory["heapFree"] = ESP.getFreeHeap();
    memory["heapMin"] = ESP.getMinFreeHeap();
    memory["heapLargestBlock"] =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    memory["heapSize"] = ESP.getHeapSize();
    memory["psramSize"] = ESP.getPsramSize();
    memory["psramFree"] = ESP.getFreePsram();
    memory["loopStackHighWaterWords"] = uxTaskGetStackHighWaterMark(nullptr);

    const uint32_t nowMs = millis();
    JsonObject loop = doc["loop"].to<JsonObject>();
    loop["count"] = loopCount;
    loop["lastDurationUs"] = loopDurationUs;
    loop["maxDurationUs"] = loopDurationMaxUs;
    loop["lastPeriodUs"] = loopPeriodUs;
    loop["maxPeriodUs"] = loopPeriodMaxUs;
    loop["ageMs"] = nowMs - lastLoopMs;
    loop["averageDurationUs"] =
        loopCount ? static_cast<uint32_t>(loopDurationTotalUs / loopCount) : 0U;
    loop["healthy"] = (nowMs - lastLoopMs) < 2000UL;
    loop["overrunThresholdUs"] = LOOP_OVERRUN_THRESHOLD_US;
    loop["overrunCount"] = loopOverrunCount;
    loop["lastOverrunUs"] = lastLoopOverrunUs;
    loop["lastOverrunAgeMs"] =
        lastLoopOverrunAtMs ? nowMs - lastLoopOverrunAtMs : 0U;

#if (configGENERATE_RUN_TIME_STATS == 1)
    loop["cpuStatsAvailable"] = true;
#else
    loop["cpuStatsAvailable"] = false;
#endif

    JsonObject web = doc["web"].to<JsonObject>();
    web["responses"] = webResponses;
    web["errors"] = webErrors;
    web["lastUri"] = lastWebUri;
    web["lastStatus"] = lastWebStatus;
    web["lastBytes"] = static_cast<uint32_t>(lastWebBytes);
    web["lastGenerationUs"] = lastWebGenerationUs;
    web["maxGenerationUs"] = maxWebGenerationUs;
    web["lastAgeMs"] = lastWebAtMs ? nowMs - lastWebAtMs : 0U;

    JsonObject w = doc["wifi"].to<JsonObject>();
    if (wifi) {
        w["state"] = wifi->stateStr();
        w["connected"] = wifi->isConnected();
        w["captive"] = wifi->isCaptivePortal();
        w["rssi"] = wifi->getRssi();
        w["ip"] = wifi->isConnected()
                    ? wifi->getIP().toString()
                    : wifi->getApIP().toString();
    } else {
        w["state"] = "indisponible";
        w["connected"] = false;
        w["captive"] = false;
        w["rssi"] = 0;
        w["ip"] = "";
    }

    w["channel"] = WiFi.channel();
    w["mac"] = WiFi.macAddress();

    RuntimeProfiler::fillJson(doc);
}
