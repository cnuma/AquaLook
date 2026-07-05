#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "WiFiManager.h"

// Instrumentation légère AquaLook.
// Aucun stockage flash, aucune allocation périodique, aucune tâche dédiée.
class SystemDiagnostics {
public:
    static void begin();
    static void loopEnter();
    static void loopExit();

    static void noteWebResponse(const char* uri,
                                uint16_t statusCode,
                                size_t responseBytes,
                                uint32_t generationUs);

    static void fillJson(JsonDocument& doc, const WiFiManager* wifi);

private:
    static portMUX_TYPE _mux;

    static uint32_t _bootMs;
    static uint32_t _loopStartedUs;
    static uint32_t _lastLoopMs;
    static uint32_t _loopCount;
    static uint32_t _loopDurationUs;
    static uint32_t _loopDurationMaxUs;
    static uint32_t _loopPeriodUs;
    static uint32_t _loopPeriodMaxUs;
    static uint64_t _loopDurationTotalUs;

    static uint32_t _webResponses;
    static uint32_t _webErrors;
    static uint32_t _lastWebGenerationUs;
    static uint32_t _maxWebGenerationUs;
    static uint32_t _lastWebAtMs;
    static uint16_t _lastWebStatus;
    static size_t   _lastWebBytes;
    static char     _lastWebUri[64];

    static const char* resetReasonStr(esp_reset_reason_t reason);
};
