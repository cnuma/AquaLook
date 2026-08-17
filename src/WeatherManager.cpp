#include "WeatherManager.h"
#include "ConfigManager.h"
#include "EventBus.h"
#include "BootLoopGuard.h"
#include "EventLog.h"

#include <esp_heap_caps.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
portMUX_TYPE g_weatherMux = portMUX_INITIALIZER_UNLOCKED;

void clearForecast(ForecastDay (&forecast)[5]) {
    for (uint8_t i = 0; i < 5; ++i) {
        forecast[i] = ForecastDay{};
    }
}

bool deadlineReached(uint32_t now, uint32_t deadline) {
    return deadline == 0 || static_cast<int32_t>(now - deadline) >= 0;
}

String payloadPreview(const String& payload) {
    String preview;
    preview.reserve(97);
    const size_t limit = payload.length() < 96 ? payload.length() : 96;
    for (size_t i = 0; i < limit; ++i) {
        const char c = payload[i];
        preview += (c == '\r' || c == '\n' || c == '\t') ? ' ' : c;
    }
    return preview;
}
}

void WeatherManager::begin(ConfigManager* config) {
    _config = config;
    clearForecast(_forecast);

    // Premier appel volontairement differe, et non immediat.
    //
    // Avec _nextFetchAt = 0, la requete partait des la premiere iteration de
    // la boucle, c'est-a-dire au moment precis ou tout le reste s'alloue :
    // 95 Ko de sprites d'affichage, association WiFi, demarrage du serveur
    // Web. S'y ajoutait la reponse OWM d'environ 16 Ko a analyser. Ce pic
    // simultane epuisait la memoire et provoquait un abort() sur allocation
    // echouee (exceptions desactivees), une a deux fois sur trois demarrages
    // observes le 17 aout 2026 — juste apres la ligne "Meteo: HTTP 200".
    //
    // Etaler suffit : rien n'exige une meteo dans les premieres secondes,
    // alors que le demarrage est le moment ou la memoire est la plus sollicitee.
    _nextFetchAt = millis() + FIRST_FETCH_DELAY_MS;

    Serial.println("[Meteo] Initialisé");
}

void WeatherManager::update(bool wifiConnected) {
    // Fonction de confort : suspendue tant que le module est en mode degrade.
    // C'est precisement une reponse meteo qui a provoque l'une des deux
    // boucles de redemarrages du 17 aout 2026.
    if (BootLoopGuard::isDegraded()) return;

    if (EventBus::configDirty && _config) {
        if (_config->owm().apiKey[0] != '\0') {
            _forceFetch = true;
            _fetched = false;
            _nextFetchAt = 0;
            Serial.println("[Meteo] Reconfiguration — nouveau fetch OWM");
        } else {
            _fetched = false;
        }
        EventBus::configDirty = false;
    }

    applyPendingResult();

    if (!wifiConnected || _fetchInProgress) return;

    const uint32_t now = millis();
    const bool due = _forceFetch || deadlineReached(now, _nextFetchAt);
    if (!due) return;

    const char* apiKey = _config ? _config->owm().apiKey : OWM_API_KEY;
    if (!apiKey || apiKey[0] == '\0') {
        if (!_fetched) {
            Serial.println("[Meteo] Pas de clé API — météo désactivée");
            _fetched = true;
        }
        _nextFetchAt = now + OWM_CHECK_INTERVAL_MS;
        return;
    }

    _lastCheck = now;
    _forceFetch = false;
    _nextFetchAt = now + FETCH_RETRY_DELAY_MS;

    if (!startFetch()) {
        // Un report volontaire pour cause de memoire s'est deja explique
        // lui-meme, avec ses chiffres. Rejournaliser "impossible de creer la
        // tache" par-dessus serait faux : aucune tache n'a ete tentee. Une
        // ligne fausse dans un journal coute plus cher qu'une ligne absente,
        // parce qu'elle envoie chercher au mauvais endroit.
        if (!_fetchDeferredForMemory) EventLog::log(
            LOG_WARN,
            "Meteo: impossible de creer la tache de fetch"
        );
    }
}

