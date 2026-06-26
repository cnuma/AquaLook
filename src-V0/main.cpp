#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

void setup() {
    Serial.begin(115200);
    delay(2000);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Touch test !", 10, 10);
    tft.drawString("Touche l'ecran", 10, 40);
    tft.drawString("Voir Serial Monitor", 10, 70);

    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin(touchSPI);
    touch.setRotation(1);

    Serial.println("=== Calibration Touch ===");
    Serial.println("Touche differents endroits de l'ecran");
    Serial.println("et note les valeurs x/y min et max");
}

void loop() {
    if (touch.tirqTouched() && touch.touched()) {
        TS_Point p = touch.getPoint();

        int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, 320);
        int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 240);

        // Clamp pour rester dans l'écran
        x = constrain(x, 0, 319);
        y = constrain(y, 0, 239);

        tft.fillCircle(x, y, 4, TFT_GREEN);
        Serial.printf("Ecran: x=%d y=%d\n", x, y);
    }
    delay(20);
}