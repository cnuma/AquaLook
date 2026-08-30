// Banc autonome WS2812 - GPIO46 - Guition JC4827W543C_I (ESP32-S3)
//
// Tourne tout seul : rien a taper, aucun moniteur serie necessaire. Les
// motifs s'enchainent en boucle, 8 s chacun, indefiniment. Il suffit de
// flasher et de regarder le ruban.
//
// Ce que chaque motif permet de conclure :
//
//   1. Chenillard          l'ORDRE physique des LED et le NOMBRE reellement
//                          cablees. Une LED morte ou un saut sautent aux yeux.
//   2. Rouge / vert / bleu l'ORDRE DES COMPOSANTES. Les trois primaires
//                          passent en toutes lettres : si le "rouge"
//                          annonce sort vert, le ruban est en NEO_RGB et
//                          non NEO_GRB.
//   3. Arc-en-ciel         toutes les teintes et tout le ruban d'un coup.
//   4. Respiration         la variation d'intensite, celle qui sera
//                          utilisee pour une zone en cours d'arrosage.
//   5. Degrade FIXE        rien ne bouge : tout scintillement observe ici
//                          est du bruit sur la ligne de donnee, pas un
//                          effet du motif. C'est le motif qui juge la
//                          reserve electrique ci-dessous.
//
// ── Reserve electrique, a lire avant de conclure ────────────────────
// Un WS2812 alimente en 5 V attend un niveau haut d'au moins 0,7 x VDD,
// soit 3,5 V. Un GPIO d'ESP32-S3 ne monte qu'a 3,3 V : on est SOUS le
// seuil garanti par la fiche technique. Beaucoup de rubans acceptent
// quand meme, mais le comportement peut dependre du lot, de la
// temperature et de la longueur du fil.
//
// Couleurs fausses, scintillement ou LED allumees au hasard ne sont donc
// PAS un defaut de code. Parades, par ordre de simplicite :
//   1. alimenter le ruban en ~4,3 V (diode 1N4001 en serie sur le +5 V)
//      abaisse le seuil a 3,0 V, que le GPIO franchit sans probleme ;
//   2. intercaler un adaptateur de niveau 3,3 -> 5 V sur la donnee ;
//   3. sacrifier la premiere LED en adaptateur : elle accepte souvent
//      3,3 V et retransmet ensuite un signal a 5 V aux suivantes.
//
// ── Consommation ────────────────────────────────────────────────────
// Une LED WS2812 en blanc plein tire ~60 mA : un ruban entier depasse vite
// ce que le rail 5 V d'un module laisse passer. La luminosite est donc
// plafonnee (BRIGHTNESS) et le blanc plein n'apparait que sur UNE LED a
// la fois, dans le chenillard.

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     46

// Longueur du ruban. Surchargeable depuis platformio.ini
// (-DAQ_TEST_LED_COUNT=n) pour ne pas avoir a toucher au code selon le
// ruban branche sur l'etabli.
#ifndef AQ_TEST_LED_COUNT
#define AQ_TEST_LED_COUNT 9
#endif
#define LED_COUNT   AQ_TEST_LED_COUNT

#define BRIGHTNESS  40    // /255 - voir la note consommation ci-dessus

// Pas de teinte entre deux LED voisines, pour que l'arc-en-ciel couvre
// exactement une fois la roue chromatique quelle que soit la longueur.
#define HUE_STEP    (256U / LED_COUNT)

// Duree d'affichage de chaque motif.
static const uint32_t STEP_MS = 8000UL;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Roue chromatique : 0..255 -> une couleur saturee, sans flottant.
static uint32_t wheel(uint8_t pos) {
    pos = (uint8_t)(255U - pos);
    if (pos < 85)  return strip.Color((uint8_t)(255 - pos * 3), 0, (uint8_t)(pos * 3));
    if (pos < 170) { pos -= 85; return strip.Color(0, (uint8_t)(pos * 3), (uint8_t)(255 - pos * 3)); }
    pos -= 170;
    return strip.Color((uint8_t)(pos * 3), (uint8_t)(255 - pos * 3), 0);
}