bool WeatherManager::startFetch() {
    if (_fetchInProgress) return false;

    const char* apiKey = _config ? _config->owm().apiKey : OWM_API_KEY;
    const char* city = _config ? _config->owm().city : OWM_CITY;
    const char* country = _config ? _config->owm().country : OWM_COUNTRY;
    const char* units = _config ? _config->owm().units : "metric";

    // Ne pas lancer un telechargement de ~17 Ko quand il n'y a pas la place.
    //
    // Constate le 17 aout 2026, boucle de redemarrages : a chaque demarrage,
    // "Meteo: HTTP 200 annonce=16739 lecture=stream" etait suivi dans la
    // seconde de
    //   abort() was called at PC 0x401a021b
    //   AsyncServer::_accepted -> operator new -> __cxa_throw -> std::terminate
    // Autrement dit : la reponse meteo epuisait le tas, puis la premiere
    // connexion HTTP entrante ne trouvait plus de quoi allouer son objet de
    // requete. operator new leve bad_alloc, personne ne l'attrape, et le
    // runtime C++ abat tout le systeme. Pas de degradation, pas de refus
    // poli — un redemarrage sec, en boucle, jusqu'a debrancher le module.
    //
    // Le seuil porte sur le plus GROS BLOC et pas seulement sur le total
    // libre : c'est la contrainte reelle de ce module, ou le tas est fragmente
    // par les deux tampons d'affichage permanents (~95 Ko). Un total
    // confortable avec des blocs minuscules ne permet toujours pas d'allouer.
    //
    // Une meteo reportee est invisible pour l'utilisateur : le prochain cycle
    // reessaiera. Un redemarrage ne l'est pas.
    const uint32_t freeBytes =
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    const uint32_t largestBlock =
        static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    if (freeBytes < MIN_FREE_FOR_FETCH || largestBlock < MIN_BLOCK_FOR_FETCH) {
        _fetchDeferredForMemory = true;
        portENTER_CRITICAL(&g_weatherMux);
        _fetchInProgress = false;
        portEXIT_CRITICAL(&g_weatherMux);
        _nextFetchAt = millis() + FETCH_RETRY_ON_LOW_MEMORY_MS;
        EventLog::log(LOG_WARN,
                      "Meteo: fetch reporte, memoire insuffisante "
                      "(libre=%lu plusGrosBloc=%lu seuils=%lu/%lu) — nouvel "
                      "essai dans %lu s",
                      static_cast<unsigned long>(freeBytes),
                      static_cast<unsigned long>(largestBlock),
                      static_cast<unsigned long>(MIN_FREE_FOR_FETCH),
                      static_cast<unsigned long>(MIN_BLOCK_FOR_FETCH),
                      static_cast<unsigned long>(FETCH_RETRY_ON_LOW_MEMORY_MS / 1000UL));
        return false;
    }

    _fetchDeferredForMemory = false;

    FetchRequest request;
    strlcpy(request.apiKey, apiKey ? apiKey : "", sizeof(request.apiKey));
    strlcpy(request.city, city ? city : "", sizeof(request.city));
    strlcpy(request.country, country ? country : "", sizeof(request.country));
    strlcpy(request.units, units ? units : "metric", sizeof(request.units));
    request.lat = _config ? _config->owm().lat : 0.0f;
    request.lon = _config ? _config->owm().lon : 0.0f;
    request.rainThresholdMm = _config
        ? _config->zone(0).rain.thresholdMm
        : DEFAULT_RAIN_THRESHOLD;

    portENTER_CRITICAL(&g_weatherMux);
    _request = request;
    _pendingResult = FetchResult{};
    _fetchInProgress = true;
    _resultReady = false;
    portEXIT_CRITICAL(&g_weatherMux);

    // Epinglee au coeur 1, et non laissee libre (xTaskCreate sans affinite).
    //
    // Motif, constate sur materiel : sans affinite, l'ordonnanceur peut placer
    // cette tache sur le coeur 0, celui ou tournent la pile WiFi et lwIP. Elle
    // y analyse un JSON de ~16 Ko lu directement depuis le flux reseau, ce qui
    // dure d'autant plus longtemps que le signal est faible (RSSI descendu a
    // -87 dBm le 17 aout 2026). Pendant ce temps la tache IDLE0, de priorite 0,
    // ne s'execute plus — yield() d'Arduino ne cede qu'aux priorites egales ou
    // superieures — et le chien de garde abat le systeme :
    //   E task_wdt: CPU 0: weather-fetch / CPU 1: loopTask / Aborting.
    // Signature observee trois fois sur quatre demarrages le 17 aout 2026, et
    // deja responsable des deux plantages du 16 aout (tache WiFi dediee, puis
    // mDNS) : dans les trois cas, une tache supplementaire atterrissait sur le
    // coeur 0 en concurrence avec celle-ci.
    //
    // Le coeur 1 est celui de loopTask, qui y cohabite sans incident depuis des
    // heures. Ce choix supprime la concurrence avec la pile reseau, cause
    // premiere de la lenteur du fetch.
    //
    // Limite assumee : si la famine se reproduisait sur IDLE1, le correctif
    // suivant consisterait a rendre la main explicitement pendant la lecture
    // (enveloppe de flux appelant vTaskDelay quand aucun octet n'est
    // disponible), yield() seul etant insuffisant pour laisser passer IDLE.
    const BaseType_t created = xTaskCreatePinnedToCore(
        fetchTaskEntry,
        "weather-fetch",
        FETCH_TASK_STACK_BYTES,
        this,
        FETCH_TASK_PRIORITY,
        nullptr,
        1
    );

    if (created != pdPASS) {
        portENTER_CRITICAL(&g_weatherMux);
        _fetchInProgress = false;
        portEXIT_CRITICAL(&g_weatherMux);
        return false;
    }

    EventLog::log(LOG_INFO, "Meteo: fetch asynchrone lance");
    return true;
}

