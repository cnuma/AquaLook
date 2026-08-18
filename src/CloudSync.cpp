#include "CloudSync.h"

#include <ArduinoJson.h>
#include <cstring>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#include "BootLoopGuard.h"
#include "ConfigManager.h"
#include "EventLog.h"
#include "MaintenanceRequest.h"
#include "OtaTlsTrust.h"
#include "RelaisManager.h"
#include "WiFiManager.h"

#ifndef AQUALOOK_VERSION
#define AQUALOOK_VERSION "unknown"
#endif
#ifndef AQUALOOK_GIT_SHA
#define AQUALOOK_GIT_SHA "unknown"
#endif

namespace {

constexpr char NVS_NAMESPACE[]   = "aq_cloud";
constexpr char KEY_ENABLED[]     = "en";
constexpr char KEY_HOST[]        = "host";
constexpr char KEY_PORT[]        = "port";
constexpr char KEY_HTTPS[]       = "https";
constexpr char KEY_MODULE_ID[]   = "mid";
constexpr char KEY_TOKEN[]       = "tok";
constexpr char KEY_INTERVAL[]    = "min";
constexpr char KEY_LAST_SYNC[]   = "last";

constexpr uint32_t RESPONSE_TIMEOUT_MS = 10000UL;
constexpr uint32_t BLOCKED_LOG_INTERVAL_MS = 3600000UL;  // 1/h, meme raison qu'UpdateCheckScheduler

void copyText(char* destination, size_t destinationSize, const char* source) {
    if (destinationSize == 0U) return;
    std::strncpy(destination, source ? source : "", destinationSize - 1U);
    destination[destinationSize - 1U] = '\0';
}

// Lit une ligne terminee par '\n' en cedant la main au planificateur.
//
// N'utilise JAMAIS readStringUntil()/readString() : Stream::timedRead() du
// coeur Arduino-ESP32 (cores/esp32/Stream.cpp) est une attente active pure,
// sans yield ni delay. Chaque appel qui n'a pas ses octets immediatement
// monopolise le CPU jusqu'a _timeout (1 s par defaut) a la priorite de la
// tache appelante, ce qui prive IDLE0 de CPU et declenche le chien de garde.
// C'est la cause racine des redemarrages du 18 aout 2026 (deux correctifs
// precedents n'avaient traite que la boucle du corps, pas les en-tetes).
bool readLineYielding(Client& client, String& outLine, uint32_t timeoutMs) {
    outLine = "";
    const uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        while (client.available()) {
            const char c = static_cast<char>(client.read());
            if (c == '\n') {
                outLine.trim();
                return true;
            }
            outLine += c;
            if (outLine.length() > 512U) return false;  // en-tete aberrant
        }
        if (!client.connected()) {
            outLine.trim();
            return outLine.length() > 0U;
        }
        delay(1);  // seule garantie que IDLE0 tourne
    }
    return false;
}

