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
    static constexpr uint32_t LOOP_OVERRUN_THRESHOLD_US = 100000UL;
    static constexpr uint32_t LOOP_OVERRUN_LOG_INTERVAL_MS = 5000UL;

    static void begin();
    static void loopEnter();
    static void loopExit();

    // Surveillance memoire : echantillonne le tas et alerte AVANT l'epuisement.
    // Appelee depuis loopExit(), throttlee en interne.
    static void sampleMemory(uint32_t nowMs);

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
    static uint32_t _loopOverrunCount;

    // ── Surveillance memoire ──────────────────────────────────────────────
    // Seuils sur le tas libre total plutot que sur le plus gros bloc : ce
    // dernier varie legitimement de 110 Ko (ecran en veille, sprites liberes)
    // a 17 Ko (ecran allume), et descend meme vers 7 Ko pendant un rendu — un
    // seuil instantane dessus produirait des alertes en rafale sans signifier
    // quoi que ce soit. Le tas libre total, lui, reste autour de 32 Ko au
    // repos ecran allume : 10 Ko laissent donc une marge reelle avant la
    // panne, sans fausse alerte. Hysteresis pour eviter le clignotement.
    static constexpr uint32_t MEM_SAMPLE_INTERVAL_MS = 5000UL;
    static constexpr uint32_t MEM_LOW_FREE_BYTES     = 10000UL;
    static constexpr uint32_t MEM_RECOVER_FREE_BYTES = 16000UL;
    static constexpr uint32_t MEM_LOG_INTERVAL_MS    = 60000UL;

    // Handle de loopTask, capture dans begin() qui s'execute dans cette tache.
    static TaskHandle_t _loopTaskHandle;
    static uint32_t _memSampleAtMs;
    static uint32_t _memLogAtMs;
    static uint32_t _minFreeBytes;      // plancher observe depuis le demarrage
    static uint32_t _minLargestBlock;   // idem, meilleur predicteur d'echec
    static bool     _memLowActive;
    static uint32_t _lastLoopOverrunUs;
    static uint32_t _lastLoopOverrunAtMs;
    static uint32_t _lastLoopOverrunLogAtMs;

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