void WeatherManager::fetchTaskEntry(void* context) {
    WeatherManager* self = static_cast<WeatherManager*>(context);
    if (self) {
        self->performFetch();
    }
    vTaskDelete(nullptr);
}

void WeatherManager::performFetch() {
    FetchRequest request;

    portENTER_CRITICAL(&g_weatherMux);
    request = _request;
    portEXIT_CRITICAL(&g_weatherMux);

    FetchResult result;
    clearForecast(result.forecast);

    String url;
    if (request.lat != 0.0f || request.lon != 0.0f) {
        url = "http://api.openweathermap.org/data/2.5/forecast?lat="
              + String(request.lat, 4)
              + "&lon=" + String(request.lon, 4)
              + "&appid=" + String(request.apiKey)
              + "&units=" + String(request.units)
              + "&cnt=40";
    } else if (request.city[0] != '\0') {
        url = "http://api.openweathermap.org/data/2.5/forecast?q="
              + String(request.city)
              + "," + String(request.country)
              + "&appid=" + String(request.apiKey)
              + "&units=" + String(request.units)
              + "&cnt=40";
    } else {
        strlcpy(result.error, "ville et coordonnees absentes", sizeof(result.error));
    }

    if (result.error[0] == '\0') {
        HTTPClient http;
        if (!http.begin(url)) {
            strlcpy(result.error, "initialisation HTTP impossible", sizeof(result.error));
        } else {
            http.setTimeout(8000);
            result.httpCode = static_cast<int16_t>(http.GET());

            if (result.httpCode == HTTP_CODE_OK) {
                const int32_t announcedSize = http.getSize();
                const String contentType = http.header("Content-Type");
                result.payloadSize = announcedSize;

                EventLog::log(
                    LOG_INFO,
                    "Meteo: HTTP 200 annonce=%ld lecture=stream type=%s",
                    static_cast<long>(announcedSize),
                    contentType.length() ? contentType.c_str() : "inconnu"
                );

                if (announcedSize == 0) {
                    strlcpy(result.error, "reponse HTTP vide", sizeof(result.error));
                } else {
                    // La reponse OWM complete contient de nombreux champs inutilises.
                    // Sans filtre, ArduinoJson duplique toute la structure en memoire
                    // et peut echouer avec NoMemory malgre une reponse HTTP valide.
                    JsonDocument filter;
                    filter["city"]["timezone"] = true;
                    filter["list"][0]["dt"] = true;
                    filter["list"][0]["main"]["temp"] = true;
                    filter["list"][0]["main"]["feels_like"] = true;
                    filter["list"][0]["main"]["humidity"] = true;
                    filter["list"][0]["main"]["pressure"] = true;
                    filter["list"][0]["clouds"]["all"] = true;
                    filter["list"][0]["wind"]["speed"] = true;
                    filter["list"][0]["wind"]["deg"] = true;
                    filter["list"][0]["wind"]["gust"] = true;
                    filter["list"][0]["pop"] = true;
                    filter["list"][0]["rain"]["3h"] = true;
                    filter["list"][0]["weather"][0]["description"] = true;
                    filter["list"][0]["weather"][0]["icon"] = true;

                    JsonDocument doc;
                    const DeserializationError err = deserializeJson(
                        doc,
                        http.getStream(),
                        DeserializationOption::Filter(filter)
                    );
                    if (err) {
                        snprintf(
                            result.error,
                            sizeof(result.error),
                            "JSON stream: %.40s",
                            err.c_str()
                        );
                    } else {
                        float totalRain = 0.0f;
                        float firstTemp = 0.0f;
                        bool gotTemp = false;

                        time_t nowT;
                        time(&nowT);
                        const uint32_t todayDay =
                            (nowT > 86400) ? static_cast<uint32_t>(nowT / 86400UL) : 0;
                        const bool useEpoch = todayDay > 0;

                        uint8_t entryCount = 0;
                        uint16_t pressureSum[5] = {0, 0, 0, 0, 0};
                        uint8_t pressureCount[5] = {0, 0, 0, 0, 0};
                        uint8_t bestNoonDist[5] = {255, 255, 255, 255, 255};
                        const int32_t cityTzOffset = doc["city"]["timezone"] | 0;

                        for (JsonObject entry : doc["list"].as<JsonArray>()) {
                            float rain = 0.0f;
                            if (entry["rain"].is<JsonObject>()) {
                                rain = entry["rain"]["3h"] | 0.0f;
                            }

                            const float temp = entry["main"]["temp"] | 0.0f;
                            const float feelsLike =
                                entry["main"]["feels_like"] | temp;
                            const uint8_t humidity = constrain(
                                static_cast<int>(entry["main"]["humidity"] | 0),
                                0,
                                100
                            );
                            const uint8_t clouds = constrain(
                                static_cast<int>(entry["clouds"]["all"] | 0),
                                0,
                                100
                            );
                            const uint16_t pressure =
                                static_cast<uint16_t>(entry["main"]["pressure"] | 0);
                            const float windKmh =
                                (entry["wind"]["speed"] | 0.0f) * 3.6f;
                            const int16_t windDeg = entry["wind"]["deg"] | -1;
                            const float gustKmh =
                                (entry["wind"]["gust"] | 0.0f) * 3.6f;
                            const float popRaw = entry["pop"] | 0.0f;
                            const uint8_t popPct = constrain(
                                static_cast<int>(lroundf(popRaw * 100.0f)),
                                0,
                                100
                            );

                            const uint32_t dt = entry["dt"] | static_cast<uint32_t>(0);
                            int8_t offset;
                            if (useEpoch) {
                                const int64_t localEntry =
                                    static_cast<int64_t>(dt) + cityTzOffset;
                                const int64_t localNow =
                                    static_cast<int64_t>(nowT) + cityTzOffset;
                                const int32_t entryDay =
                                    static_cast<int32_t>(localEntry / 86400LL);
                                const int32_t localToday =
                                    static_cast<int32_t>(localNow / 86400LL);
                                offset = static_cast<int8_t>(entryDay - localToday);
                            } else {
                                offset = static_cast<int8_t>(entryCount / 8);
                            }
                            entryCount++;

                            if (offset == 0) totalRain += rain;

                            if (offset >= 0 && offset < 5) {
                                ForecastDay& day = result.forecast[offset];
                                day.rainMm += rain;
                                if (temp > day.tempMax) day.tempMax = temp;
                                if (temp < day.tempMin) day.tempMin = temp;
                                if (feelsLike > day.feelsLikeMax) {
                                    day.feelsLikeMax = feelsLike;
                                }
                                if (windKmh > day.windMaxKmh) {
                                    day.windMaxKmh = windKmh;
                                    day.windDeg = windDeg;
                                }
                                if (gustKmh > day.gustMaxKmh) {
                                    day.gustMaxKmh = gustKmh;
                                }
                                if (popPct > day.rainProbability) {
                                    day.rainProbability = popPct;
                                }
                                if (humidity > day.humidityMax) {
                                    day.humidityMax = humidity;
                                }
                                if (clouds > day.cloudsMax) {
                                    day.cloudsMax = clouds;
                                }
                                if (pressure > 0) {
                                    pressureSum[offset] += pressure;
                                    pressureCount[offset]++;
                                }

                                uint8_t localHour = 12;
                                if (dt > 0) {
                                    const int64_t localEpoch =
                                        static_cast<int64_t>(dt) + cityTzOffset;
                                    localHour = static_cast<uint8_t>(
                                        (localEpoch % 86400LL) / 3600LL
                                    );
                                }

                                const uint8_t noonDist = static_cast<uint8_t>(
                                    abs(static_cast<int>(localHour) - 12)
                                );
                                if (noonDist < bestNoonDist[offset]) {
                                    bestNoonDist[offset] = noonDist;
                                    const char* desc =
                                        entry["weather"][0]["description"] | "";
                                    const char* icon =
                                        entry["weather"][0]["icon"] | "";
                                    strlcpy(
                                        day.description,
                                        desc,
                                        sizeof(day.description)
                                    );
                                    strlcpy(day.icon, icon, sizeof(day.icon));
                                }
                                day.valid = true;
                            }

                            if (!gotTemp) {
                                firstTemp = temp;
                                gotTemp = true;
                            }
                        }

                        for (uint8_t i = 0; i < 5; ++i) {
                            if (pressureCount[i] > 0) {
                                result.forecast[i].pressureAvg =
                                    pressureSum[i] / pressureCount[i];
                            }
                        }

                        result.rainMm = totalRain;
                        result.tempC = firstTemp;
                        result.rainExpected =
                            totalRain >= request.rainThresholdMm;
                        result.success = true;
                    }
                }
            } else {
                snprintf(
                    result.error,
                    sizeof(result.error),
                    "HTTP %d",
                    static_cast<int>(result.httpCode)
                );
            }

            http.end();
        }
    }

    portENTER_CRITICAL(&g_weatherMux);
    _pendingResult = result;
    _resultReady = true;
    _fetchInProgress = false;
    portEXIT_CRITICAL(&g_weatherMux);
}