// Emet une requete HTTP/1.1 minimale et retourne le code de statut et le
// corps de la reponse. `client` est deja connecte.
//
// Historique du 18 aout 2026 : trois iterations. Les deux premieres ne
// corrigeaient que la boucle de lecture du corps (delay(5) puis lecture
// octet par octet avec delay(1)) et plantaient a l'identique -- chien de
// garde a ~9,2 s, IDLE0 prive de CPU par la tache aqualook-maint. La cause
// reelle etait en amont, dans les readStringUntil() de la ligne de statut
// et des en-tetes : voir readLineYielding() ci-dessus. Toute lecture passe
// desormais par des boucles a delay(1) explicite.
bool httpExchange(Client& client, const char* method, const char* host,
                  const char* path, const char* bearerToken,
                  const String& body, int& outStatus, String& outBody) {
    constexpr size_t MAX_RESPONSE_BYTES = 4096U;  // reponses JSON courtes attendues

    client.print(method);
    client.print(' ');
    client.print(path);
    client.print(" HTTP/1.1\r\nHost: ");
    client.print(host);
    client.print("\r\nUser-Agent: AquaLook/" AQUALOOK_VERSION "\r\n");
    if (bearerToken && bearerToken[0]) {
        client.print("Authorization: Bearer ");
        client.print(bearerToken);
        client.print("\r\n");
    }
    if (body.length() > 0U) {
        client.print("Content-Type: application/json\r\nContent-Length: ");
        client.print(body.length());
        client.print("\r\n");
    }
    client.print("Connection: close\r\n\r\n");
    if (body.length() > 0U) {
        client.print(body);
    }

    EventLog::log(LOG_INFO, "CloudSync: %s %s requete envoyee", method, path);

    const uint32_t deadline = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() && millis() < deadline) {
        delay(10);
    }
    if (!client.available()) {
        EventLog::log(LOG_ERROR, "CloudSync: aucune reponse apres envoi");
        return false;
    }

    String statusLine;
    if (!readLineYielding(client, statusLine, RESPONSE_TIMEOUT_MS)) {
        EventLog::log(LOG_ERROR, "CloudSync: ligne de statut illisible");
        return false;
    }
    if (!statusLine.startsWith("HTTP/1.")) {
        EventLog::log(LOG_ERROR, "CloudSync: statut inattendu");
        return false;
    }
    const int firstSpace = statusLine.indexOf(' ');
    outStatus = firstSpace >= 0 ? statusLine.substring(firstSpace + 1, firstSpace + 4).toInt() : 0;
    EventLog::log(LOG_INFO, "CloudSync: statut http=%d", outStatus);

    // Sauter les en-tetes jusqu'a la ligne vide. Borne en nombre de lignes en
    // plus du delai, pour ne jamais dependre du seul comportement du pair.
    uint8_t headerCount = 0U;
    while (headerCount < 40U) {
        String header;
        if (!readLineYielding(client, header, RESPONSE_TIMEOUT_MS)) break;
        if (header.length() == 0) break;
        ++headerCount;
    }
    EventLog::log(LOG_INFO, "CloudSync: en-tetes lus (%u)", headerCount);

    // Lecture du corps octet par octet, bornee, avec un delay(1)
    // inconditionnel a chaque tour -- voir la note en tete de fonction.
    outBody = "";
    uint32_t lastDataAtMs = millis();
    while (client.connected() || client.available()) {
        while (client.available()) {
            outBody += static_cast<char>(client.read());
            lastDataAtMs = millis();
            if (outBody.length() > MAX_RESPONSE_BYTES) {
                EventLog::log(LOG_WARN, "CloudSync: corps tronque a %u octets",
                              static_cast<unsigned>(MAX_RESPONSE_BYTES));
                return true;
            }
        }
        delay(1);
        if (millis() - lastDataAtMs > RESPONSE_TIMEOUT_MS) break;
    }
    EventLog::log(LOG_INFO, "CloudSync: corps lu (%u octets)",
                  static_cast<unsigned>(outBody.length()));
    return true;
}

// Serialise la configuration effective du module : reglages systeme et
// creneaux des zones actives uniquement (system().nbZones), jamais les
// MAX_ZONES emplacements en capacite.
//
// Les creneaux sont encodes en tableaux [heure, minute, duree, actif]
// plutot qu'en objets nommes. A pleine capacite (16 zones x 8 plannings
// x 5 creneaux = 640 creneaux) la forme nommee depasserait 19 Ko quand la
// forme tableau tient sous 7 Ko ; avec 2 zones actives on reste vers 1 Ko.
// La limite serveur est de 64 Ko (MAX_PAYLOAD_BYTES), donc large, mais le
// tas du module reste la vraie contrainte.
void buildConfigPayload(const ConfigManager& cm, JsonObject payload) {
    payload["schema"] = 1;

    const CfgSystem& sys = cm.system();
    JsonObject system = payload["system"].to<JsonObject>();
    system["nbZones"]          = sys.nbZones;
    system["nbRelais"]         = sys.nbRelaisPhysical;
    system["maxWateringMin"]   = sys.maxWateringMin;
    system["screenTimeoutMin"] = sys.screenTimeoutMin;
    system["ledMode"]          = sys.ledMode;
    system["relayLogic"]       = sys.relayLogic;
    system["relayController"]  = sys.relayController;

    JsonArray zones = payload["zones"].to<JsonArray>();
    const uint8_t activeZones = cm.nbZones();
    for (uint8_t z = 0U; z < activeZones && z < MAX_ZONES; ++z) {
        const CfgZone& src = cm.zone(z);
        JsonObject zone = zones.add<JsonObject>();
        zone["i"]            = z;
        zone["name"]         = src.name;
        zone["mode"]         = src.mode;
        zone["intervalDays"] = src.intervalDays;

        JsonObject rain = zone["rain"].to<JsonObject>();
        rain["thresholdMm"]   = src.rain.thresholdMm;
        rain["forecastHours"] = src.rain.forecastHours;

        JsonArray days = zone["days"].to<JsonArray>();
        for (uint8_t d = 0U; d < NB_DAYS; ++d) {
            JsonArray day = days.add<JsonArray>();
            for (uint8_t s = 0U; s < MAX_SLOTS; ++s) {
                const CfgSlot& slot = src.daySlots[d].slots[s];
                JsonArray entry = day.add<JsonArray>();
                entry.add(slot.hour);
                entry.add(slot.minute);
                entry.add(slot.duration);
                entry.add(slot.enabled ? 1 : 0);
            }
        }

        JsonArray interval = zone["interval"].to<JsonArray>();
        for (uint8_t s = 0U; s < MAX_SLOTS; ++s) {
            const CfgSlot& slot = src.intervalSlots.slots[s];
            JsonArray entry = interval.add<JsonArray>();
            entry.add(slot.hour);
            entry.add(slot.minute);
            entry.add(slot.duration);
            entry.add(slot.enabled ? 1 : 0);
        }
    }
}

}  // namespace

