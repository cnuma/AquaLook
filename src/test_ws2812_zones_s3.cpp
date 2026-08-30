// Apercu du voyant AquaLook sur ruban WS2812 - 9 LED - GPIO46
// Guition JC4827W543C_I (ESP32-S3)
//
// Tourne tout seul, rien a taper. Ce banc ne teste pas le ruban : il
// montre le RENDU REEL que produira le firmware, pour le juger a l'oeil
// avant de l'y integrer. Le pilote definitif est src/StatusLed.cpp.
//
// ── Affectation des LED ─────────────────────────────────────────────
//   Regle posee par l'utilisateur : nombre de LED = nombre de zones + 1
//
//     LED 0        etat general du module
//     LED 1 a 8    zones 1 a 8
//
// ── Ce que montre une LED de zone ───────────────────────────────────
// La LED porte un ETAT, pas une identite : la position suffit deja a
// designer la zone, la couleur est donc libre de dire autre chose.
//
//   arrosage en cours        respiration BLEUE
//   arrosage bloque par la   respiration ORANGE
//     pluie (seuil depasse)
//   rien de prevu            eteinte
//
// L'orange n'est pas choisi au hasard : c'est Theme::AMBER, exactement
// la couleur dont le planning du LCD colore deja un creneau suspendu
// pour cause de pluie (DisplayManager.cpp, "slotColor = rainBlk ?
// Theme::AMBER : col_z"). Ruban et ecran disent donc la meme chose avec
// la meme teinte.
//
// Le bleu est Theme::BLUE, celui du planning et des icones.
//
// ── Etat general (LED 0) ────────────────────────────────────────────
// Meme echelle de priorites que ScreenManager::updateLed() :
//
//   1. panne              rouge clignotant   (FaultManager, prioritaire)
//   2. recherche WiFi     ambre rapide
//   3. mise a jour prete  violet lent
//   4. repos              bref eclat vert toutes les 4 s
//
// Le rouge reste reserve EXCLUSIVEMENT a une panne, pour qu'il garde son
// sens au premier coup d'oeil.
//
// ── Initialisation ──────────────────────────────────────────────────
// Un chenillard parcourt le ruban UNE FOIS au demarrage. Il fait office
// de temoin d'initialisation du module, et confirme au passage que
// toutes les LED repondent et dans quel ordre.

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     46

// 8 zones + 1 etat general. Surchargeable depuis platformio.ini
// (-DAQ_TEST_LED_COUNT=n) si le ruban de l'etabli est plus court.
#ifndef AQ_TEST_LED_COUNT
#define AQ_TEST_LED_COUNT 9
#endif
#define LED_COUNT   AQ_TEST_LED_COUNT

#define NB_ZONES    (LED_COUNT - 1)
#define ZONE_LED(z) ((uint16_t)((z) + 1U))   // zone 0 -> LED 1

#define BRIGHTNESS  40    // /255 - 9 LED en blanc plein tireraient ~540 mA

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

struct Rgb { uint8_t r, g, b; };

// Equivalents RGB888 des couleurs du theme (converties depuis le RGB565
// de Theme.h, pour que le ruban et le LCD montrent la meme teinte).
static const Rgb C_WATERING = {  0, 146, 255};   // Theme::BLUE  0x049F
static const Rgb C_RAIN     = {255, 166,   0};   // Theme::AMBER 0xFD20
static const Rgb C_OFF      = {  0,   0,   0};

// ── Scenarios joues en boucle ───────────────────────────────────────
enum Status : uint8_t { ST_IDLE, ST_WIFI, ST_UPDATE, ST_FAULT };

struct Scene {
    Status      status;
    uint16_t    wateringMask;   // bit z leve = zone z en cours d'arrosage
    uint16_t    rainMask;       // bit z leve = zone z bloquee par la pluie
    const char* label;
};

static const Scene SCENES[] = {
    { ST_IDLE,   0x000, 0x000, "1/9 repos - toutes zones eteintes" },
    { ST_IDLE,   0x001, 0x000, "2/9 zone 1 en arrosage - bleu" },
    { ST_IDLE,   0x024, 0x000, "3/9 zones 3 et 6 en arrosage" },
    { ST_IDLE,   0x0FF, 0x000, "4/9 les 8 zones en arrosage" },
    { ST_IDLE,   0x000, 0x002, "5/9 zone 2 bloquee par la pluie - orange" },
    { ST_IDLE,   0x001, 0x048, "6/9 zone 1 arrose, zones 4 et 7 bloquees" },
    { ST_WIFI,   0x000, 0x000, "7/9 recherche WiFi - ambre rapide" },
    { ST_UPDATE, 0x000, 0x000, "8/9 mise a jour en attente - violet lent" },
    { ST_FAULT,  0x001, 0x000, "9/9 PANNE - rouge, prioritaire meme en arrosage" },
};
static const uint8_t  SCENE_COUNT = sizeof(SCENES) / sizeof(SCENES[0]);
static const uint32_t SCENE_MS    = 8000UL;

