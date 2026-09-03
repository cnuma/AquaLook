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
#include "ScheduleManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "NotificationManager.h"
#include "HeapMetrics.h"
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

portMUX_TYPE g_cloudSyncMux = portMUX_INITIALIZER_UNLOCKED;

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
// Decode un corps decoupe en morceaux (RFC 9112 §7.1).
//
// Apache emploie ce decoupage des que la reponse est produite dynamiquement,
// ce qui est le cas de toutes celles de cette API. Le corps arrive alors sous
// la forme :
//
//     1d3<CRLF> {"correlationId":...}<CRLF> 0<CRLF><CRLF>
//
// Sans decodage, deserializeJson() bute sur la taille hexadecimale de tete et
// echoue. Le module concluait alors "aucune commande en attente" -- en
// silence, puisque l'echec d'analyse n'etait pas distingue d'une absence de
// commande. Constate le 2 septembre 2026 : la premiere commande reellement
// emise depuis l'espace utilisateur n'est jamais arrivee, alors que le module
// lisait bien ses 477 octets.
//
// Le defaut ne pouvait pas se voir plus tot : les reponses de /v1/report sont
// ignorees, et /v1/pending-command n'avait jamais rien eu a rendre.
//
// Rend false et laisse le corps intact si le decoupage est illisible : un
// JSON invalide est un symptome plus lisible qu'un corps vide.
bool dechunkBody(String& body) {
    String out;
    out.reserve(body.length());

    int i = 0;
    const int n = static_cast<int>(body.length());
    while (i < n) {
        const int eol = body.indexOf('\n', i);
        if (eol < 0) return false;

        String sizeLine = body.substring(i, eol);
        sizeLine.trim();
        // Extensions eventuelles apres un point-virgule : "1d3;info=x".
        const int semi = sizeLine.indexOf(';');
        if (semi >= 0) sizeLine = sizeLine.substring(0, semi);
        if (sizeLine.length() == 0) return false;

        // Validation explicite : strtol rendrait 0 sur une chaine non
        // hexadecimale, ce qui se confondrait avec le morceau final.
        for (unsigned k = 0U; k < sizeLine.length(); ++k) {
            if (!isxdigit(static_cast<unsigned char>(sizeLine[k]))) return false;
        }
        const long taille = strtol(sizeLine.c_str(), nullptr, 16);
        if (taille < 0) return false;

        i = eol + 1;
        if (taille == 0) {          // morceau final : fin du corps
            body = out;
            return true;
        }
        if (i + static_cast<int>(taille) > n) return false;
        out += body.substring(i, i + static_cast<int>(taille));
        i += static_cast<int>(taille) + 2;   // saute le CRLF de fin de morceau
    }
    // Corps tronque avant le morceau final : ce qui a ete decode reste utile.
    body = out;
    return out.length() > 0U;
}

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

    // Parcourir les en-tetes jusqu'a la ligne vide. Borne en nombre de lignes
    // en plus du delai, pour ne jamais dependre du seul comportement du pair.
    //
    // On y guette le decoupage en morceaux : Apache l'emploie des que la
    // reponse est produite dynamiquement, ce qui est le cas de toutes celles
    // de cette API. Les sauter sans les lire revenait a ignorer cette
    // information -- voir dechunkBody() pour ce que cela coutait.
    uint8_t headerCount = 0U;
    bool chunked = false;
    while (headerCount < 40U) {
        String header;
        if (!readLineYielding(client, header, RESPONSE_TIMEOUT_MS)) break;
        if (header.length() == 0) break;
        String lower = header;
        lower.toLowerCase();
        if (lower.startsWith("transfer-encoding:") && lower.indexOf("chunked") >= 0) {
            chunked = true;
        }
        ++headerCount;
    }
    EventLog::log(LOG_INFO, "CloudSync: en-tetes lus (%u)%s", headerCount,
                  chunked ? ", corps decoupe en morceaux" : "");

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

    if (chunked && !dechunkBody(outBody)) {
        // Corps annonce decoupe mais indechiffrable : le dire, et laisser le
        // brut a l'appelant. Une analyse JSON qui echoue ensuite est un
        // symptome bien plus lisible qu'un corps vide.
        EventLog::log(LOG_ERROR, "CloudSync: decoupage en morceaux illisible");
    }
    return true;
}

