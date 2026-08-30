#include "StatusLed.h"

#if AQUALOOK_BOARD_S3
#include <Adafruit_NeoPixel.h>
#endif

namespace AquaLook {
namespace StatusLed {

namespace {

struct Rgb { uint8_t r, g, b; };

// Etat voulu. Les setters ne levent g_dirty que si la couleur change
// reellement, ce qui suffit a eviter de repousser le ruban a chaque
// tour de boucle (voir la note sur commit() dans StatusLed.h).
Rgb     g_wanted[MAX_LEDS] = {};
bool    g_dirty            = true;   // premier commit() toujours transmis
uint8_t g_activeLeds       = 1U;     // etat general seul tant qu'on ignore nbZones

#if AQUALOOK_BOARD_S3

// Ruban externe : cette carte n'a aucun voyant embarque.
//
// NEO_GRB est l'ordre de composantes du WS2812 standard. Si le banc
// test_ws2812_s3 montre un "rouge" qui sort vert, c'est un ruban en
// NEO_RGB : c'est ici qu'il faut le corriger, pas dans la logique.
Adafruit_NeoPixel g_strip(MAX_LEDS, AQ_S3_LED_PIN, NEO_GRB + NEO_KHZ800);

#else

// Voyant RGB embarque de l'ESP32-2432S028R, en logique INVERSEE
// (anode commune : rapport cyclique 0 = pleine lumiere). Les broches
// et canaux vivaient dans ScreenManager ; ils appartiennent au
// materiel du voyant, donc a ce fichier.
constexpr uint8_t PIN_R = 4;
constexpr uint8_t PIN_G = 16;
constexpr uint8_t PIN_B = 17;
constexpr uint8_t CH_R  = 5;
constexpr uint8_t CH_G  = 6;
constexpr uint8_t CH_B  = 7;

#endif

}  // namespace

void begin() {
#if AQUALOOK_BOARD_S3
    g_strip.begin();
    // Plafond de luminosite : 9 LED en blanc plein tireraient ~540 mA,
    // bien au-dela de ce que le rail 5 V du module laisse passer.
    // Valeur retenue par le banc test_ws2812_s3.
    g_strip.setBrightness(AQ_S3_LED_BRIGHTNESS);
    g_strip.clear();
    g_strip.show();

    // Temoin d'initialisation du module : une passe unique du ruban.
    // C'est le seul endroit du firmware ou le voyant est bloquant, et
    // c'est assume - on est dans setup(), rien n'arrose encore, et cela
    // confirme d'un coup d'oeil que toutes les LED repondent et dans
    // quel ordre. Cout : environ 1 s au demarrage.
    for (uint8_t i = 0; i < MAX_LEDS; i++) {
        g_strip.clear();
        g_strip.setPixelColor(i, 255, 255, 255);
        g_strip.show();
        delay(100);
    }
    g_strip.clear();
    g_strip.show();
#else
    ledcSetup(CH_R, 5000, 8);
    ledcSetup(CH_G, 5000, 8);
    ledcSetup(CH_B, 5000, 8);
    ledcAttachPin(PIN_R, CH_R);
    ledcAttachPin(PIN_G, CH_G);
    ledcAttachPin(PIN_B, CH_B);
#endif

    for (uint8_t i = 0; i < MAX_LEDS; i++) {
        g_wanted[i] = Rgb{0, 0, 0};
    }
    g_dirty = true;
    commit();
}

void setZoneCount(uint8_t nbZones) {
    if (nbZones > MAX_ACTIVE_ZONES) nbZones = MAX_ACTIVE_ZONES;

    // nombre de LED = nombre de zones + 1 (regle posee le 30 aout 2026).
    const uint8_t wanted = (uint8_t)(nbZones + 1U);
    if (wanted == g_activeLeds) return;

    // Les LED qui sortent du ruban actif doivent s'eteindre, sinon elles
    // resteraient figees sur la couleur de la derniere zone qu'elles
    // representaient apres une reduction du nombre de zones.
    for (uint8_t i = wanted; i < MAX_LEDS; i++) {
        g_wanted[i] = Rgb{0, 0, 0};
    }
    g_activeLeds = wanted;
    g_dirty = true;
}

void setStatus(uint8_t r, uint8_t g, uint8_t b) {
    if (g_wanted[0].r == r && g_wanted[0].g == g && g_wanted[0].b == b) return;
    g_wanted[0] = Rgb{r, g, b};
    g_dirty = true;
}

void setZone(uint8_t zone, uint8_t r, uint8_t g, uint8_t b) {
#if AQUALOOK_BOARD_S3
    const uint8_t idx = (uint8_t)(zone + 1U);
    if (idx >= g_activeLeds) return;   // zone au-dela du ruban actif
    if (g_wanted[idx].r == r && g_wanted[idx].g == g && g_wanted[idx].b == b) return;
    g_wanted[idx] = Rgb{r, g, b};
    g_dirty = true;
#else
    // Voyant unique sur la carte historique : rien a montrer par zone.
    (void)zone; (void)r; (void)g; (void)b;
#endif
}

void clearZones() {
    for (uint8_t i = 1; i < MAX_LEDS; i++) {
        if (g_wanted[i].r || g_wanted[i].g || g_wanted[i].b) {
            g_wanted[i] = Rgb{0, 0, 0};
            g_dirty = true;
        }
    }
}

void commit() {
    if (!g_dirty) return;
    g_dirty = false;

#if AQUALOOK_BOARD_S3
    for (uint8_t i = 0; i < MAX_LEDS; i++) {
        const Rgb c = (i < g_activeLeds) ? g_wanted[i] : Rgb{0, 0, 0};
        g_strip.setPixelColor(i, c.r, c.g, c.b);
    }
    g_strip.show();
#else
    // Logique inversee : 255 - valeur.
    ledcWrite(CH_R, 255 - g_wanted[0].r);
    ledcWrite(CH_G, 255 - g_wanted[0].g);
    ledcWrite(CH_B, 255 - g_wanted[0].b);
#endif
}

}  // namespace StatusLed
}  // namespace AquaLook
