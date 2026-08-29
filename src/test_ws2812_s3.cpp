// Test isole du ruban WS2812 - Guition JC4827W543C_I (ESP32-S3)
//
// Cette carte n'a pas de voyant RGB embarque, contrairement a l'ancienne
// ESP32-2432S028R : ScreenManager::updateLed() et ses modes (arrosage en
// cours, recherche WiFi, mise a jour en attente) existent toujours mais ne
// pilotent plus rien. Un ruban externe est le remplacant prevu.
//
// Question a laquelle ce test repond : le ruban repond-il sur GPIO46, avec
// quel ordre de couleurs, et le niveau logique 3,3 V suffit-il ?
//
// ── Reserve electrique, a lire avant de conclure ────────────────────
// Le WS2812 alimente en 5 V attend un niveau haut d'au moins 0,7 x VDD,
// soit 3,5 V. Un GPIO d'ESP32-S3 ne monte qu'a 3,3 V : on est SOUS le
// seuil garanti par la fiche technique. En pratique beaucoup de rubans
// acceptent quand meme, mais le comportement peut etre instable, dependre
// de la temperature ou du lot, et se degrader avec la longueur du fil.
//
// Si ce test montre des couleurs fausses, du scintillement ou des LED qui
// s'allument au hasard, ce n'est PAS un defaut de code. Les parades
// habituelles, par ordre de simplicite :
//   1. alimenter le ruban en ~4,3 V (une diode 1N4001 en serie sur le +5 V)
//      abaisse le seuil a 3,0 V, que le GPIO franchit sans probleme ;
//   2. intercaler un adaptateur de niveau 3,3 -> 5 V sur la ligne de donnee ;
//   3. sacrifier la premiere LED du ruban en adaptateur : elle accepte
//      souvent 3,3 V et retransmet ensuite un signal a 5 V aux suivantes.
//
// ── Consommation ────────────────────────────────────────────────────
// 8 LED en blanc plein consomment ~480 mA, bien au-dela de ce que la
// plupart des modules laissent passer sur leur rail 5 V. La luminosite est
// donc volontairement plafonnee ici (BRIGHTNESS), et le blanc plein n'est
// utilise que sur UNE led a la fois.

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN    46
#define LED_COUNT   8
#define BRIGHTNESS 40    // /255 - voir la note consommation ci-dessus

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Couleurs de zone, reprises des valeurs par defaut de l'interface Web
// (cZone0..3 dans data/app.js) pour que le ruban et l'ecran parlent le meme
// langage : vert, bleu, ambre, violet, puis on recommence.
struct ZoneColor { uint8_t r, g, b; };
static const ZoneColor ZONE_COLORS[4] = {
    {0x00, 0xfc, 0x00},   // vert
    {0x00, 0x90, 0xf8},   // bleu
    {0xf8, 0xa4, 0x00},   // ambre
    {0x78, 0x00, 0x78},   // violet
};

static void showAll(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, r, g, b);
    strip.show();
}

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) delay(10);
    delay(300);

    Serial.println();
    Serial.println(F("=== Test ruban WS2812 - GPIO46, 8 LED ==="));
    Serial.println(F("Sequence : extinction, defilement blanc, R/V/B, couleurs de zone,"));
    Serial.println(F("puis apercu du rendu vise (zone 0 en arrosage)."));
    Serial.println(F("Si les couleurs sont fausses ou instables : voir la note"));
    Serial.println(F("sur le niveau logique 3,3 V en tete de ce fichier."));

    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    strip.clear();
    strip.show();
}

void loop() {
    // 1. Defilement : confirme le NOMBRE de LED et leur ORDRE physique.
    Serial.println(F("[1] Defilement blanc, une LED a la fois"));
    for (uint16_t i = 0; i < LED_COUNT; i++) {
        strip.clear();
        strip.setPixelColor(i, 255, 255, 255);
        strip.show();
        Serial.printf("    LED %u\n", (unsigned)i);
        delay(400);
    }

    // 2. Primaires : confirme l'ORDRE DES COMPOSANTES. Si "rouge" sort vert,
    //    le ruban est en RGB et non en GRB - changer NEO_GRB en NEO_RGB.
    Serial.println(F("[2] Rouge, puis vert, puis bleu (tout le ruban)"));
    showAll(255, 0, 0); delay(1000);
    showAll(0, 255, 0); delay(1000);
    showAll(0, 0, 255); delay(1000);

    // 3. Couleurs de zone au repos : le rendu vise, une LED par zone.
    Serial.println(F("[3] Couleurs de zone au repos"));
    strip.clear();
    for (uint16_t i = 0; i < LED_COUNT; i++) {
        const ZoneColor& c = ZONE_COLORS[i % 4];
        strip.setPixelColor(i, c.r, c.g, c.b);
    }
    strip.show();
    delay(2000);

    // 4. Apercu du rendu vise : zone 0 en arrosage, bleu en respiration ;
    //    les autres gardent leur couleur de repos.
    Serial.println(F("[4] Zone 0 en arrosage - respiration bleue, 6 s"));
    const uint32_t until = millis() + 6000;
    while (millis() < until) {
        // Respiration : sinusoide approchee sur 2 s, jamais totalement
        // eteinte pour que la zone reste identifiable.
        const float phase = (millis() % 2000) / 2000.0f;
        const float wave  = 0.5f * (1.0f - cosf(phase * 2.0f * PI));
        const uint8_t level = (uint8_t)(40 + wave * 215);

        for (uint16_t i = 0; i < LED_COUNT; i++) {
            if (i == 0) {
                strip.setPixelColor(i, 0, level / 3, level);   // bleu
            } else {
                const ZoneColor& c = ZONE_COLORS[i % 4];
                strip.setPixelColor(i, c.r, c.g, c.b);
            }
        }
        strip.show();
        delay(30);
    }

    strip.clear();
    strip.show();
    delay(1000);
}