// Le reste des reglages : tout ce qui ne concerne pas l'arrosage mais qu'il
// faudrait ressaisir a la main sur un module de remplacement.
//
// Ce que ce bloc ne contient PAS, deliberement : le mot de passe WiFi, la clef
// OpenWeatherMap et le jeton ntfy. Un secret recopie dans une base distante
// devient lisible par quiconque accede a cette base ou a ses sauvegardes, et
// l'utilisateur ne peut plus savoir ou il se trouve. Les trois se ressaisissent
// une fois au remplacement -- le WiFi par le portail captif de toute facon
// obligatoire pour joindre le reseau. Le SSID est absent pour la meme raison :
// le portail scanne et propose la liste, le stocker ne ferait qu'associer le
// compte a un reseau physique sans rien faire gagner.
//
// Ces reglages sont ranges a part des zones : l'editeur en ligne ne travaille
// que sur payload.zones et payload.system, il ignore cette section sans avoir
// a la connaitre.
void buildSettingsPayload(const ConfigManager& cm, JsonObject settings) {
    const CfgNtp& ntp = cm.ntp();
    JsonObject jntp = settings["ntp"].to<JsonObject>();
    jntp["server"]    = ntp.server;
    jntp["gmtOffset"] = ntp.gmtOffset;
    jntp["dstOffset"] = ntp.dstOffset;

    // Sans la clef : la localisation seule ne vaut rien pour un tiers, la clef si.
    const CfgOwm& owm = cm.owm();
    JsonObject jowm = settings["owm"].to<JsonObject>();
    jowm["lat"]     = owm.lat;
    jowm["lon"]     = owm.lon;
    jowm["units"]   = owm.units;
    jowm["city"]    = owm.city;
    jowm["country"] = owm.country;
    // Dit si une clef est en place, sans jamais la reveler : sans cela, une
    // restauration laisserait croire la meteo fonctionnelle alors qu'il manque
    // la seule chose que la sauvegarde ne pouvait pas rapporter.
    jowm["apiKeySet"] = owm.apiKey[0] != '\0';

    settings["manualDurationMin"] = cm.manual().durationMin;

    const CfgWindAlert& wind = cm.windAlert();
    JsonObject jwind = settings["wind"].to<JsonObject>();
    jwind["gustKmh"]   = wind.gustKmh;
    jwind["severeKmh"] = wind.severeKmh;

    // Propre a la dalle montee, pas au jardin : conservee parce qu'un module
    // repare avec le meme ecran repart sans recalibrage.
    const CfgTouch& touch = cm.touch();
    JsonObject jtouch = settings["touch"].to<JsonObject>();
    jtouch["xMin"] = touch.xMin;
    jtouch["xMax"] = touch.xMax;
    jtouch["yMin"] = touch.yMin;
    jtouch["yMax"] = touch.yMax;

    const CfgDisplay& d = cm.display();
    JsonObject jd = settings["display"].to<JsonObject>();
    jd["cBg"]       = d.cBg;
    jd["cSurface"]  = d.cSurface;
    jd["cSurface2"] = d.cSurface2;
    jd["cBorder"]   = d.cBorder;
    jd["cText"]     = d.cText;
    jd["cText2"]    = d.cText2;
    jd["cMuted"]    = d.cMuted;
    jd["cActiveBg"] = d.cActiveBg;
    jd["cZone0"]    = d.cZone0;
    jd["cZone1"]    = d.cZone1;
    jd["cZone2"]    = d.cZone2;
    jd["cZone3"]    = d.cZone3;
    jd["rSm"]        = d.rSm;
    jd["rMd"]        = d.rMd;
    jd["rLg"]        = d.rLg;
    jd["accentBarW"] = d.accentBarW;
    jd["refreshNomMs"] = d.refreshNomMs;
    jd["refreshActMs"] = d.refreshActMs;
    jd["planGap"] = d.planGap;
    jd["g2Gpad"]  = d.g2Gpad;
    jd["g4Gpad"]  = d.g4Gpad;
    jd["showWeatherIcon"] = d.showWeatherIcon;
    jd["showWeatherTemp"] = d.showWeatherTemp;
    JsonObject jtips = jd["weatherTips"].to<JsonObject>();
    jtips["condition"] = d.weatherTipCondition;
    jtips["temp"]      = d.weatherTipTemp;
    jtips["rain"]      = d.weatherTipRain;
    jtips["pop"]       = d.weatherTipPop;
    jtips["humidity"]  = d.weatherTipHumidity;
    jtips["wind"]      = d.weatherTipWind;
    jtips["gust"]      = d.weatherTipGust;
    jtips["clouds"]    = d.weatherTipClouds;
    jtips["pressure"]  = d.weatherTipPressure;
}
// Serialise la configuration effective du module : reglages systeme et
// creneaux des zones actives uniquement (system().nbZones), jamais les
// MAX_ZONES emplacements en capacite.
//
// Les creneaux sont encodes en tableaux [heure, minute, duree, actif]
// plutot qu'en objets nommes. A pleine capacite (16 zones x 8 plannings
// x 5 creneaux = 640 creneaux) la forme nommee depasserait 19 Ko quand la
// forme tableau tient sous 7 Ko. Mesure le 3 septembre 2026 sur le module
// d'essai, 4 zones actives, section settings comprise : 3163 octets.
// La limite serveur est de 64 Ko (MAX_PAYLOAD_BYTES), donc large, mais le
// tas du module reste la vraie contrainte : ce corps est conserve en String
// pendant toute la duree de la synchronisation.
void buildConfigPayload(const ConfigManager& cm, JsonObject payload) {
    payload["schema"] = 2;
    // Version de la configuration : c'est sur elle que le serveur s'appuie
    // pour proposer une modification, et c'est elle que le module compare a
    // baseRevision avant d'appliquer quoi que ce soit
    // (docs/architecture/CLOUD_REMOTE_CONFIG.md).
    payload["revision"] = cm.configRevision();

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

    buildSettingsPayload(cm, payload["settings"].to<JsonObject>());
}

}  // namespace

