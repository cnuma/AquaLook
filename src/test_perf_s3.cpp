// Test isole n°4 du portage ESP32-S3 (docs/architecture/HW_JC4827W543_PORT_IMPACT.md, §8)
// Carte cible : Guition JC4827W543C_I - NV3041A QSPI, framebuffer PSRAM.
//
// Question a laquelle ce test repond : combien coute un rafraichissement
// plein ecran depuis la PSRAM ? Decisif - peut invalider la strategie du
// §3 (framebuffer plein ecran unique en PSRAM plutot que des sprites
// partiels). Mesure chiffree exigee, pas une impression qualitative.
//
// Arduino_Canvas n'expose que flush() (transfert integral du framebuffer,
// pas de variante par rectangle - voir Arduino_Canvas.h). La mesure porte
// donc sur le seul cas mesurable : le cout d'un flush() plein ecran,
// separe du cout de dessin en RAM (fillScreen seul).
//
// Brochage/parametres : voir la section [jc4827w543c_i] de platformio.ini,
// meme source que test_screen_s3/test_relay_s3/test_touch_s3.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define LCD_BL AQ_S3_LCD_BL
#define LCD_BL_CHANNEL AQ_S3_LCD_BL_CHANNEL

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);
Arduino_NV3041A *panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS);
Arduino_GFX *gfx = new Arduino_Canvas(AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT, panel);

static const size_t kIterations = 20;
static const size_t kFramebufferBytes =
    (size_t)AQ_S3_SCREEN_WIDTH * (size_t)AQ_S3_SCREEN_HEIGHT * 2U;

struct Stats {
    uint32_t minUs = UINT32_MAX;
    uint32_t maxUs = 0;
    uint64_t sumUs = 0;
    size_t count = 0;
    void add(uint32_t us) {
        if (us < minUs) minUs = us;
        if (us > maxUs) maxUs = us;
        sumUs += us;
        count++;
    }
    uint32_t avgUs() const { return count ? (uint32_t)(sumUs / count) : 0; }
};

static void printStats(const char *label, const Stats &s) {
    Serial.printf("%-22s min=%6lu us  avg=%6lu us  max=%6lu us  (n=%u)\n",
                  label, (unsigned long)s.minUs, (unsigned long)s.avgUs(),
                  (unsigned long)s.maxUs, (unsigned)s.count);
}

static void showResultsOnScreen(const Stats &draw, const Stats &flush) {
    gfx->fillScreen(0x0000);
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->print(F("AquaLook - test 4/9 : perf ecran"));
    gfx->setTextSize(1);

    gfx->setCursor(10, 45);
    gfx->printf("Framebuffer : %dx%d x2o = %u o", AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT,
                (unsigned)kFramebufferBytes);

    gfx->setCursor(10, 65);
    gfx->printf("fillScreen (dessin RAM)  min=%lu max=%lu avg=%lu us",
                (unsigned long)draw.minUs, (unsigned long)draw.maxUs, (unsigned long)draw.avgUs());

    gfx->setCursor(10, 85);
    gfx->printf("flush() (transfert QSPI) min=%lu max=%lu avg=%lu us",
                (unsigned long)flush.minUs, (unsigned long)flush.maxUs, (unsigned long)flush.avgUs());

    double avgFlushSec = flush.avgUs() / 1e6;
    double mbps = avgFlushSec > 0 ? (kFramebufferBytes * 8.0 / avgFlushSec) / 1e6 : 0;
    double fps = avgFlushSec > 0 ? 1.0 / avgFlushSec : 0;

    gfx->setCursor(10, 110);
    gfx->printf("Debit effectif : %.1f Mbit/s", mbps);
    gfx->setCursor(10, 130);
    gfx->printf("Taux max rafraichissement plein ecran : %.1f im/s", fps);

    gfx->setCursor(10, 160);
    gfx->print(F("Sur %u iterations, apres 5 de mise en chauffe"));

    gfx->setCursor(10, 250);
    gfx->print(F("Resultat aussi imprime sur le port serie."));
    gfx->flush();
}

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== AquaLook - bring-up ESP32-S3 JC4827W543C_I - test 4/9 ==="));
    Serial.printf("Framebuffer attendu : %dx%d x 2 octets = %u octets\n",
                  AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT, (unsigned)kFramebufferBytes);

    ledcSetup(LCD_BL_CHANNEL, 5000, 12);
    ledcAttachPin(LCD_BL, LCD_BL_CHANNEL);
    ledcWrite(LCD_BL_CHANNEL, 4095);

    if (!gfx->begin()) {
        Serial.println(F(">>> ECHEC gfx->begin() - impossible de mesurer"));
        while (true) {
            delay(1000);
        }
    }

    // Mise en chauffe : les premiers appels incluent parfois une init
    // paresseuse (cache, allocation) qui fausserait la mesure basse.
    for (int i = 0; i < 5; i++) {
        gfx->fillScreen(i % 2 ? 0xFFFF : 0x0000);
        gfx->flush();
    }

    Stats drawStats, flushStats;
    for (size_t i = 0; i < kIterations; i++) {
        uint16_t color = (i % 2) ? 0xF800 : 0x001F;

        uint32_t t0 = micros();
        gfx->fillScreen(color);
        uint32_t t1 = micros();
        gfx->flush();
        uint32_t t2 = micros();

        drawStats.add(t1 - t0);
        flushStats.add(t2 - t1);
    }

    Serial.println(F("\n--- Resultats (plein ecran, fillScreen uni) ---"));
    printStats("fillScreen (RAM)", drawStats);
    printStats("flush() (QSPI)", flushStats);

    double avgFlushSec = flushStats.avgUs() / 1e6;
    double mbps = avgFlushSec > 0 ? (kFramebufferBytes * 8.0 / avgFlushSec) / 1e6 : 0;
    double fps = avgFlushSec > 0 ? 1.0 / avgFlushSec : 0;
    Serial.printf("Debit effectif flush() : %.1f Mbit/s\n", mbps);
    Serial.printf("Taux max rafraichissement plein ecran : %.1f images/s\n", fps);
    Serial.println(F(">>> RESULTAT test 4 : mesure ci-dessus - comparer au §3 du document"));
    Serial.println(F("    d'impact pour decider framebuffer plein ecran vs sprites partiels."));

    showResultsOnScreen(drawStats, flushStats);
}

void loop() {
    // Rien en continu : la mesure est faite une fois au demarrage, le
    // resultat reste affiche a l'ecran.
    delay(1000);
}