// ═══════════════════════════════════════════════════════════════
//  CloudSync — echange reseau
// ═══════════════════════════════════════════════════════════════

CloudSyncConfig CloudSync::loadConfig() {
    CloudSyncConfig cfg;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return cfg;
    cfg.enabled = prefs.getBool(KEY_ENABLED, false);
    String host = prefs.getString(KEY_HOST, "");
    copyText(cfg.host, sizeof(cfg.host), host.c_str());
    cfg.port = static_cast<uint16_t>(prefs.getUShort(KEY_PORT, 80U));
    cfg.useHttps = prefs.getBool(KEY_HTTPS, false);
    String moduleId = prefs.getString(KEY_MODULE_ID, "");
    copyText(cfg.moduleId, sizeof(cfg.moduleId), moduleId.c_str());
    String token = prefs.getString(KEY_TOKEN, "");
    copyText(cfg.token, sizeof(cfg.token), token.c_str());
    cfg.intervalMinutes = static_cast<uint16_t>(prefs.getUShort(KEY_INTERVAL, 15U));
    prefs.end();
    if (cfg.intervalMinutes == 0U || cfg.intervalMinutes > 1440U) cfg.intervalMinutes = 15U;
    return cfg;
}

CloudSyncResult CloudSync::run(const CloudSyncConfig& cfg,
                               const ConfigManager& configManager) {
    CloudSyncResult result;

    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    Client* client = nullptr;
    if (cfg.useHttps) {
        OtaTlsTrust::configure(secureClient);
        secureClient.setHandshakeTimeout(10U);
        secureClient.setTimeout(RESPONSE_TIMEOUT_MS / 1000U);
        client = &secureClient;
    } else {
        plainClient.setTimeout(RESPONSE_TIMEOUT_MS / 1000U);
        client = &plainClient;
    }

    const uint16_t port = cfg.port != 0U ? cfg.port : (cfg.useHttps ? 443U : 80U);
    EventLog::log(LOG_INFO, "CloudSync: connexion %s:%u...", cfg.host, port);
    const uint32_t connectStartMs = millis();
    if (!client->connect(cfg.host, port)) {
        EventLog::log(LOG_ERROR, "CloudSync: connexion echouee apres %lu ms",
                      static_cast<unsigned long>(millis() - connectStartMs));
        copyText(result.detail, sizeof(result.detail), "connexion impossible");
        return result;
    }
    EventLog::log(LOG_INFO, "CloudSync: connecte en %lu ms",
                  static_cast<unsigned long>(millis() - connectStartMs));

    // ── 1. Telemetrie ────────────────────────────────────────────────────
    {
        JsonDocument doc;
        doc["type"] = "diag";
        JsonObject payload = doc["payload"].to<JsonObject>();
        payload["firmware"] = AQUALOOK_VERSION;
        payload["gitSha"] = AQUALOOK_GIT_SHA;
        payload["uptimeSec"] = millis() / 1000UL;
        payload["heapFree"] = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
        payload["heapLargestBlock"] =
            static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        payload["resetReason"] = static_cast<int>(esp_reset_reason());

        String body;
        serializeJson(doc, body);

        int status = 0;
        String respBody;
        if (!httpExchange(*client, "POST", cfg.host, "/v1/report", cfg.token, body, status, respBody)) {
            copyText(result.detail, sizeof(result.detail), "rapport: pas de reponse");
            client->stop();
            return result;
        }
        result.reportSuccess = (status >= 200 && status < 300);
        if (!result.reportSuccess) {
            char detail[64];
            snprintf(detail, sizeof(detail), "rapport: http=%d", status);
            copyText(result.detail, sizeof(result.detail), detail);
        }
    }

    // ── 2. Configuration effective (miroir cote serveur) ────────────────
    //
    // Envoyee a chaque cycle, sans detection de changement. A 1 Ko par
    // heure le gain d'une empreinte serait negligeable devant la
    // complexite d'un etat supplementaire a maintenir ; a revoir si
    // l'intervalle descend nettement ou si le nombre de zones augmente.
    client->stop();
    if (!client->connect(cfg.host, port)) {
        copyText(result.detail, sizeof(result.detail), "config: connexion impossible");
        result.valid = true;
        return result;
    }
    {
        JsonDocument doc;
        doc["type"] = "config";
        buildConfigPayload(configManager, doc["payload"].to<JsonObject>());

        String body;
        serializeJson(doc, body);
        EventLog::log(LOG_INFO, "CloudSync: config serialisee (%u octets)",
                      static_cast<unsigned>(body.length()));

        int status = 0;
        String respBody;
        if (!httpExchange(*client, "POST", cfg.host, "/v1/report", cfg.token, body, status, respBody)) {
            copyText(result.detail, sizeof(result.detail), "config: pas de reponse");
            client->stop();
            result.valid = true;
            return result;
        }
        result.configSuccess = (status >= 200 && status < 300);
        if (!result.configSuccess) {
            char detail[64];
            snprintf(detail, sizeof(detail), "config: http=%d", status);
            copyText(result.detail, sizeof(result.detail), detail);
        }
    }

    // ── 3. Sondage d'une commande en attente ────────────────────────────
    client->stop();
    if (!client->connect(cfg.host, port)) {
        copyText(result.detail, sizeof(result.detail), "sondage: connexion impossible");
        result.valid = true;
        return result;
    }
    {
        int status = 0;
        String respBody;
        if (!httpExchange(*client, "GET", cfg.host, "/v1/pending-command", cfg.token, "", status, respBody)) {
            copyText(result.detail, sizeof(result.detail), "sondage: pas de reponse");
            client->stop();
            result.valid = true;
            return result;
        }
        if (status >= 200 && status < 300) {
            JsonDocument doc;
            if (deserializeJson(doc, respBody) == DeserializationError::Ok) {
                const char* corr = doc["correlationId"] | (const char*)nullptr;
                if (corr && corr[0]) {
                    result.commandReceived = true;
                    copyText(result.correlationId, sizeof(result.correlationId), corr);
                }
            }
        }
    }

    // ── 4. Accuse reception (sans encore appliquer -- voir CloudSync.h) ─
    if (result.commandReceived) {
        client->stop();
        if (client->connect(cfg.host, port)) {
            JsonDocument doc;
            doc["correlationId"] = result.correlationId;
            doc["state"] = "accepted";
            JsonObject r = doc["result"].to<JsonObject>();
            r["note"] = "recue, application non encore implementee cote firmware";
            String body;
            serializeJson(doc, body);

            int status = 0;
            String respBody;
            if (httpExchange(*client, "POST", cfg.host, "/v1/command/ack", cfg.token, body, status, respBody)) {
                result.ackSuccess = (status >= 200 && status < 300);
            }
        }
    }

    client->stop();
    result.valid = true;
    if (result.reportSuccess) {
        copyText(result.detail, sizeof(result.detail),
                result.commandReceived ? "ok, commande recue" : "ok, rien en attente");
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  CloudSyncScheduler — decide quand redemarrer en maintenance
// ═══════════════════════════════════════════════════════════════

void CloudSyncScheduler::begin() {
    load();
    _loaded = true;
    EventLog::log(LOG_INFO,
                  "CloudSync: %s, intervalle %u min, hote=%s",
                  _cfg.enabled ? "active" : "desactive",
                  static_cast<unsigned>(_cfg.intervalMinutes),
                  _cfg.host[0] ? _cfg.host : "(non configure)");
}

void CloudSyncScheduler::load() {
    _cfg = CloudSync::loadConfig();
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return;
    _lastSyncEpochSec = prefs.getULong(KEY_LAST_SYNC, 0UL);
    prefs.end();
}

void CloudSyncScheduler::save() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "CloudSync: enregistrement du reglage impossible");
        return;
    }
    prefs.putBool(KEY_ENABLED, _cfg.enabled);
    prefs.putString(KEY_HOST, _cfg.host);
    prefs.putUShort(KEY_PORT, _cfg.port);
    prefs.putBool(KEY_HTTPS, _cfg.useHttps);
    prefs.putString(KEY_MODULE_ID, _cfg.moduleId);
    prefs.putString(KEY_TOKEN, _cfg.token);
    prefs.putUShort(KEY_INTERVAL, _cfg.intervalMinutes);
    prefs.end();
}

