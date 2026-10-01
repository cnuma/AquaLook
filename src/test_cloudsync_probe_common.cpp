// Implementation partagee -- voir test_cloudsync_probe_common.h et le
// commentaire en tete de test_cloudsync_probe.cpp pour le contexte complet
// de ce banc (isoler la cause de la panne CloudSync documentee dans
// docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md).

#include "test_cloudsync_probe_common.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "OtaTlsTrust.h"

namespace ProbeCommon {
namespace {

constexpr char SERVER_HOST[] = "aqualook.alwaysdata.net";
constexpr uint16_t SERVER_PORT = 443U;
constexpr uint32_t CONNECT_TIMEOUT_S = 4U;      // identique a CloudSync.cpp
constexpr uint32_t RESPONSE_TIMEOUT_MS = 8000U;
constexpr uint32_t DELAY_BETWEEN_CYCLES_MS = 20000U;  // pas de martelage

uint32_t cycleCount = 0;
uint32_t cycleSuccessCount = 0;   // les DEUX phases ont reussi
uint32_t nextCycleAtMs = 0;
String bearerToken;  // optionnel, saisi en serie, jamais en dur

String readSerialLine(const char* prompt) {
    Serial.print(prompt);
    String line;
    while (true) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\r') continue;
            if (c == '\n') break;
            line += c;
            Serial.print(c);
        }
        delay(5);
    }
    Serial.println();
    return line;
}

bool waitForWifi(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(300);
        Serial.print('.');
    }
    Serial.println();
    return WiFi.status() == WL_CONNECTED;
}