// ═══════════════════════════════════════════════════════════════
//  CloudSync — echange reseau
// ═══════════════════════════════════════════════════════════════

// Adresse d'un reseau prive (RFC 1918) ou bouclage ?
//
// Sert a distinguer un serveur d'etabli d'un serveur joignable depuis
// Internet. La reconnaissance est volontairement litterale, sur la forme
// pointee : un nom d'hote n'est PAS considere comme prive, meme s'il resout
// vers une adresse privee. Une resolution DNS peut changer sous nos pieds,
// et on ne va pas fonder une decision de securite dessus.
static bool isPrivateAddress(const char* host) {
    if (!host || !host[0]) return false;
    unsigned a = 0U, b = 0U, c = 0U, d = 0U;
    if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    if (a > 255U || b > 255U || c > 255U || d > 255U) return false;
    if (a == 127U) return true;                      // 127.0.0.0/8
    if (a == 10U) return true;                       // 10.0.0.0/8
    if (a == 192U && b == 168U) return true;         // 192.168.0.0/16
    if (a == 172U && b >= 16U && b <= 31U) return true;  // 172.16.0.0/12
    return false;
}

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

    // Relecture defensive, meme regle qu'a l'ecriture : une configuration
    // enregistree AVANT ce garde - ou modifiee hors de set() - ne doit pas
    // pouvoir faire circuler le jeton en clair vers Internet. On desactive la
    // synchronisation plutot que de basculer silencieusement en HTTPS : le
    // serveur n'ecoute peut-etre pas en TLS, et echouer bruyamment vaut mieux
    // que paraitre fonctionner.
    if (cfg.enabled && !cfg.useHttps && !isPrivateAddress(cfg.host)) {
        EventLog::log(LOG_ERROR,
                      "CloudSync: desactive - configuration HTTP en clair vers %s, "
                      "HTTPS obligatoire hors reseau local",
                      cfg.host);
        cfg.enabled = false;
    }
    return cfg;
}

String CloudSync::buildConfigBody(const ConfigManager& configManager) {
    JsonDocument doc;
    doc["type"] = "config";
    buildConfigPayload(configManager, doc["payload"].to<JsonObject>());
    String body;
    serializeJson(doc, body);
    return body;
}

