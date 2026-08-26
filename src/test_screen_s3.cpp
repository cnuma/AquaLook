// Tests isoles n°2 (retroeclairage) + n°3 (ecran QSPI NV3041A) du portage
// ESP32-S3 (docs/architecture/HW_JC4827W543_PORT_IMPACT.md, §8)
// Carte cible : Guition JC4827W543C_I - controleur NV3041A, bus QSPI
//
// Brochage et construction Arduino_GFX repris tels quels d'un exemple
// communautaire fonctionnel (profi-max/JC4827W543_4.3inch_ESP32S3_board),
// pas invente : ce controleur a des pieges deja documentes (inversion
// couleur, swap d'octets, rotation limitee a 0°/180°) - autant partir
// d'une construction connue pour marcher que d'une supposition.
//
// Question a laquelle ce test repond : le NV3041A s'initialise-t-il,
// couleurs et orientation correctes ? Critere de reussite : mire stable,
// rouge/vert/bleu justes, pas d'inversion.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define LCD_BL 1  // retroeclairage, PWM (test isole n°2)
#define LCD_BL_CHANNEL 0  // API LEDC par canal (ledcSetup/ledcAttachPin) -
                          // ce framework n'a pas encore l'API par broche

// Bus + panneau + surface : repris de l'exemple communautaire cite plus
// haut. Arduino_Canvas alloue le framebuffer 480x272 en PSRAM et ne
// l'envoie au panneau qu'au flush() explicite (voir §3/§7 du document
// d'impact sur la contention de bus que cela implique avec la SD).
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    45 /* cs */, 47 /* sck */, 21 /* d0 */, 48 /* d1 */, 40 /* d2 */, 39 /* d3 */);
// ips=true, comme l'exemple communautaire de reference : confirme correct
// sur cette carte reelle (ips=false essaye a tort le 26 aout, plus faux).
Arduino_NV3041A *panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, true /* IPS */);
Arduino_GFX *gfx = new Arduino_Canvas(480 /* width */, 272 /* height */, panel);

static void setBacklight(uint8_t percent) {
    uint32_t duty = (uint32_t)percent * 4095u / 100u;
    ledcWrite(LCD_BL_CHANNEL, duty);
}

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== AquaLook - bring-up ESP32-S3 JC4827W543C_I - tests 2+3/9 ==="));

    ledcSetup(LCD_BL_CHANNEL, 5000, 12);  // 5 kHz, 12 bits
    ledcAttachPin(LCD_BL, LCD_BL_CHANNEL);
    setBacklight(0);

    Serial.print(F("gfx->begin()... "));
    if (!gfx->begin()) {
        Serial.println(F(">>> RESULTAT test 3 : ECHEC - gfx->begin() a renvoye false"));
        return;
    }
    Serial.println(F("OK"));

    setBacklight(100);
    Serial.println(F(">>> RESULTAT test 2 (retroeclairage) : allume a 100% sur GPIO1"));
    Serial.println(F("Mire couleur en cours - verifier a l'oeil sur l'ecran : rouge/vert/bleu/blanc/noir,"));
    Serial.println(F("3 s chacune. Juger : couleurs justes (pas d'inversion), pas de decalage/artefact."));
}

struct NamedColor {
    const char *name;
    uint16_t rgb565;
};

// Constantes ecrites en dur plutot que via les macros de la bibliotheque :
// evite toute ambiguite si leurs noms different d'une version a l'autre.
static const NamedColor kColors[] = {
    {"ROUGE", 0xF800},
    {"VERT",  0x07E0},
    {"BLEU",  0x001F},
    {"BLANC", 0xFFFF},
    {"NOIR",  0x0000},
};
static size_t s_idx = 0;
static uint32_t s_lastSwitch = 0;

void loop() {
    if (millis() - s_lastSwitch >= 3000) {
        s_lastSwitch = millis();
        gfx->fillScreen(kColors[s_idx].rgb565);
        gfx->flush();
        Serial.printf("Affichage : %s (0x%04X)\n", kColors[s_idx].name, kColors[s_idx].rgb565);
        s_idx = (s_idx + 1) % (sizeof(kColors) / sizeof(kColors[0]));
    }
}
