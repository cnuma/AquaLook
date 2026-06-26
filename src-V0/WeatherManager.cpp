#include "WeatherManager.h"

void WeatherManager::begin() {
    Serial.println("[Meteo] Initialisé");
}

void WeatherManager::update(bool wifiConnected) {
    if (!wifiConnected) return;
    if (!_firstDone || millis() - _lastCheck >= OWM_CHECK_INTERVAL) {
        fetch();
        _firstDone = true;
        _lastCheck = millis();
    }
}

void WeatherManager::fetch() {
    Serial.println("[Meteo] Vérification...");

    String url = String("http://api.openweathermap.org/data/2.5/forecast?q=")
               + OWM_CITY + "," + OWM_COUNTRY
               + "&appid=" + OWM_API_KEY
               + "&units=metric&cnt=4";

    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[Meteo] Erreur HTTP : %d\n", code);
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        Serial.println("[Meteo] Erreur JSON");
        return;
    }

    float total = 0.0;
    for (JsonObject entry : doc["list"].as<JsonArray>()) {
        total += entry["rain"]["3h"] | 0.0f;
    }

    _rainMm       = total;
    _rainExpected = (total >= RAIN_THRESHOLD_MM);

    Serial.printf("[Meteo] %.0fh : %.1f mm — %s\n",
        (float)FORECAST_HOURS, total,
        _rainExpected ? "ARROSAGE BLOQUÉ" : "arrosage OK");
}

bool WeatherManager::isRainExpected() { return _rainExpected; }
float WeatherManager::getRainMm()     { return _rainMm; }

String WeatherManager::getStatusStr() {
    return _rainExpected
        ? String("Pluie prévue : ") + _rainMm + "mm — bloqué"
        : String("Pas de pluie — arrosage OK");
}