// Banc de test isole, ETAPE 6/N : reprend le banc CloudSync de base
// (memes phases "rapport"/"sondage" + connexion WiFi simplifiee, voir
// test_cloudsync_probe_common.*, TOUJOURS RAS aux etapes 1 a 5 -- voir
// docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md)
// et rajoute UNE
// REPRODUCTION FIDELE de la sonde de keepalive de la VRAIE classe
// WiFiManager (WiFiManager.cpp, checkKeepaliveReachable()/
// keepaliveProbeTask()) -- PAS la classe WiFiManager entiere (qui aurait
// exige les vrais identifiants WiFi en dur ou en saisie serie manuelle,
// hors de portee d'un banc automatise) : juste CE mecanisme precis, copie
// a l'identique.
//
// Hypothese testee, plus ciblee que "contention generique" (AsyncTCP/SD/
// ecran, etapes 2-5, toutes sur le COEUR 0, toutes RAS) : WiFiManager cree
// une TACHE DEDIEE toutes les 45s, EPINGLEE AU COEUR 1 (KEEPALIVE_PROBE_
// CORE=1, voir WiFiManager.h), qui fait un connect() BLOQUANT (WiFiClient
// simple, port 80, timeout 1s) vers la passerelle -- EXACTEMENT le meme
// coeur que la tache CloudSync (voir commentaires "epinglee au coeur 1"
// dans CloudSync.cpp/WeatherManager.cpp). Deux taches qui font chacune un
// connect() bloquant sur le MEME coeur, a des cadences qui se chevauchent
// tot ou tard (45s vs 30s puis 5min), est un candidat de contention bien
// plus precis que ceux deja ecartes.

#include <Arduino.h>
#include <WiFi.h>

#include "test_cloudsync_probe_common.h"

namespace {

// ── Reproduction fidele de WiFiManager::KEEPALIVE_* (WiFiManager.h),
// SAUF l'intervalle : 7s au lieu de 45s, deliberement raccourci pour ce
// banc -- un test de quelques minutes ne verrait sinon que 5-6 sondes,
// alors que la contention (si elle existe) semble intermittente en
// production ("4 episodes en 3 minutes" une nuit, "3 fois par nuit" une
// autre -- voir checkpoint). Rapprocher les deux taches augmente la
// probabilite de collision SANS changer la nature du mecanisme teste :
// si la contention existe, un rythme plus soutenu ne fait que la revele
// plus vite, il ne l'invente pas.
constexpr uint32_t KEEPALIVE_CHECK_INTERVAL_MS = 7000U;
constexpr uint32_t KEEPALIVE_CHECK_TIMEOUT_MS = 1000U;
constexpr uint16_t KEEPALIVE_CHECK_PORT = 80U;
constexpr uint32_t KEEPALIVE_PROBE_STACK = 4096U;
constexpr UBaseType_t KEEPALIVE_PROBE_PRIORITY = 1U;
constexpr BaseType_t KEEPALIVE_PROBE_CORE = 1;  // MEME coeur que CloudSync

volatile bool probeRunning = false;
volatile bool probeDone = false;
volatile bool probeResult = false;
uint32_t lastKeepaliveCheckMs = 0;
uint32_t keepaliveProbeCount = 0;
uint32_t keepaliveFailCount = 0;

// Reproduction exacte de WiFiManager::keepaliveProbeTask().
void keepaliveProbeTask(void* /*param*/) {
    WiFiClient probe;
    const bool reachable = probe.connect(WiFi.gatewayIP(), KEEPALIVE_CHECK_PORT,
                                         KEEPALIVE_CHECK_TIMEOUT_MS);
    probe.stop();
    probeResult = reachable;
    probeDone = true;
    vTaskDelete(nullptr);
}

// Reproduction exacte de WiFiManager::checkKeepaliveReachable() (sans les
// parties specifiques a l'etat CONNECTED/escalade FaultManager, non
// pertinentes pour ce banc -- seul le DECLENCHEMENT periodique de la
// tache compte ici).
void checkKeepaliveReachable(uint32_t now) {
    if (probeRunning) {
        if (probeDone) {
            probeRunning = false;
            ++keepaliveProbeCount;
            if (!probeResult) {
                ++keepaliveFailCount;
                Serial.printf("[PROBE] keepalive : cible injoignable (%lu/%lu echecs)\n",
                              static_cast<unsigned long>(keepaliveFailCount),
                              static_cast<unsigned long>(keepaliveProbeCount));
            }
        }
        return;
    }

    if (now - lastKeepaliveCheckMs < KEEPALIVE_CHECK_INTERVAL_MS) return;
    lastKeepaliveCheckMs = now;

    if (WiFi.status() != WL_CONNECTED) return;

    probeDone = false;
    probeRunning = true;
    const BaseType_t created = xTaskCreatePinnedToCore(
        keepaliveProbeTask, "wifi-keepalive-probe", KEEPALIVE_PROBE_STACK, nullptr,
        KEEPALIVE_PROBE_PRIORITY, nullptr, KEEPALIVE_PROBE_CORE);
    if (created != pdPASS) {
        probeRunning = false;
        Serial.println(F("[PROBE] keepalive : creation de tache echouee"));
    }
}

}  // namespace

void setup() {
    ProbeCommon::begin(
        "=== AquaLook - banc isole ETAPE 6/N : + sonde keepalive WiFiManager (meme coeur que CloudSync) ===");
    lastKeepaliveCheckMs = millis();
}

void loop() {
    ProbeCommon::loop();
    checkKeepaliveReachable(millis());

    static uint32_t lastReportMs = 0;
    if (millis() - lastReportMs >= 10000U) {
        lastReportMs = millis();
        Serial.printf("[PROBE] keepalive : %lu sondes effectuees, %lu echecs\n",
                      static_cast<unsigned long>(keepaliveProbeCount),
                      static_cast<unsigned long>(keepaliveFailCount));
    }

    delay(50);
}
