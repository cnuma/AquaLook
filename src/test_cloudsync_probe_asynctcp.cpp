// Banc de test isole, ETAPE 2/N : reprend EXACTEMENT le banc de l'etape 1
// (TLS+requete CloudSync reelle, voir test_cloudsync_probe.cpp et
// docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md
// pour le contexte complet)
// et rajoute UN SEUL composant : un serveur ESPAsyncWebServer/AsyncTCP
// minimal, epinglé au coeur 0 comme en production
// (-DCONFIG_ASYNC_TCP_RUNNING_CORE=0, deja dans le [env] partage de
// platformio.ini). Hypothese testee : la contention entre la tache
// AsyncTCP (coeur 0) et la pile WiFi/lwIP (coeur 0 egalement, tache
// interne IDF) pendant une poignee de main TLS CloudSync -- deja la cause
// confirmee du "gel de fond" corrige le 28 sept.
// (docs/engineering/15_RUNTIME_AND_PROFILING.md), donc le candidat le plus
// probable a tester en premier.
//
// Pour que le test soit representatif, generer du trafic LAN reel pendant
// que ce firmware tourne (le serveur seul, sans requete entrante, ne
// reproduirait pas la charge reelle d'un navigateur ouvert sur l'interface
// du module) -- depuis le PC : boucle de requetes HTTP repetees vers
// http://192.168.1.141/ et /api/status pendant la duree du test.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "test_cloudsync_probe_common.h"

namespace {
AsyncWebServer server(80);
uint32_t requestCount = 0;
}  // namespace

void setup() {
    ProbeCommon::begin(
        "=== AquaLook - banc isole ETAPE 2/N : + serveur AsyncTCP/ESPAsyncWebServer ===");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        ++requestCount;
        req->send(200, "text/plain", "ok");
    });
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        ++requestCount;
        req->send(200, "application/json",
                  "{\"probe\":true,\"heapFree\":" + String(ESP.getFreeHeap()) + "}");
    });
    server.begin();
    Serial.println(F("[PROBE] serveur AsyncTCP demarre sur :80 (coeur 0, comme en production)"));
}

void loop() {
    ProbeCommon::loop();

    static uint32_t lastReportMs = 0;
    if (millis() - lastReportMs >= 10000U) {
        lastReportMs = millis();
        Serial.printf("[PROBE] AsyncTCP : %lu requetes HTTP LAN traitees jusqu'ici\n",
                      static_cast<unsigned long>(requestCount));
    }

    delay(50);
}
