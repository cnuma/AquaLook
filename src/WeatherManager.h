#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

class ConfigManager;

// ── Prévisions par jour ────────────────────────
struct ForecastDay {
    float    rainMm          = 0.0f;
    float    tempMax         = -99.0f;  // initialisé bas pour que le premier temp réel gagne
    float    tempMin         =  99.0f;
    float    feelsLikeMax    = -99.0f;
    float    windMaxKmh      = 0.0f;
    int16_t  windDeg         = -1;    // direction du vent associee a la vitesse max
    float    gustMaxKmh      = 0.0f;
    uint8_t  rainProbability = 0;       // maximum journalier, en %
    uint8_t  humidityMax     = 0;
    uint8_t  cloudsMax       = 0;
    uint16_t pressureAvg     = 0;
    char     description[40] = "";      // condition représentative autour de midi
    char     icon[8]         = "";       // code icône OpenWeather, ex. 10d
    bool     valid           = false;
};

// ═══════════════════════════════════════════════════════════════
//  WeatherManager — fetch OWM 5 jours, non bloquant
//
//  Paramètres runtime (invariant I20) :
//    apiKey, lat, lon, units lus depuis ConfigManager.
//    Sur EventBus::configDirty → force un re-fetch immédiat.
//
//  Si apiKey vide → météo désactivée, getRainMm()=0, hasFetched()=false.
// ═══════════════════════════════════════════════════════════════
class WeatherManager {
public:
    void begin(ConfigManager* config = nullptr);
    void update(bool wifiConnected);

    // ── Getters état global ───────────────────
    bool   isRainExpected()  const;
    float  getRainMm()       const;
    float  getTempC()        const;
    bool   hasFetched()      const;
    String getStatusStr()    const;

    // ── Prévisions par jour (offset 0=aujourd'hui..4) ──
    ForecastDay getForecastDay(uint8_t offset) const;

private:
    ConfigManager* _config       = nullptr;
    bool           _rainExpected = false;
    float          _rainMm       = 0.0f;
    float          _tempC        = 0.0f;
    bool           _fetched      = false;
    uint32_t       _lastCheck    = 0;
    bool           _forceFetch   = false;  // positionné sur configDirty

    ForecastDay _forecast[5];  // J+0..J+4

    void fetch();
};