// Respiration : sinusoide sur 2 s, jamais totalement eteinte pour que la
// zone reste identifiable au creux. Meme calcul que renderZones() dans
// ScreenManager.cpp.
static uint16_t breathLevel(uint32_t now) {
    const float phase = (now % 2000UL) / 2000.0f;
    const float wave  = 0.5f * (1.0f - cosf(phase * 2.0f * (float)PI));
    return (uint16_t)(40.0f + wave * 215.0f);
}

// Clignotement symetrique : vrai la moitie du temps.
static bool blink(uint32_t now, uint32_t periodMs) {
    return (now % periodMs) < (periodMs / 2U);
}

static Rgb statusColor(Status st, uint32_t now) {
    switch (st) {
        case ST_FAULT:                                   // rouge, 500 ms
            return blink(now, 1000U) ? Rgb{255, 0, 0} : C_OFF;
        case ST_WIFI:                                    // ambre, 250 ms
            return blink(now, 500U)  ? Rgb{255, 100, 0} : C_OFF;
        case ST_UPDATE:                                  // violet, 1,5 s
            return blink(now, 3000U) ? Rgb{102, 51, 204} : C_OFF;
        case ST_IDLE:
        default: {
            // Eclat vert de 150 ms toutes les 4 s (mode 1, celui par defaut).
            return ((now % 4000UL) < 150UL) ? Rgb{0, 255, 0} : C_OFF;
        }
    }
}

// Temoin d'initialisation : une passe unique au demarrage. Confirme au
// passage que toutes les LED repondent, et dans quel ordre.
static void initSweep() {
    for (uint16_t i = 0; i < LED_COUNT; i++) {
        strip.clear();
        strip.setPixelColor(i, 255, 255, 255);
        strip.show();
        delay(120);
    }
    strip.clear();
    strip.show();
    delay(200);
}

void setup() {
    Serial.begin(115200);
    const uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) delay(10);
    delay(300);

    Serial.println();
    Serial.println(F("=== Apercu du voyant AquaLook - ruban WS2812, GPIO46 ==="));
    Serial.printf("%d LED : LED 0 = etat general, LED 1 a %d = zones 1 a %d.\n",
                  LED_COUNT, NB_ZONES, NB_ZONES);
    Serial.println(F("Zone : bleu = arrosage, orange = bloque par la pluie, eteint = rien."));

    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    strip.clear();
    strip.show();

    Serial.println(F("Chenillard d'initialisation..."));
    initSweep();
    Serial.println(F("Scenarios en boucle."));
}

void loop() {
    const uint32_t now   = millis();
    const uint8_t  idx   = (uint8_t)((now / SCENE_MS) % SCENE_COUNT);
    const Scene&   scene = SCENES[idx];

    static uint8_t lastIdx = 255;
    if (idx != lastIdx) {
        lastIdx = idx;
        Serial.println(scene.label);
    }

    // ── LED 0 : etat general du module ──────────────────────────────
    const Rgb st = statusColor(scene.status, now);
    strip.setPixelColor(0, st.r, st.g, st.b);

    // ── LED 1 a 8 : etat de chaque zone ─────────────────────────────
    const uint16_t level = breathLevel(now);
    for (uint8_t z = 0; z < NB_ZONES; z++) {
        const uint16_t bit = (uint16_t)(1U << z);

        // L'arrosage en cours prime sur le blocage : une zone qui arrose
        // n'est, par definition, pas bloquee.
        const Rgb* c = nullptr;
        if      (scene.wateringMask & bit) c = &C_WATERING;
        else if (scene.rainMask     & bit) c = &C_RAIN;

        if (c == nullptr) {
            strip.setPixelColor(ZONE_LED(z), 0, 0, 0);
            continue;
        }
        strip.setPixelColor(ZONE_LED(z),
                            (uint8_t)((c->r * level) / 255U),
                            (uint8_t)((c->g * level) / 255U),
                            (uint8_t)((c->b * level) / 255U));
    }

    strip.show();
    delay(20);
}