static void showAll(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, r, g, b);
}

static const char* STEP_NAME[5] = {
    "1/5 chenillard - ordre et nombre de LED",
    "2/5 rouge, vert, bleu - ordre des composantes",
    "3/5 arc-en-ciel glissant",
    "4/5 respiration - variation d'intensite",
    "5/5 degrade FIXE - juge le scintillement",
};

void setup() {
    Serial.begin(115200);
    const uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) delay(10);
    delay(300);

    Serial.println();
    Serial.println(F("=== Banc autonome WS2812 - GPIO46 ==="));
    Serial.println(F("Aucune action requise : les motifs s'enchainent en boucle."));

    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    strip.clear();
    strip.show();
}

void loop() {
    const uint32_t now = millis();

    // Motif courant, et temps ecoule depuis son debut.
    const uint8_t  step  = (uint8_t)((now / STEP_MS) % 5U);
    const uint32_t local = now % STEP_MS;

    // Annonce du motif au changement, pour qui aurait un moniteur ouvert.
    // Le banc reste parfaitement utilisable sans.
    static uint8_t lastStep = 255;
    if (step != lastStep) {
        lastStep = step;
        Serial.println(STEP_NAME[step]);
    }

    switch (step) {
        // 1. Chenillard avec trainee : ordre physique et LED manquantes.
        case 0: {
            const uint16_t head = (uint16_t)((local / 250U) % LED_COUNT);
            for (uint16_t i = 0; i < LED_COUNT; i++) {
                const uint16_t back = (uint16_t)((head + LED_COUNT - i) % LED_COUNT);
                const uint8_t  lvl  = (back == 0) ? 255
                                    : (back == 1) ? 90
                                    : (back == 2) ? 25 : 0;
                strip.setPixelColor(i, lvl, lvl, lvl);   // blanc : neutre
            }
            break;
        }

        // 2. Les trois primaires en toutes lettres, ~2,6 s chacune.
        case 1: {
            const uint32_t third = STEP_MS / 3U;
            if      (local < third)      showAll(255, 0, 0);
            else if (local < 2U * third) showAll(0, 255, 0);
            else                         showAll(0, 0, 255);
            break;
        }

        // 3. Arc-en-ciel qui glisse le long du ruban.
        case 2: {
            const uint8_t base = (uint8_t)((now / 12U) & 0xFFU);
            for (uint16_t i = 0; i < LED_COUNT; i++) {
                strip.setPixelColor(i, wheel((uint8_t)(base + i * HUE_STEP)));
            }
            break;
        }

        // 4. Respiration : teinte qui derive, intensite qui monte et
        //    descend sans jamais atteindre zero - c'est ce qui distingue
        //    une respiration d'un clignotement.
        case 3: {
            const uint32_t phase = now % 3000U;
            const uint32_t up    = (phase < 1500U) ? phase : (3000U - phase);
            const uint16_t level = (uint16_t)(30U + (up * 225U) / 1500U);
            const uint32_t hue   = wheel((uint8_t)((now / 40U) & 0xFFU));

            const uint8_t r = (uint8_t)((((hue >> 16) & 0xFFU) * level) / 255U);
            const uint8_t g = (uint8_t)((((hue >> 8)  & 0xFFU) * level) / 255U);
            const uint8_t b = (uint8_t)((( hue        & 0xFFU) * level) / 255U);
            showAll(r, g, b);
            break;
        }

        // 5. Degrade fixe : reference de stabilite. Rien ne doit bouger.
        default: {
            for (uint16_t i = 0; i < LED_COUNT; i++) {
                strip.setPixelColor(i, wheel((uint8_t)(i * HUE_STEP)));
            }
            break;
        }
    }

    strip.show();
    delay(20);
}