void connectWifi() {
    // WiFi.persistent() n'est jamais desactive dans WiFiManager (verifie) :
    // l'IDF garde donc les derniers identifiants dans SA propre NVS interne,
    // independante de tout ce que ce firmware minimal ne lit pas. Un
    // WiFi.begin() sans argument tente de les reutiliser -- evite de
    // demander le mot de passe en serie a chaque flash sur un module deja
    // appaire (ex. .141), tout en gardant le prompt en repli pour un module
    // neuf/NVS vierge. Constate flaky (a echoue une fois sur deux essais,
    // voir checkpoint) : c'est pour ca que le repli manuel existe.
    Serial.println("[PROBE] tentative de reconnexion avec les identifiants deja enregistres...");
    WiFi.mode(WIFI_STA);
    WiFi.begin();

    if (!waitForWifi(10000U)) {
        Serial.println("[PROBE] pas d'identifiants memorises (ou echec) -- saisie manuelle");
        String ssid = readSerialLine("SSID : ");
        String pass = readSerialLine("Mot de passe (affiche en clair, banc de test) : ");
        Serial.printf("[PROBE] connexion WiFi a \"%s\"...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        if (!waitForWifi(15000U)) {
            Serial.printf("[PROBE] ECHEC WiFi (statut=%d) -- redemarrage dans 5s\n",
                          static_cast<int>(WiFi.status()));
            delay(5000);
            ESP.restart();
        }
    }

    Serial.printf("[PROBE] WiFi OK ip=%s rssi=%d dBm canal=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.channel());
}

// Reproduit fidelement httpExchange() de CloudSync.cpp : memes entetes, meme
// ordre, meme "Connection: close". Une seule difference volontaire : pas de
// lecture/validation du corps de reponse (on s'arrete a la ligne de statut,
// suffisant pour juger succes/echec du round-trip).
bool runPhase(const char* phaseName, const char* method, const char* path,
              const String& body) {
    WiFiClientSecure client;
    OtaTlsTrust::configure(client);
    client.setHandshakeTimeout(CONNECT_TIMEOUT_S);
    client.setTimeout(CONNECT_TIMEOUT_S);

    Serial.printf("[PROBE] [%s] connexion %s:%u...\n", phaseName, SERVER_HOST, SERVER_PORT);
    const uint32_t connectStartMs = millis();
    const bool connected = client.connect(SERVER_HOST, SERVER_PORT);
    const uint32_t connectDurationMs = millis() - connectStartMs;

    if (!connected) {
        char errorBuffer[128] = "";
        const int errorCode = client.lastError(errorBuffer, sizeof(errorBuffer));
        Serial.printf(
            "[PROBE] [%s] RESULTAT=ECHEC phase=tls durationMs=%lu error=%d detail=%s\n",
            phaseName, static_cast<unsigned long>(connectDurationMs), errorCode,
            errorBuffer[0] ? errorBuffer : "inconnu");
        client.stop();
        return false;
    }

    // ── Requete : meme sequence d'entetes que httpExchange() ────────────
    client.print(method);
    client.print(' ');
    client.print(path);
    client.print(" HTTP/1.1\r\nHost: ");
    client.print(SERVER_HOST);
    client.print("\r\nUser-Agent: AquaLook/5.9.7\r\n");
    if (bearerToken.length() > 0U) {
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

    const uint32_t deadlineMs = millis() + RESPONSE_TIMEOUT_MS;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - deadlineMs) < 0) {
        delay(10);
    }

    if (!client.available()) {
        Serial.printf("[PROBE] [%s] RESULTAT=ECHEC phase=reponse connected=%s durationMs=%lu\n",
                      phaseName, client.connected() ? "oui" : "non",
                      static_cast<unsigned long>(millis() - connectStartMs));
        client.stop();
        return false;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    client.stop();

    Serial.printf("[PROBE] [%s] RESULTAT=SUCCES statusLine=\"%s\" totalDurationMs=%lu\n",
                  phaseName, statusLine.c_str(),
                  static_cast<unsigned long>(millis() - connectStartMs));
    return true;
}

void runOneCycle() {
    ++cycleCount;
    Serial.printf(
        "\n[PROBE] === cycle #%lu === heapFree=%lu heapLargestBlock=%lu rssi=%d\n",
        static_cast<unsigned long>(cycleCount),
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        WiFi.RSSI());

    // Phase 1/2, TOUJOURS envoyee par un vrai cycle CloudSync : telemetrie.
    // JSON compose a la main (pas d'ArduinoJson ici : forme fixe et
    // connue, pas besoin d'une dependance de plus dans un banc minimal).
    String diagBody = "{\"type\":\"diag\",\"payload\":{\"firmware\":\"probe\",\"gitSha\":\"probe\",";
    diagBody += "\"uptimeSec\":" + String(millis() / 1000UL);
    diagBody += ",\"heapFree\":" + String(ESP.getFreeHeap());
    diagBody += ",\"heapLargestBlock\":" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    diagBody += ",\"resetReason\":" + String(static_cast<int>(esp_reset_reason()));
    diagBody += "}}";
    const bool reportOk = runPhase("rapport", "POST", "/v1/report", diagBody);

    // Phase 2/2, TOUJOURS envoyee elle aussi : sondage de commande en attente.
    const bool sondageOk = runPhase("sondage", "GET", "/v1/pending-command", "");

    if (reportOk && sondageOk) {
        ++cycleSuccessCount;
    }
    Serial.printf("[PROBE] bilan cycle : rapport=%s sondage=%s -- %lu succes / %lu cycles\n",
                  reportOk ? "ok" : "echec", sondageOk ? "ok" : "echec",
                  static_cast<unsigned long>(cycleSuccessCount),
                  static_cast<unsigned long>(cycleCount));
}

}  // namespace

void initSerialAndToken(const char* banner) {
    Serial.begin(115200);
    uint32_t deadline = millis() + 5000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(banner);
    Serial.printf("MAC : %s\n", WiFi.macAddress().c_str());
}

void begin(const char* banner) {
    initSerialAndToken(banner);
    connectWifi();

    Serial.println(F("Jeton CloudSync (optionnel, Entree pour aucun -- voir commentaire en tete de fichier) :"));
    bearerToken = readSerialLine("Jeton : ");

    nextCycleAtMs = millis();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[PROBE] WiFi deconnecte -- reconnexion...");
        connectWifi();
    }

    if (static_cast<int32_t>(millis() - nextCycleAtMs) >= 0) {
        runOneCycle();
        nextCycleAtMs = millis() + DELAY_BETWEEN_CYCLES_MS;
    }
}

void beginCycleOnly(const char* banner) {
    initSerialAndToken(banner);

    Serial.println(F("Jeton CloudSync (optionnel, Entree pour aucun -- voir commentaire en tete de fichier) :"));
    bearerToken = readSerialLine("Jeton : ");

    nextCycleAtMs = millis();
}

void runCycleIfDue() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (static_cast<int32_t>(millis() - nextCycleAtMs) >= 0) {
        runOneCycle();
        nextCycleAtMs = millis() + DELAY_BETWEEN_CYCLES_MS;
    }
}

}  // namespace ProbeCommon
