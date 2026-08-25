// Test isole n°1 du portage ESP32-S3 (docs/architecture/HW_JC4827W543_PORT_IMPACT.md, §8)
// Carte cible : Guition JC4827W543C_I (ESP32-S3-WROOM-1-N4R8, 4 Mo flash, 8 Mo PSRAM octale)
//
// Question a laquelle ce test repond : la carte demarre-t-elle, la PSRAM
// octale est-elle vue par le firmware ? Rien d'autre - pas d'ecran, pas de
// tactile, pas de SD, pas de relais. Jetable, non destine a etre integre.
//
// Critere de reussite : ~8 Mo de PSRAM rapportes, console série lisible sur
// le port USB natif (pas de convertisseur UART externe sur cette carte).

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    // USB natif (USB Serial/JTAG) : laisser le temps a l'hote de s'attacher
    // avant le premier message, sinon il est perdu.
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== AquaLook - bring-up ESP32-S3 JC4827W543C_I - test 1/9 ==="));
    Serial.printf("Puce              : %s, rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("Coeurs            : %d\n", ESP.getChipCores());
    Serial.printf("Frequence CPU     : %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
    Serial.printf("Flash             : %lu Ko\n", (unsigned long)(ESP.getFlashChipSize() / 1024));
    Serial.printf("Tas interne libre : %lu o (plus gros bloc %lu o)\n",
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMaxAllocHeap());

    bool psramOk = psramFound();
    Serial.printf("PSRAM detectee    : %s\n", psramOk ? "oui" : "NON");
    if (psramOk) {
        Serial.printf("PSRAM taille      : %lu o (attendu ~8 Mo, type octal)\n",
                      (unsigned long)ESP.getPsramSize());
        Serial.printf("PSRAM libre       : %lu o\n", (unsigned long)ESP.getFreePsram());
        Serial.println(F(">>> RESULTAT : PSRAM OK"));
    } else {
        Serial.println(F(">>> RESULTAT : ECHEC - PSRAM non vue."));
        Serial.println(F("    Verifier board_build.arduino.memory_type=qio_opi dans platformio.ini"));
        Serial.println(F("    et le mode PSRAM reellement cable sur cette carte (octal vs quad)."));
    }
}

void loop() {
    static uint32_t lastBeat = 0;
    if (millis() - lastBeat >= 2000) {
        lastBeat = millis();
        Serial.printf("[%lus] vivant - tas=%lu o, psram_libre=%lu o\n",
                      (unsigned long)(millis() / 1000),
                      (unsigned long)ESP.getFreeHeap(),
                      (unsigned long)(psramFound() ? ESP.getFreePsram() : 0));
    }
}
