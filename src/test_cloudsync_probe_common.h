#pragma once

// Logique partagee entre les differentes etapes du banc isole de
// validation CloudSync (voir test_cloudsync_probe.cpp pour l'etape de
// base sans aucun autre composant, et les variantes _asynctcp/_sd/_display
// pour la reintroduction progressive de contention). Chaque etape appelle
// ProbeCommon::begin() dans setup() et ProbeCommon::loop() dans loop(),
// puis ajoute son propre composant a cote.
namespace ProbeCommon {
// banner : ligne(s) affichees juste apres l'init serie, avant la connexion
// WiFi -- decrit ce que CETTE etape rajoute par rapport au banc de base.
void begin(const char* banner);
void loop();

// Variante pour une etape qui gere elle-meme la connexion WiFi (ex. la
// VRAIE classe WiFiManager, etape 6) : init serie + prompt jeton, MAIS
// AUCUNE gestion WiFi ici (le firmware appelant doit s'en charger et
// s'assurer que WiFi.status()==WL_CONNECTED avant d'appeler
// runCycleIfDue()).
void beginCycleOnly(const char* banner);
void runCycleIfDue();
}
