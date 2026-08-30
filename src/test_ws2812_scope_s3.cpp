// Banc oscilloscope WS2812 - GPIO46 - Guition JC4827W543C_I (ESP32-S3)
//
// Taille pour la mesure, pas pour l'oeil. A utiliser quand le ruban reste
// ETEINT alors que l'oscilloscope montre bien du trafic sur DIN : le
// signal existe, mais quelque chose empeche le ruban de l'interpreter.
//
// Le banc envoie toujours LA MEME trame, toutes les 50 ms :
//
//     LED 0 en blanc plein, toutes les autres eteintes
//
// Ce choix n'est pas cosmetique. En NEO_GRB, cela produit exactement :
//
//     24 bits a "1"          puis   24 x (n-1) bits a "0"   puis  repos
//     (LED 0 = 0xFF,FF,FF)          (les autres a 0x00,00,00)      > 50 us
//
// soit, pour un ruban de 9 LED : 24 bits a "1" puis 192 bits a "0".
//
// Les deux formes de bit se suivent donc DANS LA MEME TRAME : T1H et T0H
// se mesurent cote a cote, sur un seul ecran, sans toucher aux reglages.
// Et une seule LED blanche tire ~60 mA, contre ~480 mA pour un ruban
// entier - on peut laisser tourner sans risque pour le rail 5 V.
//
// Si le ruban fonctionne, la LED 0 s'allume en blanc, fixe. C'est aussi
// un test visuel immediat.
//
// ── Ce qu'il faut lire sur l'oscilloscope ───────────────────────────
//
// 1. NIVEAU AU REPOS, en base de temps lente (5 ms/div) :
//
//         rafale ~240 us            50 ms de silence
//        ┌──────────────┐                              ┌────
//    ────┘              └──────────────────────────────┘
//                                                  ^
//                            doit etre a 0 V, JAMAIS a 5 V
//
//    Le WS2812 verrouille les couleurs sur un maintien BAS de plus de
//    50 us. Une ligne au repos a 5 V donne un trafic parfaitement
//    valide a l'oscilloscope et un ruban qui n'affiche jamais rien :
//    c'est le symptome d'un montage qui inverse. Deux etages inverseurs
//    en serie doivent restituer la polarite - si le repos est haut,
//    c'est qu'un seul etage inverse reellement.
//
// 2. DUREE DES BITS, en base de temps rapide (200 ns/div) :
//
//      bit "1"  ┌────────┐            bit "0"  ┌────┐
//               │  800ns │                     │400 │
//      ─────────┘        └──────           ────┘    └────────
//               <---- 1,25 us ---->        <--- 1,25 us --->
//
//    | Grandeur                | Attendu  | Tolerance |
//    |-------------------------|----------|-----------|
//    | Temps de montee 10-90 % | < 100 ns | critique  |
//    | Bit "1" - duree du haut | 800 ns   | +/-150 ns |
//    | Bit "0" - duree du haut | 400 ns   | +/-150 ns |
//    | Periode d'un bit        | 1,25 us  | +/-600 ns |
//
//    Un front montant mou est le defaut classique d'un decaleur a
//    transistor : l'etage tire fort vers le bas, mais remonte a travers
//    sa resistance de tirage. Avec 10 kOhm et ~50 pF, la constante de
//    temps atteint ~500 ns, soit plus que le bit "0" lui-meme : le front
//    n'a pas fini de monter que le bit est deja passe. Descendre a
//    1 kOhm, voire 470 Ohm.
//
// 3. Si tout ce qui precede est conforme et que le ruban reste eteint,
//    le probleme n'est plus sur la ligne de donnee : verifier la MASSE
//    COMMUNE entre l'ESP32, le decaleur et l'alimentation du ruban, puis
//    l'alimentation 5 V du ruban, puis le sens (la fleche serigraphiee
//    doit pointer VERS le ruban ; sur DOUT, rien ne s'allume).

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     46

// Meme reglage que le banc autonome, surchargeable depuis platformio.ini
// (-DAQ_TEST_LED_COUNT=n).
#ifndef AQ_TEST_LED_COUNT
#define AQ_TEST_LED_COUNT 9
#endif
#define LED_COUNT   AQ_TEST_LED_COUNT

// Periode de repetition de la trame. 50 ms laisse un repos tres large
// devant les ~240 us de la trame : le niveau au repos est facile a lire.
static const uint32_t FRAME_PERIOD_MS = 50UL;

// Sortie de declenchement optionnelle, basculee juste avant l'envoi.
// Desactivee par defaut (-1) : les broches encore libres de cette carte
// ne sont pas toutes identifiees, et en piloter une au hasard pourrait
// percuter un peripherique. La rafale elle-meme suffit a declencher
// l'oscilloscope. Mettre un numero de broche ici pour l'activer.
#define TRIGGER_PIN  -1

// setBrightness() n'est volontairement PAS appelee : la bibliotheque
// enverrait alors des octets mis a l'echelle, et les 24 bits de la LED 0
// ne seraient plus tous a "1". La mesure perdrait sa reference.
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    const uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) delay(10);
    delay(300);

    Serial.println();
    Serial.println(F("=== Banc oscilloscope WS2812 - GPIO46 ==="));
    Serial.printf("Trame fixe toutes les 50 ms : LED 0 blanche, les %d autres eteintes.\n",
                  LED_COUNT - 1);
    Serial.printf("Soit 24 bits a 1 puis %d bits a 0, dans la meme trame.\n",
                  24 * (LED_COUNT - 1));
    Serial.println();
    Serial.println(F("Au repos la ligne doit etre a 0 V. A 5 V, le montage inverse"));
    Serial.println(F("et le ruban ne verrouille jamais ses couleurs."));
    Serial.println(F("Bit 1 : 800 ns haut. Bit 0 : 400 ns haut. Periode 1,25 us."));
    Serial.println(F("Temps de montee : moins de 100 ns."));

#if TRIGGER_PIN >= 0
    pinMode(TRIGGER_PIN, OUTPUT);
    digitalWrite(TRIGGER_PIN, LOW);
#endif

    strip.begin();
    strip.clear();
    strip.show();
}

void loop() {
    // Trame identique a chaque fois : rien ne varie, donc toute variation
    // observee a l'oscilloscope vient du montage, jamais du motif.
    strip.setPixelColor(0, 255, 255, 255);            // 24 bits a "1"
    for (uint16_t i = 1; i < LED_COUNT; i++) {
        strip.setPixelColor(i, 0, 0, 0);              // puis que des "0"
    }

#if TRIGGER_PIN >= 0
    digitalWrite(TRIGGER_PIN, HIGH);
#endif
    strip.show();
#if TRIGGER_PIN >= 0
    digitalWrite(TRIGGER_PIN, LOW);
#endif

    // Signe de vie discret : une ligne toutes les 5 s, pour confirmer que
    // la carte tourne toujours si un moniteur est ouvert. Sans incidence
    // sur la mesure.
    static uint32_t lastLog = 0;
    const uint32_t now = millis();
    if (now - lastLog >= 5000UL) {
        lastLog = now;
        Serial.println(F("trame envoyee (LED 0 blanche attendue)"));
    }

    delay(FRAME_PERIOD_MS);
}
