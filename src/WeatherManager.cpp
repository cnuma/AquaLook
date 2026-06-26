#include "WeatherManager.h"
#include "ConfigManager.h"
#include "EventBus.h"

// OWM_CHECK_INTERVAL_MS defini dans config.h

// ─────────────────────────────────────────────────────────────
void WeatherManager::begin(ConfigManager* config) {
    _config = config;
    Serial.println("[Meteo] Initialisé");
}

// ─────────────────────────────────────────────────────────────
void WeatherManager::update(bool wifiConnected) {
    if (!wifiConnected) return;

    // Invariant I20 : nouvelle config OWM → re-fetch immédiat
    if (EventBus::configDirty && _config) {
        if (_config->owm().apiKey[0] != '\0') {
            _forceFetch = true;
            _fetched    = false;
            Serial.println("[Meteo] Reconfiguration — nouveau fetch OWM");
        } else {
            _fetched = false;  // permettre détection future d'une clé
        }
        EventBus::configDirty = false;
    }

    const uint32_t now = millis();
    if (!_fetched || _forceFetch ||
        (now - _lastCheck >= OWM_CHECK_INTERVAL_MS)) {
        _lastCheck  = now;
        _forceFetch = false;
        fetch();
    }
}

// ─────────────────────────────────────────────────────────────
//  Fetch OWM forecast — cnt=40 créneaux 3h = 5 jours
// ─────────────────────────────────────────────────────────────
void WeatherManager::fetch() {
    // Vérifier qu'on a une clé API
    const char* apiKey = _config ? _config->owm().apiKey : OWM_API_KEY;
    if (!apiKey || apiKey[0] == '\0') {
        // Marquer comme "fetched" pour éviter les appels répétés sans clé
        // Le fetch sera relancé si une clé est configurée (via configDirty)
        if (!_fetched) {
            Serial.println("[Meteo] Pas de clé API — météo désactivée");
            _fetched = true;  // stoppe les appels répétés
        }
        return;
    }

    float lat  = _config ? _config->owm().lat  : 0.0f;
    float lon  = _config ? _config->owm().lon  : 0.0f;
    const char* units = _config ? _config->owm().units : "metric";

    String url;
    const char* city    = _config ? _config->owm().city    : "";
    const char* country = _config ? _config->owm().country : OWM_COUNTRY;

    if (lat != 0.0f || lon != 0.0f) {
        // Mode coordonnées GPS — prioritaire
        url = "http://api.openweathermap.org/data/2.5/forecast?lat="
              + String(lat, 4) + "&lon=" + String(lon, 4)
              + "&appid=" + String(apiKey)
              + "&units=" + String(units) + "&cnt=40";
    } else if (city && city[0] != '\0') {
        // Mode ville/pays depuis config flash
        url = "http://api.openweathermap.org/data/2.5/forecast?q="
              + String(city) + "," + String(country)
              + "&appid=" + String(apiKey)
              + "&units=" + String(units) + "&cnt=40";
    } else {
        // Fallback compile-time (config.h)
        url = "http://api.openweathermap.org/data/2.5/forecast?q="
              + String(OWM_CITY) + "," + String(OWM_COUNTRY)
              + "&appid=" + String(apiKey)
              + "&units=" + String(units) + "&cnt=40";
    }

    Serial.println("[Meteo] Fetch OWM...");

    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[Meteo] Erreur HTTP %d\n", code);
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[Meteo] Erreur JSON : %s\n", err.c_str());
        return;
    }

    // Réinitialiser prévisions
    for (uint8_t i = 0; i < 5; i++) _forecast[i] = ForecastDay{};

    float totalRain = 0.0f;
    float firstTemp = 0.0f;
    bool  gotTemp   = false;

    // Récupérer la date du jour (epoch/86400)
    time_t nowT;
    time(&nowT);
    uint32_t todayDay = (nowT > 86400) ? (uint32_t)(nowT / 86400UL) : 0;
    bool useEpoch = (todayDay > 0);

    uint8_t entryCount = 0;  // fallback si todayDay inconnu
    uint16_t pressureSum[5]   = {0, 0, 0, 0, 0};
    uint8_t  pressureCount[5] = {0, 0, 0, 0, 0};
    uint8_t  bestNoonDist[5]  = {255, 255, 255, 255, 255};
    const int32_t cityTzOffset = doc["city"]["timezone"] | 0;

    for (JsonObject entry : doc["list"].as<JsonArray>()) {
        float rain = 0.0f;
        if (entry["rain"].is<JsonObject>()) {
            rain = entry["rain"]["3h"] | 0.0f;
        }
        const float temp      = entry["main"]["temp"]       | 0.0f;
        const float feelsLike = entry["main"]["feels_like"] | temp;
        const uint8_t humidity = constrain((int)(entry["main"]["humidity"] | 0), 0, 100);
        const uint8_t clouds   = constrain((int)(entry["clouds"]["all"] | 0), 0, 100);
        const uint16_t pressure = (uint16_t)(entry["main"]["pressure"] | 0);
        const float windKmh = (entry["wind"]["speed"] | 0.0f) * 3.6f;
        const float gustKmh = (entry["wind"]["gust"]  | 0.0f) * 3.6f;
        const float popRaw = entry["pop"] | 0.0f;
        const uint8_t popPct = constrain((int)lroundf(popRaw * 100.0f), 0, 100);

        const uint32_t dt = entry["dt"] | (uint32_t)0;
        int8_t offset;
        if (useEpoch) {
            const int64_t localEntry = (int64_t)dt + cityTzOffset;
            const int64_t localNow   = (int64_t)nowT + cityTzOffset;
            const int32_t entryDay   = (int32_t)(localEntry / 86400LL);
            const int32_t localToday = (int32_t)(localNow   / 86400LL);
            offset = (int8_t)(entryDay - localToday);
        } else {
            // NTP pas encore synchronisé : distribuer séquentiellement (8 créneaux/jour)
            offset = (int8_t)(entryCount / 8);
        }
        entryCount++;

        // Cumul pluie J+0
        if (offset == 0) totalRain += rain;

        // Stocker et agréger par jour
        if (offset >= 0 && offset < 5) {
            ForecastDay& day = _forecast[offset];
            day.rainMm += rain;
            if (temp > day.tempMax) day.tempMax = temp;
            if (temp < day.tempMin) day.tempMin = temp;
            if (feelsLike > day.feelsLikeMax) day.feelsLikeMax = feelsLike;
            if (windKmh > day.windMaxKmh) day.windMaxKmh = windKmh;
            if (gustKmh > day.gustMaxKmh) day.gustMaxKmh = gustKmh;
            if (popPct > day.rainProbability) day.rainProbability = popPct;
            if (humidity > day.humidityMax) day.humidityMax = humidity;
            if (clouds > day.cloudsMax) day.cloudsMax = clouds;
            if (pressure > 0) {
                pressureSum[offset] += pressure;
                pressureCount[offset]++;
            }

            // Condition représentative : créneau le plus proche de midi local
            uint8_t localHour = 12;
            if (dt > 0) {
                int64_t localEpoch = (int64_t)dt + cityTzOffset;
                localHour = (uint8_t)((localEpoch % 86400LL) / 3600LL);
            }
            const uint8_t noonDist = (uint8_t)abs((int)localHour - 12);
            if (noonDist < bestNoonDist[offset]) {
                bestNoonDist[offset] = noonDist;
                const char* desc = entry["weather"][0]["description"] | "";
                const char* icon = entry["weather"][0]["icon"] | "";
                strlcpy(day.description, desc, sizeof(day.description));
                strlcpy(day.icon, icon, sizeof(day.icon));
            }
            day.valid = true;
        }

        if (!gotTemp) {
            firstTemp = temp;
            gotTemp   = true;
        }
    }

    for (uint8_t i = 0; i < 5; i++) {
        if (pressureCount[i] > 0) {
            _forecast[i].pressureAvg = pressureSum[i] / pressureCount[i];
        }
    }

    _rainMm       = totalRain;
    _tempC        = firstTemp;
    _rainExpected = _config
        ? (totalRain >= _config->zone(0).rain.thresholdMm)
        : (totalRain >= DEFAULT_RAIN_THRESHOLD);
    _fetched      = true;

    Serial.printf("[Meteo] Pluie 24h : %.1fmm — Temp : %.1f°C — %s\n",
        totalRain, firstTemp,
        _rainExpected ? "ARROSAGE BLOQUÉ" : "OK");
}

// ─────────────────────────────────────────────────────────────
//  Getters
// ─────────────────────────────────────────────────────────────
bool   WeatherManager::isRainExpected() const { return _rainExpected; }
float  WeatherManager::getRainMm()      const { return _rainMm; }
float  WeatherManager::getTempC()       const { return _tempC; }
bool   WeatherManager::hasFetched()     const { return _fetched; }

ForecastDay WeatherManager::getForecastDay(uint8_t offset) const {
    if (offset >= 5) return ForecastDay{};
    return _forecast[offset];
}

String WeatherManager::getStatusStr() const {
    if (!_fetched) return "En attente...";
    return _rainExpected
        ? "Pluie " + String(_rainMm, 1) + "mm — bloqué"
        : "OK " + String(_rainMm, 1) + "mm";
}