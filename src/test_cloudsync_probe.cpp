// Banc de test isole, ETAPE 1/N : valide UNIQUEMENT la connexion reseau
// (WiFi + TLS) vers le serveur CloudSync (aqualook.alwaysdata.net:443),
// sans AUCUN autre composant de l'application (pas de ConfigManager,
// WiFiManager, ecran, relais, planificateur, AsyncTCP...). But : reproduire
// la panne CloudSync documentee dans
// docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md
// dans un contexte totalement depourvu de contention pour savoir si le probleme suit le module (meme
// IP domestique, meme pile mbedTLS) ou disparait hors du contexte
// applicatif complet.
//
// RESULTAT de cette etape (30 sept. 2026) : 8/8 puis 5/5 cycles (requete
// fidele a CloudSync.cpp) tous reussis -- voir le checkpoint. Ecarte le
// blocage externe (IP/materiel) ET le blocage par contenu de requete.
//
// Etape suivante : voir test_cloudsync_probe_asynctcp.cpp, qui reprend
// EXACTEMENT la meme logique (factorisee dans test_cloudsync_probe_common.*)
// et rajoute un serveur web ESPAsyncWebServer/AsyncTCP a cote, pour tester
// l'hypothese de contention coeur 0 (deja la cause du "gel de fond" corrige
// le 28 sept., voir docs/engineering/15_RUNTIME_AND_PROFILING.md).
//
// SSID/mot de passe/jeton demandes en direct sur le port serie au
// demarrage, jamais ecrits en dur ni committes (meme convention que
// test_wifi_s3.cpp).

#include <Arduino.h>

#include "test_cloudsync_probe_common.h"

void setup() {
    ProbeCommon::begin(
        "=== AquaLook - banc isole ETAPE 1/N : TLS+requete reelle, RIEN d'autre ===");
}

void loop() {
    ProbeCommon::loop();
    delay(50);
}
