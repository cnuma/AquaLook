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
    // Seuils memoire exiges avant de lancer un fetch. Voir la note detaillee
    // dans WeatherManager.cpp : sans eux, la reponse de ~17 Ko epuisait le tas
    // et la premiere connexion HTTP suivante faisait abort() le systeme.
    static constexpr uint32_t MIN_FREE_FOR_FETCH  = 45000UL;
    static constexpr uint32_t MIN_BLOCK_FOR_FETCH = 25000UL;
    static constexpr uint32_t FETCH_RETRY_ON_LOW_MEMORY_MS = 120000UL;

private:
    // Distingue un report volontaire pour cause de memoire d'un vrai echec de
    // creation de tache, pour ne pas journaliser une cause fausse.
    bool _fetchDeferredForMemory = false;

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
        int32_t payloadSize = -1;
        char error[64] = "";
    };

    ConfigManager* _config = nullptr;
    bool _rainExpected = false;
    float _rainMm = 0.0f;
    float _tempC = 0.0f;
    bool _fetched = false;
    uint32_t _lastCheck = 0;
    uint32_t _nextFetchAt = 0;
    bool _forceFetch = false;

    ForecastDay _forecast[5];

    volatile bool _fetchInProgress = false;
    volatile bool _resultReady = false;
    FetchRequest _request;
    FetchResult _pendingResult;

    static constexpr uint32_t FETCH_TASK_STACK_BYTES = 12288;
    static constexpr UBaseType_t FETCH_TASK_PRIORITY = 1;
    // Delai avant la toute premiere requete meteo, pour ne pas ajouter son pic
    // memoire a celui du demarrage (voir le commentaire de begin()). Assez long
    // pour que WiFi, serveur Web et affichage aient fini de s'installer, assez
    // court pour que la meteo apparaisse sans attente perceptible.
    static constexpr uint32_t FIRST_FETCH_DELAY_MS = 20000UL;
    static constexpr uint32_t FETCH_RETRY_DELAY_MS = 60000UL;

    bool startFetch();
    void applyPendingResult();

    static void fetchTaskEntry(void* context);
    void performFetch();
};