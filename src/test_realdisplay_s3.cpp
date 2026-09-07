// Test cible du 27 aout 2026 - bissection : SD, I2C et WiFi/serveur web
// ont chacun ete ecartes individuellement (toujours grise/noir sans
// eux, teste separement sur le firmware complet). Ce test utilise la
// VRAIE classe DisplayManager (pas une reproduction simplifiee comme
// test_sprite_s3.cpp) dans un harnais minimal - aucun autre gestionnaire
// du firmware reel (EventLog, FaultManager, ConfigManager,
// ScheduleManager, RelaisManager, SystemDiagnostics, BootLoopGuard,
// OtaBootGuard...) n'est construit. Isole si le bug vient de
// DisplayManager lui-meme (et de sa complexite reelle : HomeCache,
// drawHomeFull_list, renderPlanSprite complet...) ou de son interaction
// avec le reste de l'echafaudage du firmware.
//
// Toutes les dependances de DisplayManager::begin() sont nullptr : le
// code gere deja ce cas partout (_config ? ... : ..., etc.), verifie a
// la lecture avant d'ecrire ce test.

#include <Arduino.h>
#include "DisplayManager.h"

DisplayManager displayMgr;

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== Test vraie classe DisplayManager, harnais minimal - 27 aout 2026 ==="));

    // Reproduit l'ordre exact de main.cpp : initTft()+showSplash() AVANT
    // begin(), pas begin() seul comme le premier essai de ce test.
    displayMgr.initTft();
    for (uint8_t i = 0; i < 8; i++) {
        displayMgr.showSplash(i, "Etape");
        delay(100);
    }

    displayMgr.begin(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    Serial.println(F("displayMgr.begin(nullptr x6) fait - HOME devrait s'afficher"));
}

void loop() {
    displayMgr.update();
    delay(10);
}