void WeatherManager::applyPendingResult() {
    if (!_resultReady) return;

    FetchResult result;

    portENTER_CRITICAL(&g_weatherMux);
    result = _pendingResult;
    _resultReady = false;
    portEXIT_CRITICAL(&g_weatherMux);

    const uint32_t now = millis();
    if (!result.success) {
        _nextFetchAt = now + FETCH_RETRY_DELAY_MS;
        EventLog::log(
            LOG_WARN,
            "Meteo: fetch echoue code=%d taille=%ld retry=%lus erreur=%s",
            static_cast<int>(result.httpCode),
            static_cast<long>(result.payloadSize),
            FETCH_RETRY_DELAY_MS / 1000UL,
            result.error[0] ? result.error : "inconnue"
        );
        return;
    }

    for (uint8_t i = 0; i < 5; ++i) {
        _forecast[i] = result.forecast[i];
    }

    _rainMm = result.rainMm;
    _tempC = result.tempC;
    _rainExpected = result.rainExpected;
    _fetched = true;
    _nextFetchAt = now + OWM_CHECK_INTERVAL_MS;

    EventBus::displayDirty = true;

    EventLog::log(
        LOG_INFO,
        "Meteo: fetch termine taille=%ld pluie=%.1fmm temp=%.1fC statut=%s",
        static_cast<long>(result.payloadSize),
        _rainMm,
        _tempC,
        _rainExpected ? "arrosage_bloque" : "ok"
    );
}

bool WeatherManager::isRainExpected() const {
    return _rainExpected;
}

float WeatherManager::getRainMm() const {
    return _rainMm;
}

float WeatherManager::getTempC() const {
    return _tempC;
}

bool WeatherManager::hasFetched() const {
    return _fetched;
}

ForecastDay WeatherManager::getForecastDay(uint8_t offset) const {
    if (offset >= 5) return ForecastDay{};
    return _forecast[offset];
}

String WeatherManager::getStatusStr() const {
    if (_fetchInProgress) return "Mise à jour...";
    if (!_fetched) return "En attente...";

    return _rainExpected
        ? "Pluie " + String(_rainMm, 1) + "mm — bloqué"
        : "OK " + String(_rainMm, 1) + "mm";
}
