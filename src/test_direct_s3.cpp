// Test cible du 27 aout 2026 : le firmware reel (env ProgrammeArrosage_s3)
// dessine directement sur le panneau (pas via Arduino_Canvas), exactement
// comme lib/tft_espi_compat_s3/src/TFT_eSPI.cpp::init(). Ce test reproduit
// EXACTEMENT cette construction pour isoler un soupcon : fillScreen()
// (plein ecran) fonctionne sur materiel reel, mais rien d'autre (fillRect
// partiel, sprites) n'apparait sur l'ecran principal - a confirmer ou
// infirmer isolement, sans passer par tout DisplayManager.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define LCD_BL AQ_S3_LCD_BL
#define LCD_BL_CHANNEL AQ_S3_LCD_BL_CHANNEL

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);
Arduino_NV3041A *panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS);
// PAS de Arduino_Canvas ici - dessin direct sur panel, comme _tft en
// production. C'est precisement ce chemin qui est suspecte.
Arduino_GFX *gfx = panel;

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== Test direct-panel (sans Canvas) - 27 aout 2026 ==="));

    ledcSetup(LCD_BL_CHANNEL, 5000, 12);
    ledcAttachPin(LCD_BL, LCD_BL_CHANNEL);
    ledcWrite(LCD_BL_CHANNEL, 4095);

    if (!panel->begin()) {
        Serial.println(F(">>> ECHEC panel->begin()"));
        while (true) delay(1000);
    }
    Serial.println(F("panel->begin() OK"));
}

static uint8_t s_step = 0;
static uint32_t s_last = 0;

void loop() {
    if (millis() - s_last < 3000) return;
    s_last = millis();

    switch (s_step) {
        case 0:
            Serial.println(F("Etape 0 : fillScreen(NOIR) - plein ecran"));
            gfx->fillScreen(0x0000);
            break;
        case 1:
            Serial.println(F("Etape 1 : fillRect PARTIEL rouge, centre 200x100"));
            gfx->fillRect(140, 86, 200, 100, 0xF800);
            break;
        case 2:
            Serial.println(F("Etape 2 : fillRoundRect PARTIEL vert, 100,50,280,172,10"));
            gfx->fillRoundRect(100, 50, 280, 172, 10, 0x07E0);
            break;
        case 3:
            Serial.println(F("Etape 3 : drawRect PARTIEL contour blanc (sans fill)"));
            gfx->drawRect(50, 30, 380, 212, 0xFFFF);
            break;
        case 4:
            Serial.println(F("Etape 4 : setCursor+print texte blanc"));
            gfx->fillScreen(0x0000);
            gfx->setTextColor(0xFFFF);
            gfx->setTextSize(3);
            gfx->setCursor(60, 120);
            gfx->print("TEST TEXTE");
            break;
        default:
            s_step = 255;  // reste sur le dernier etat, boucle stoppee
            return;
    }
    s_step++;
}