void CloudSyncScheduler::saveLastSync(uint32_t epochSec) {
    _lastSyncEpochSec = epochSec;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putULong(KEY_LAST_SYNC, epochSec);
    prefs.end();
}

bool CloudSyncScheduler::set(bool enabled, const char* host, uint16_t port, bool useHttps,
                              const char* moduleId, const char* token, uint16_t intervalMinutes) {
    if (intervalMinutes == 0U || intervalMinutes > 1440U) return false;
    if (enabled && (!host || host[0] == '\0')) return false;

    _cfg.enabled = enabled;
    copyText(_cfg.host, sizeof(_cfg.host), host);
    _cfg.port = port;
    _cfg.useHttps = useHttps;
    copyText(_cfg.moduleId, sizeof(_cfg.moduleId), moduleId);
    copyText(_cfg.token, sizeof(_cfg.token), token);
    _cfg.intervalMinutes = intervalMinutes;
    save();
    EventLog::log(LOG_INFO,
                  "CloudSync: reglage change -> %s, hote=%s, intervalle=%u min",
                  enabled ? "active" : "desactive", _cfg.host,
                  static_cast<unsigned>(intervalMinutes));
    return true;
}

void CloudSyncScheduler::logBlocked(const char* reason) {
    const uint32_t nowMs = millis();
    if (nowMs - _blockedLogAtMs < BLOCKED_LOG_INTERVAL_MS) return;
    _blockedLogAtMs = nowMs;
    EventLog::log(LOG_INFO, "CloudSync: echeance atteinte mais report — %s", reason);
}