CloudSyncResult CloudSync::run(const CloudSyncConfig& cfg,
                               const String& configBody,
                               const CloudSyncPendingAck& pendingAck) {
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
        payload["heapFree"] = static_cast<uint32_t>(AquaLook::Heap::freeBytes());
        payload["heapLargestBlock"] =
            static_cast<uint32_t>(AquaLook::Heap::largestFreeBlock());
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
        EventLog::log(LOG_INFO, "CloudSync: config serialisee (%u octets)",
                      static_cast<unsigned>(configBody.length()));

        int status = 0;
        String respBody;
        if (!httpExchange(*client, "POST", cfg.host, "/v1/report", cfg.token, configBody, status, respBody)) {
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

                    // Recopier la commande pour la boucle principale, SAUF si
                    // c'est celle qu'elle vient justement de traiter : le
                    // serveur la represente tant qu'elle n'est pas reglee, et
                    // l'appliquer deux fois serait une faute.
                    const bool alreadyHandled =
                        pendingAck.correlationId[0] &&
                        strcmp(pendingAck.correlationId, corr) == 0;
                    if (!alreadyHandled) {
                        JsonVariantConst cmd = doc["command"];
                        if (!cmd.isNull()) {
                            const size_t needed = measureJson(cmd) + 1U;
                            char* buf = static_cast<char*>(malloc(needed));
                            if (buf) {
                                serializeJson(cmd, buf, needed);
                                result.commandJson = buf;
                            } else {
                                EventLog::log(LOG_ERROR,
                                    "CloudSync: commande non recopiee, memoire insuffisante");
                            }
                        }
                    }
                }
            }
        }
    }

    // ── 4. Accuse reception ────────────────────────────────────────────
    //
    // N'accuse QUE la commande deja traitee par la boucle principale : c'est
    // elle qui applique, jamais cette tache. Une commande fraichement recue
    // repart donc sans accuse et sera acquittee au cycle suivant, declenche
    // immediatement apres son application (voir _ackSyncSoon).
    const bool ackReady =
        pendingAck.correlationId[0] &&
        strcmp(pendingAck.correlationId, result.correlationId) == 0;

    if (result.commandReceived && ackReady) {
        client->stop();
        if (client->connect(cfg.host, port)) {
            JsonDocument doc;
            doc["correlationId"] = result.correlationId;
            doc["state"] = pendingAck.state[0] ? pendingAck.state : "accepted";
            JsonObject r = doc["result"].to<JsonObject>();
            if (pendingAck.detail[0]) r["detail"] = pendingAck.detail;
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

    // HTTPS impose des que le serveur n'est pas sur le reseau local.
    //
    // Le jeton porteur est la SEULE preuve d'identite du module. En clair, il
    // est capturable par quiconque ecoute la ligne, et rejouable ensuite pour
    // se faire passer pour lui. Hacher les jetons cote serveur protege la
    // base de donnees, pas la ligne : c'est le transport qui decide.
    //
    // L'exception au reseau prive est deliberee et bornee : elle laisse
    // travailler avec un serveur d'etabli en HTTP, sans jamais autoriser le
    // clair vers Internet - le seul cas ou l'ecoute est realiste. Elle est
    // journalisee a chaque reglage, pour qu'elle ne s'oublie pas en service.
    if (enabled && !useHttps) {
        if (!isPrivateAddress(host)) {
            EventLog::log(LOG_ERROR,
                          "CloudSync: HTTP refuse vers %s - HTTPS obligatoire hors reseau local",
                          host);
            return false;
        }
        EventLog::log(LOG_WARN,
                      "CloudSync: HTTP en clair tolere vers %s (reseau local) - "
                      "le jeton circule en clair, a ne pas conserver en service",
                      host);
    }

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
    // Recupere d'abord le resultat d'une synchro terminee : c'est la boucle
    // principale qui journalise et libere la memoire, jamais la tache.
    applyPendingResult();

    if (BootLoopGuard::isDegraded()) return;
    if (!_loaded || _triggered || !_cfg.enabled) return;
    if (_syncInProgress) return;

    const uint32_t nowMs = millis();
    // Report apres manque de memoire : ne pas reessayer en continu.
    if (_deferUntilMs != 0U) {
        if (nowMs < _deferUntilMs) return;
        _deferUntilMs = 0U;
    }
    if (wifi == nullptr || !wifi->isConnected()) {
        _wifiConnectedSinceMs = 0U;
    } else if (_wifiConnectedSinceMs == 0U) {
        _wifiConnectedSinceMs = nowMs;
    }

    if (!ntpSynced) return;

    // Un accuse est du : ne pas faire attendre le serveur un intervalle
    // complet pour apprendre le sort de sa commande.
    if (_ackSyncSoon && _lastSyncEpochSec != 0U) {
        _lastSyncEpochSec = 0U;
        _ackSyncSoon = false;
    }

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

    if (config == nullptr) {
        logBlocked("configuration indisponible");
        return;
    }

    // Lancement en tache dediee, sans redemarrage. Si la memoire manque,
    // startSync() reporte et n'enregistre rien : la prochaine tentative aura
    // lieu apres RETRY_ON_LOW_MEMORY_MS, sans perdre l'echeance.
    if (!startSync(*config)) return;

    // Enregistre seulement une fois la tache lancee : un report memoire ne
    // doit pas consommer l'echeance.
    saveLastSync(epochSec);
}

// Lance la synchronisation dans une tache dediee. Retourne false si la
// memoire est insuffisante ou si la tache n'a pas pu etre creee.
bool CloudSyncScheduler::startSync(const ConfigManager& configManager) {
    if (_syncInProgress) return false;

    const uint32_t freeBytes =
        static_cast<uint32_t>(AquaLook::Heap::freeBytes());
    const uint32_t largestBlock =
        static_cast<uint32_t>(AquaLook::Heap::largestFreeBlock());
    if (freeBytes < MIN_FREE_FOR_SYNC || largestBlock < MIN_BLOCK_FOR_SYNC) {
        _deferUntilMs = millis() + RETRY_ON_LOW_MEMORY_MS;
        EventLog::log(LOG_WARN,
                      "CloudSync: reporte, memoire libre=%lu bloc=%lu",
                      static_cast<unsigned long>(freeBytes),
                      static_cast<unsigned long>(largestBlock));
        return false;
    }

    // Serialise ICI, dans la boucle principale : voir CloudSync::buildConfigBody.
    _taskConfigBody = CloudSync::buildConfigBody(configManager);
    _taskCfg = _cfg;

    portENTER_CRITICAL(&g_cloudSyncMux);
    _pendingResult = CloudSyncResult{};
    _syncInProgress = true;
    _resultReady = false;
    portEXIT_CRITICAL(&g_cloudSyncMux);

    // Epinglee au coeur 1, jamais laissee libre.
    //
    // Meme motif que WeatherManager : sans affinite, l'ordonnanceur peut
    // placer la tache sur le coeur 0, ou tournent la pile WiFi et lwIP. Elle
    // y prive IDLE0 de CPU et le chien de garde abat le systeme. C'est la
    // meme famille de panne que celle corrigee dans httpExchange() le
    // 18 aout 2026 (lectures en attente active), abordee cette fois par
    // l'autre bout : ne pas concurrencer la pile reseau sur son propre coeur.
    const BaseType_t created = xTaskCreatePinnedToCore(
        syncTaskEntry,
        "cloud-sync",
        SYNC_TASK_STACK_BYTES,
        this,
        SYNC_TASK_PRIORITY,
        nullptr,
        1
    );

    if (created != pdPASS) {
        portENTER_CRITICAL(&g_cloudSyncMux);
        _syncInProgress = false;
        portEXIT_CRITICAL(&g_cloudSyncMux);
        _taskConfigBody = String();
        EventLog::log(LOG_ERROR, "CloudSync: creation de la tache impossible");
        return false;
    }

    EventLog::log(LOG_INFO, "CloudSync: synchro asynchrone lancee");
    return true;
}

void CloudSyncScheduler::syncTaskEntry(void* context) {
    CloudSyncScheduler* self = static_cast<CloudSyncScheduler*>(context);
    if (self) {
        self->performSync();
    }
    vTaskDelete(nullptr);
}

void CloudSyncScheduler::performSync() {
    const CloudSyncResult result = CloudSync::run(_taskCfg, _taskConfigBody, _pendingAck);

    portENTER_CRITICAL(&g_cloudSyncMux);
    _pendingResult = result;   // POD : copie sure en section critique
    _resultReady = true;
    _syncInProgress = false;
    portEXIT_CRITICAL(&g_cloudSyncMux);
}

// ═══════════════════════════════════════════════════════════════
//  Application d'une commande de configuration
//
//  Appelee UNIQUEMENT depuis la boucle principale : elle ecrit en NVS et
//  modifie l'etat que lit l'affichage. La tache de synchronisation n'a le
//  droit ni de l'un ni de l'autre.
//
//  Regles, arretees avec l'utilisateur le 29 aout 2026
//  (docs/architecture/CLOUD_REMOTE_CONFIG.md) :
//    - PERIMETRE : configuration seulement. Demarrer un arrosage, changer
//      les identifiants WiFi ou declencher une mise a jour sont refuses,
//      quelle que soit la commande.
//    - CONFLIT : le local gagne toujours. Verrouillage optimiste sur
//      baseRevision - si la version a bouge depuis que le serveur a lu la
//      configuration, la commande est refusee avec son motif.
//    - FORMAT : partiel. Seuls les champs presents sont appliques, ce qui
//      empeche une commande tronquee d'effacer ce qu'elle ne mentionne pas.
// ═══════════════════════════════════════════════════════════════
void CloudSyncScheduler::applyCommand(const char* json, const char* correlationId) {
    copyText(_pendingAck.correlationId, sizeof(_pendingAck.correlationId), correlationId);
    copyText(_pendingAck.state, sizeof(_pendingAck.state), "refused");

    if (!_configTarget) {
        copyText(_pendingAck.detail, sizeof(_pendingAck.detail),
                 "configuration indisponible cote module");
        return;
    }

    JsonDocument cmd;
    if (deserializeJson(cmd, json) != DeserializationError::Ok) {
        copyText(_pendingAck.detail, sizeof(_pendingAck.detail), "json-illisible");
        return;
    }

    const char* type = cmd["type"] | "";
    if (strcmp(type, "config.apply") != 0) {
        // Refus explicite plutot que silencieux : un type inconnu peut etre
        // une commande d'action deguisee, ou un contrat plus recent que ce
        // firmware. Dans les deux cas, ne rien faire et le dire.
        snprintf(_pendingAck.detail, sizeof(_pendingAck.detail),
                 "type-non-supporte: %.40s", type[0] ? type : "(absent)");
        return;
    }

    // ── Verrouillage optimiste ────────────────────────────────────────
    if (!cmd["baseRevision"].is<uint32_t>()) {
        // Sans base de comparaison, appliquer reviendrait a ecrire a
        // l'aveugle - exactement ce que la regle "le local gagne" interdit.
        copyText(_pendingAck.detail, sizeof(_pendingAck.detail),
                 "baseRevision-absente");
        return;
    }
    const uint32_t base = cmd["baseRevision"].as<uint32_t>();
    const uint32_t current = _configTarget->configRevision();
    if (base != current) {
        snprintf(_pendingAck.detail, sizeof(_pendingAck.detail),
                 "config-modifiee-localement (attendu=%lu courant=%lu)",
                 (unsigned long)base, (unsigned long)current);
        EventLog::log(LOG_WARN,
                      "CloudSync: commande refusee, config modifiee localement "
                      "(base=%lu courant=%lu)",
                      (unsigned long)base, (unsigned long)current);
        // Prevenir : c'est le seul cas ou une demande faite depuis l'espace en
        // ligne ne produit RIEN, et ou l'utilisateur peut etre ailleurs.
        NotificationManager::enqueueRemoteConfig(false, 0U, current, _pendingAck.detail);
        return;
    }

    // ── Application partielle ─────────────────────────────────────────
    uint8_t applied = 0U;

    JsonVariantConst sys = cmd["system"];
    if (sys.is<JsonObjectConst>()) {
        if (sys["screenTimeoutMin"].is<uint8_t>()) {
            _configTarget->setSystemScreenTimeout(sys["screenTimeoutMin"].as<uint8_t>());
            applied++;
        }
        if (sys["maxWateringMin"].is<uint16_t>()) {
            _configTarget->setSystemMaxWatering(sys["maxWateringMin"].as<uint16_t>());
            applied++;
        }
        if (sys["ledMode"].is<uint8_t>()) {
            _configTarget->setSystemLedMode(sys["ledMode"].as<uint8_t>());
            applied++;
        }
        // nbZones, nbRelais et relayLogic ne sont volontairement PAS
        // applicables a distance : ils engagent le cablage physique, et une
        // valeur fausse ferait commuter les mauvaises vannes.
    }

    JsonVariantConst wind = cmd["windAlert"];
    if (wind.is<JsonObjectConst>()) {
        CfgWindAlert w = _configTarget->windAlert();
        if (wind["gustKmh"].is<uint8_t>())   { w.gustKmh   = wind["gustKmh"].as<uint8_t>();   applied++; }
        if (wind["severeKmh"].is<uint8_t>()) { w.severeKmh = wind["severeKmh"].as<uint8_t>(); applied++; }
        if (applied) _configTarget->setWindAlert(w);
    }

    // Duree maximale autorisee : un creneau distant ne doit jamais pouvoir
    // depasser la limite que l'utilisateur a fixee localement. Sans ce
    // plafond, une commande pourrait programmer un arrosage de 24 h.
    const uint16_t maxDurationMin = _configTarget->system().maxWateringMin;

    JsonArrayConst zones = cmd["zones"];
    if (!zones.isNull()) {
        for (JsonObjectConst z : zones) {
            if (!z["i"].is<uint8_t>()) continue;
            const uint8_t idx = z["i"].as<uint8_t>();
            if (idx >= MAX_ZONES) continue;

            if (z["name"].is<const char*>()) {
                _configTarget->setZoneName(idx, z["name"].as<const char*>());
                applied++;
            }
            if (z["mode"].is<uint8_t>()) {
                const uint8_t mode = z["mode"].as<uint8_t>();
                if (mode <= SCHEDULE_MODE_INTERVAL) {
                    _configTarget->setZoneMode(idx, mode);
                    if (_scheduleTarget) _scheduleTarget->setMode(idx, mode);
                    applied++;
                }
            }
            if (z["intervalDays"].is<uint8_t>()) {
                const uint8_t days = z["intervalDays"].as<uint8_t>();
                if (days >= 1U && days <= 30U) {
                    _configTarget->setZoneIntervalDays(idx, days);
                    if (_scheduleTarget) _scheduleTarget->setIntervalDays(idx, days);
                    applied++;
                }
            }
            // Seuil de pluie : les deux orthographes sont acceptees.
            //
            // buildConfigPayload() REMONTE "thresholdMm" et "forecastHours",
            // alors que ce bloc n'attendait que "threshMm" et "hours". Une
            // interface qui relit la configuration du module et la renvoie
            // telle quelle perdait donc silencieusement les seuils de pluie :
            // les clefs ne correspondaient pas, le bloc etait ignore, et
            // applied ne bougeait pas -- l'accuse annoncait un succes partiel
            // sans dire ce qui avait ete laisse de cote.
            //
            // Les noms remontes font foi ; les courts restent acceptes pour ne
            // pas casser les commandes deja ecrites a la main.
            JsonVariantConst rain = z["rain"];
            if (rain.is<JsonObjectConst>()) {
                JsonVariantConst vThresh = rain["thresholdMm"].isNull()
                                         ? rain["threshMm"] : rain["thresholdMm"];
                JsonVariantConst vHours  = rain["forecastHours"].isNull()
                                         ? rain["hours"] : rain["forecastHours"];
                if (vThresh.is<float>() && vHours.is<uint8_t>()) {
                    const float thresh = vThresh.as<float>();
                    const uint8_t hours = vHours.as<uint8_t>();
                    if (thresh >= 0.0f && thresh <= 100.0f && hours <= MAX_FORECAST_HOURS) {
                        _configTarget->setZoneRain(idx, thresh, hours);
                        if (_scheduleTarget) _scheduleTarget->setRainConfig(idx, thresh, hours);
                        applied++;
                    }
                }
            }

            // ── Creneaux par jour fixe ────────────────────────────────
            JsonArrayConst daySlots = z["daySlots"];
            if (!daySlots.isNull()) {
                for (JsonObjectConst sl : daySlots) {
                    const uint8_t day  = sl["day"]  | 255U;
                    const uint8_t slot = sl["slot"] | 255U;
                    const uint8_t h    = sl["h"]    | 255U;
                    const uint8_t m    = sl["m"]    | 255U;
                    const uint16_t dur = sl["dur"]  | 0U;
                    const bool on      = sl["on"]   | false;
                    if (day >= NB_DAYS || slot >= MAX_SLOTS) continue;
                    if (h >= 24U || m >= 60U) continue;
                    if (dur == 0U || dur > maxDurationMin) continue;
                    _configTarget->setZoneDaySlot(idx, day, slot, h, m, dur, on);
                    if (_scheduleTarget) {
                        _scheduleTarget->setDaySlot(idx, day, slot, h, m, dur, on);
                    }
                    applied++;
                }
            }

            // ── Creneaux du mode intervalle ───────────────────────────
            JsonArrayConst intSlots = z["intervalSlots"];
            if (!intSlots.isNull()) {
                for (JsonObjectConst sl : intSlots) {
                    const uint8_t slot = sl["slot"] | 255U;
                    const uint8_t h    = sl["h"]    | 255U;
                    const uint8_t m    = sl["m"]    | 255U;
                    const uint16_t dur = sl["dur"]  | 0U;
                    const bool on      = sl["on"]   | false;
                    if (slot >= MAX_SLOTS) continue;
                    if (h >= 24U || m >= 60U) continue;
                    if (dur == 0U || dur > maxDurationMin) continue;
                    _configTarget->setZoneIntervalSlot(idx, slot, h, m, dur, on);
                    if (_scheduleTarget) {
                        _scheduleTarget->setIntervalSlot(idx, slot, h, m, dur, on);
                    }
                    applied++;
                }
            }
        }
    }

    if (applied > 0U) EventBus::displayDirty = true;

    if (applied == 0U) {
        copyText(_pendingAck.detail, sizeof(_pendingAck.detail),
                 "aucun champ applicable dans la commande");
        return;
    }

    copyText(_pendingAck.state, sizeof(_pendingAck.state), "accepted");
    snprintf(_pendingAck.detail, sizeof(_pendingAck.detail),
             "%u champ(s) applique(s), revision=%lu",
             (unsigned)applied, (unsigned long)_configTarget->configRevision());
    EventLog::log(LOG_INFO, "CloudSync: commande appliquee, %u champ(s), revision=%lu",
                  (unsigned)applied, (unsigned long)_configTarget->configRevision());
    // Une configuration qui change sans que personne n'ait touche au module
    // merite d'etre annoncee : c'est le seul evenement de ce genre.
    NotificationManager::enqueueRemoteConfig(true, applied,
                                             _configTarget->configRevision(), "");
}

void CloudSyncScheduler::applyPendingResult() {
    if (!_resultReady) return;

    CloudSyncResult result;
    portENTER_CRITICAL(&g_cloudSyncMux);
    result = _pendingResult;
    _resultReady = false;
    portEXIT_CRITICAL(&g_cloudSyncMux);

    // Libere le corps serialise (~1,3 Ko) des que la tache n'en a plus besoin.
    _taskConfigBody = String();

    const bool ok = result.valid && result.reportSuccess && result.configSuccess;
    EventLog::log(ok ? LOG_INFO : LOG_WARN,
                  "CloudSync: cycle rapport=%s config=%s cmd=%s",
                  result.reportSuccess ? "ok" : "echec",
                  result.configSuccess ? "ok" : "echec",
                  // "accuse-differe" et non "echec" : une commande fraichement
                  // recue repart TOUJOURS sans accuse, par conception -- c'est
                  // la boucle principale qui l'applique, et l'accuse part au
                  // cycle suivant. Journaliser cela comme un echec fait
                  // soupconner une panne la ou tout se passe comme prevu ;
                  // constate le 2 septembre 2026 en suivant la premiere
                  // commande reelle, ou la ligne disait "cmd=echec" alors que
                  // la commande venait d'etre appliquee avec succes.
                  result.commandReceived
                      ? (result.ackSuccess ? "accuse-ok" : "accuse-differe")
                      : "aucune");
    if (!ok && result.detail[0]) {
        EventLog::log(LOG_WARN, "CloudSync: detail %s", result.detail);
    }

    // Commande fraichement recue : c'est ICI qu'elle est appliquee, dans la
    // boucle principale, jamais dans la tache. Le tampon appartient
    // desormais a cette fonction, qui doit le liberer dans tous les cas.
    if (result.commandJson) {
        applyCommand(result.commandJson, result.correlationId);
        free(result.commandJson);
        result.commandJson = nullptr;
        // Le serveur attend son accuse : declencher le cycle suivant tout de
        // suite plutot que de le laisser patienter l'intervalle complet.
        _ackSyncSoon = true;
    } else if (result.commandReceived && result.ackSuccess) {
        // Accuse parti : la commande est reglee cote serveur, oublier son
        // identifiant pour ne pas le comparer indefiniment.
        _pendingAck = CloudSyncPendingAck{};
        _ackSyncSoon = false;
    }
}
