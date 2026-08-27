// Test isole n°5 du portage ESP32-S3 (docs/architecture/HW_JC4827W543_PORT_IMPACT.md, §8)
// Carte cible : Guition JC4827W543C_I - tactile GT911 capacitif, I2C.
//
// Question a laquelle ce test repond : adresse I2C, coordonnees, orientation
// coherente avec l'ecran ? Critere de reussite : appui restitue au bon
// endroit, SANS etalonnage (contrairement au XPT2046 resistif de la carte
// actuelle - §5 du document d'impact : les reglages TOUCH_X_MIN/MAX,
// TOUCH_Y_MIN/MAX et l'ecran de calibration deviennent sans objet ici).
//
// Bus I2C DEDIE, distinct de celui du bloc relais (SCL=17/SDA=18, voir
// test_relay_s3.cpp) : SCL=GPIO4, SDA=GPIO8, RST=GPIO38, INT=GPIO3.
// Brochage repris de la meme source communautaire que l'ecran
// (profi-max/JC4827W543_4.3inch_ESP32S3_board), pas invente.
//
// Ecran repris de test_screen_s3.cpp (ips=true confirme sur materiel reel
// le 26 aout 2026) : chaque appui dessine un repere a l'endroit touche,
// pour verifier l'orientation a l'oeil sans avoir besoin du port serie.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>

// Brochage/parametres carte : voir la section [jc4827w543c_i] de
// platformio.ini - source de verite unique, partagee par tous les
// env test_*_s3, plutot que des #define disperses et divergents.
#define TOUCH_SDA AQ_S3_TOUCH_SDA
#define TOUCH_SCL AQ_S3_TOUCH_SCL
#define TOUCH_INT AQ_S3_TOUCH_INT
#define TOUCH_RST AQ_S3_TOUCH_RST
#define TOUCH_WIDTH AQ_S3_SCREEN_WIDTH
#define TOUCH_HEIGHT AQ_S3_SCREEN_HEIGHT

#define LCD_BL AQ_S3_LCD_BL
#define LCD_BL_CHANNEL AQ_S3_LCD_BL_CHANNEL

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);
Arduino_NV3041A *panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS /* confirme le 26 aout */);
Arduino_GFX *gfx = new Arduino_Canvas(AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT, panel);

TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

static void drawIdle() {
    gfx->fillScreen(0x0000);
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->print(F("AquaLook - test tactile GT911 (S3)"));
    gfx->setTextSize(1);
    gfx->setCursor(10, 40);
    gfx->print(F("I2C SCL=4 SDA=8 RST=38 INT=3"));
    gfx->setCursor(10, 60);
    gfx->print(F("En attente d'un contact - touchez l'ecran"));
    gfx->setCursor(10, 250);
    gfx->print(F("Critere : le repere apparait sous le doigt, sans etalonnage"));
    gfx->flush();
}

static void drawTouch(int16_t x, int16_t y, uint8_t index) {
    gfx->fillScreen(0x0000);
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->print(F("AquaLook - test tactile GT911 (S3)"));
    gfx->setTextSize(1);
    gfx->setCursor(10, 40);
    gfx->printf("Contact #%d : x=%d y=%d (ecran %dx%d)", index, x, y,
                AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT);

    // Reperes visuels : croix pleine + cercle a l'endroit touche, pour
    // juger l'orientation/la precision a l'oeil.
    gfx->drawFastHLine(0, y, AQ_S3_SCREEN_WIDTH, 0x8410);
    gfx->drawFastVLine(x, 0, AQ_S3_SCREEN_HEIGHT, 0x8410);
    gfx->fillCircle(x, y, 10, 0x07E0);
    gfx->drawCircle(x, y, 10, 0xFFFF);

    gfx->setCursor(10, 250);
    gfx->print(F("Le repere doit apparaitre exactement sous le doigt"));
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
    Serial.println(F("=== AquaLook - bring-up ESP32-S3 JC4827W543C_I - test 5/9 ==="));
    Serial.println(F("Tactile GT911 : SCL=GPIO4 SDA=GPIO8 RST=GPIO38 INT=GPIO3"));

    ledcSetup(LCD_BL_CHANNEL, 5000, 12);
    ledcAttachPin(LCD_BL, LCD_BL_CHANNEL);
    ledcWrite(LCD_BL_CHANNEL, 4095);

    if (!gfx->begin()) {
        Serial.println(F(">>> ECHEC gfx->begin() - impossible de continuer ce test"));
        while (true) {
            delay(1000);
        }
    }

    ts.begin();
    // AQ_S3_TOUCH_ROTATION=1 (ROTATION_INVERTED, TAMC_GT911.h) : la valeur
    // par defaut ROTATION_NORMAL=3 donnait les deux axes inverses (rotation
    // 180) par rapport a l'ecran sur cette carte reelle, constate le
    // 27 aout 2026.
    ts.setRotation(AQ_S3_TOUCH_ROTATION);

    drawIdle();
    Serial.println(F("Pret. Touchez l'ecran - coordonnees imprimees ici et affichees a l'ecran."));
}

void loop() {
    ts.read();
    if (ts.isTouched) {
        // ts.touches a ete observe a 14 sur cette carte reelle (27 aout
        // 2026) alors que TAMC_GT911.h ne declare que points[5] - une
        // boucle for(i<ts.touches) lisait hors tableau (memoire
        // aleatoire), d'ou le chaos visuel constate. On se limite ici a
        // un seul contact (suffisant pour verifier la precision/
        // l'orientation) et on borne par securite a la taille reelle du
        // tableau.
        uint8_t n = ts.touches;
        if (n > 5) n = 5;
        if (n >= 1) {
            int16_t x = ts.points[0].x;
            int16_t y = ts.points[0].y;
            Serial.printf(">>> Contact (ts.touches brut=%d, borne=%d) : x=%d y=%d\n",
                          ts.touches, n, x, y);
            drawTouch(x, y, 0);
        }
        delay(80);  // limite le taux de rafraichissement, lisible a l'oeil
    }
}