void CloudSyncScheduler::update(bool ntpSynced,
                                uint32_t epochSec,
                                const WiFiManager* wifi,
                                const RelaisManager* relais,
                                const ConfigManager* config) {
    // Mode degrade : surtout pas de redemarrage volontaire, meme raison
    // qu'UpdateCheckScheduler.
    if (BootLoopGuard::isDegraded()) return;
    if (!_loaded || _triggered || !_cfg.enabled) return;

    const uint32_t nowMs = millis();
    if (wifi == nullptr || !wifi->isConnected()) {
        _wifiConnectedSinceMs = 0U;
    } else if (_wifiConnectedSinceMs == 0U) {
        _wifiConnectedSinceMs = nowMs;
    }

    if (!ntpSynced) return;

    // Premiere execution : ne pas synchroniser immediatement, meme raison
    // qu'UpdateCheckScheduler (eviter un redemarrage surprise a l'instant
    // ou la synchro cloud est activee).
    if (_lastSyncEpochSec == 0U) {
        saveLastSync(epochSec);
        EventLog::log(LOG_INFO, "CloudSync: premiere echeance dans %u min",
                      static_cast<unsigned>(_cfg.intervalMinutes));
        return;
    }

    if (epochSec < _lastSyncEpochSec) {
        // Horloge reculee (correction NTP) : repartir de la date courante.
        saveLastSync(epochSec);
        return;
    }
    if ((epochSec - _lastSyncEpochSec) < static_cast<uint32_t>(_cfg.intervalMinutes) * 60UL) return;

    if (_wifiConnectedSinceMs == 0U) {
        logBlocked("pas de connexion WiFi");
        return;
    }
    if ((nowMs - _wifiConnectedSinceMs) < WIFI_STABLE_MS) {
        logBlocked("connexion WiFi trop recente pour etre jugee stable");
        return;
    }
    if (config != nullptr && relais != nullptr) {
        for (uint8_t zone = 0U; zone < config->nbZones(); ++zone) {
            if (relais->getState(zone)) {
                logBlocked("arrosage en cours");
                return;
            }
        }
    }

    // Enregistre AVANT le redemarrage : un echec ne peut pas relancer une
    // boucle, la prochaine tentative attendra l'echeance suivante.
    saveLastSync(epochSec);

    if (!MaintenanceRequestStore::save(MaintenanceRequest::CLOUD_SYNC)) {
        EventLog::log(LOG_ERROR,
                      "CloudSync: demande de maintenance non enregistree, synchro abandonnee");
        return;
    }

    _triggered = true;
    EventLog::log(LOG_WARN, "CloudSync: echeance atteinte, redemarrage en mode maintenance");
    BootLoopGuard::restartDeliberately("synchronisation cloud");
}
