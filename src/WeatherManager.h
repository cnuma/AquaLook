#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

class ConfigManager;

// ── Prévisions par jour ────────────────────────
struct ForecastDay {
    float    rainMm          = 0.0f;
    float    tempMax         = -99.0f;
    float    tempMin         =  99.0f;
    float    feelsLikeMax    = -99.0f;
    float    windMaxKmh      = 0.0f;
    int16_t  windDeg         = -1;
    float    gustMaxKmh      = 0.0f;
    uint8_t  rainProbability = 0;
    uint8_t  humidityMax     = 0;
    uint8_t  cloudsMax       = 0;
    uint16_t pressureAvg     = 0;
    char     description[40] = "";
    char     icon[8]         = "";
    bool     valid           = false;
};

// ═══════════════════════════════════════════════════════════════
//  WeatherManager — fetch OWM 5 jours
//
//  Le chemin appelé depuis loop() est non bloquant :
//  - update() décide si un fetch est nécessaire ;
//  - une seule tâche de travail effectue HTTP + JSON ;
//  - update() applique ensuite un résultat borné et statique.
//
//  Aucun accès ConfigManager n'est effectué depuis la tâche :
//  la configuration utile est copiée avant son lancement.
// ═══════════════════════════════════════════════════════════════
class WeatherManager {
public:
    void begin(ConfigManager* config = nullptr);
    void update(bool wifiConnected);

    bool   isRainExpected() const;
    float  getRainMm() const;
    float  getTempC() const;
    bool   hasFetched() const;
    String getStatusStr() const;

    ForecastDay getForecastDay(uint8_t offset) const;

private:
    struct FetchRequest {
        char apiKey[96] = "";
        char city[64] = "";
        char country[8] = "";
        char units[16] = "metric";
        float lat = 0.0f;
        float lon = 0.0f;
        float rainThresholdMm = DEFAULT_RAIN_THRESHOLD;
    };

    struct FetchResult {
        ForecastDay forecast[5];
        float rainMm = 0.0f;
        float tempC = 0.0f;
        bool rainExpected = false;
        bool success = false;
        int16_t httpCode = 0;
        char error[64] = "";
    };

    ConfigManager* _config = nullptr;
    bool _rainExpected = false;
    float _rainMm = 0.0f;
    float _tempC = 0.0f;
    bool _fetched = false;
    uint32_t _lastCheck = 0;
    bool _forceFetch = false;

    ForecastDay _forecast[5];

    volatile bool _fetchInProgress = false;
    volatile bool _resultReady = false;
    FetchRequest _request;
    FetchResult _pendingResult;

    static constexpr uint32_t FETCH_TASK_STACK_BYTES = 12288;
    static constexpr UBaseType_t FETCH_TASK_PRIORITY = 1;

    bool startFetch();
    void applyPendingResult();

    static void fetchTaskEntry(void* context);
    void performFetch();
};
