// Test isole n°9 du portage ESP32-S3 (docs/architecture/HW_JC4827W543_PORT_IMPACT.md, §8)
// Carte cible : Guition JC4827W543C_I - WiFi (module ESP32-S3-WROOM-1).
//
// Question a laquelle ce test repond : connexion et portee ? Critere de
// reussite : association, adresse IP, RSSI correct.
//
// SSID/mot de passe demandes en direct sur le port serie au demarrage,
// jamais ecrits en dur ni committes : evite tout secret dans le depot,
// contrairement a un #define SSID/PASSWORD qui finirait dans git.

#include <Arduino.h>
#include <WiFi.h>

static String readSerialLine(const char *prompt) {
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

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 5000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== AquaLook - bring-up ESP32-S3 JC4827W543C_I - test 9/9 ==="));
    Serial.printf("Adresse MAC : %s\n", WiFi.macAddress().c_str());

    String ssid = readSerialLine("SSID : ");
    String pass = readSerialLine("Mot de passe (affiche en clair, banc de test) : ");

    Serial.printf("Connexion a \"%s\"...\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(300);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F(">>> RESULTAT test 9 : OK - associe"));
        Serial.printf("    IP     : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("    RSSI   : %d dBm\n", WiFi.RSSI());
        Serial.printf("    Canal  : %d\n", WiFi.channel());
        Serial.printf("    Delai association : %lu ms\n", (unsigned long)(millis() - start));
    } else {
        Serial.printf(">>> RESULTAT test 9 : ECHEC - statut WiFi.status()=%d apres 15 s\n",
                      (int)WiFi.status());
        Serial.println(F("    Codes : 1=SSID absent 4=echec 6=mauvais mot de passe"));
    }
}

void loop() {
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck >= 5000) {
        lastCheck = millis();
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[%lus] connecte - IP=%s RSSI=%d dBm\n",
                          (unsigned long)(millis() / 1000),
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else {
            Serial.printf("[%lus] non connecte - statut=%d\n",
                          (unsigned long)(millis() / 1000), (int)WiFi.status());
        }
    }
}
