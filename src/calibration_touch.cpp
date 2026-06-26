/**
 * @file calibration_touch.cpp
 * @brief Sketch autonome de calibration du touch XPT2046 sur ESP32 CYD.
 *
 * Flasher avec l'environnement "calibration" (platformio.ini).
 * Toucher les 4 coins de l'écran quand demandé.
 * Noter les valeurs xMin/xMax/yMin/yMax affichées sur le moniteur série
 * et les reporter dans config.h (TOUCH_X_MIN etc.) ou via /api/touch.
 *
 * NE PAS inclure dans le build principal (build_src_filter l'exclut).
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ── Pins touch CYD (VSPI séparé) ─────────────
#define T_IRQ   36
#define T_MOSI  32
#define T_MISO  39
#define T_CLK   25
#define T_CS    33

TFT_eSPI         tft;
SPIClass         touchSPI(VSPI);
XPT2046_Touchscreen touch(T_CS, T_IRQ);

// ── Suivi min/max ─────────────────────────────
int16_t xMin = 4095, xMax = 0;
int16_t yMin = 4095, yMax = 0;
uint32_t lastPrint = 0;
uint32_t sampleCount = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setFreeFont(&FreeSans9pt7b);

    tft.drawString("Calibration Touch", 10, 10);
    tft.drawString("Toucher tous les coins", 10, 35);
    tft.drawString("et les bords de l'ecran", 10, 55);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Voir Serial Monitor", 10, 85);

    // Croix aux 4 coins
    uint16_t cx[] = {10, 310, 10,  310};
    uint16_t cy[] = {10,  10, 230, 230};
    for (int i = 0; i < 4; i++) {
        tft.drawLine(cx[i]-8, cy[i],   cx[i]+8, cy[i],   TFT_RED);
        tft.drawLine(cx[i],   cy[i]-8, cx[i],   cy[i]+8, TFT_RED);
    }

    touchSPI.begin(T_CLK, T_MISO, T_MOSI, T_CS);
    touch.begin(touchSPI);
    touch.setRotation(1);

    Serial.println("=== Calibration Touch XPT2046 ===");
    Serial.println("Toucher tous les coins + bords de l'écran");
    Serial.println("Les valeurs min/max sont affichées toutes les 2s");
    Serial.println();
}

void loop() {
    if (touch.tirqTouched() && touch.touched()) {
        TS_Point p = touch.getPoint();

        if (p.x < xMin) xMin = p.x;
        if (p.x > xMax) xMax = p.x;
        if (p.y < yMin) yMin = p.y;
        if (p.y > yMax) yMax = p.y;
        sampleCount++;

        // Point visuel sur l'écran
        int16_t sx = map(p.x, xMin, xMax, 0, 319);
        int16_t sy = map(p.y, yMin, yMax, 0, 239);
        sx = constrain(sx, 0, 319);
        sy = constrain(sy, 0, 239);
        tft.fillCircle(sx, sy, 3, TFT_CYAN);
    }

    // Affichage toutes les 2s
    if (millis() - lastPrint >= 2000 && sampleCount > 0) {
        lastPrint = millis();

        Serial.printf("Échantillons : %lu\n", sampleCount);
        Serial.printf("  xMin=%d  xMax=%d\n", xMin, xMax);
        Serial.printf("  yMin=%d  yMax=%d\n", yMin, yMax);
        Serial.println("─── À reporter dans config.h ───");
        Serial.printf("#define TOUCH_X_MIN  %d\n", xMin);
        Serial.printf("#define TOUCH_X_MAX  %d\n", xMax);
        Serial.printf("#define TOUCH_Y_MIN  %d\n", yMin);
        Serial.printf("#define TOUCH_Y_MAX  %d\n", yMax);
        Serial.println();

        // Mise à jour écran
        tft.fillRect(0, 110, 320, 80, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        char buf[32];
        snprintf(buf, sizeof(buf), "xMin=%d xMax=%d", xMin, xMax);
        tft.drawString(buf, 8, 118);
        snprintf(buf, sizeof(buf), "yMin=%d yMax=%d", yMin, yMax);
        tft.drawString(buf, 8, 138);
        snprintf(buf, sizeof(buf), "N=%lu", sampleCount);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(buf, 8, 162);
    }
}