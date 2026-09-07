// Test cible du 27 aout 2026 - suite de test_direct_s3.cpp : les
// primitives directes marchent isolement, ET un seul sprite cree/
// dessine/pousse une fois marche aussi (meme avec WiFi AP actif). Mais
// rien ne s'affiche sur l'ecran HOME reel au-dela du fillScreen initial.
// Ce test reproduit plus fidelement le pattern exact de DisplayManager :
// 4 sprites COEXISTANTS (memes dimensions que production), et le
// "bouton" reutilise DEUX FOIS dans la meme frame (Z1 puis Z2) via
// fillSprite+dessin+pushImage(x,y,w,visibleH) - pas juste pushSprite
// simple. Isole si c'est la coexistence de plusieurs sprites ou la
// reutilisation qui declenche le probleme, plutot qu'un seul sprite
// simple comme le test precedent.

#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;
TFT_eSprite sprTime(&tft);
TFT_eSprite sprSignal(&tft);
TFT_eSprite sprPlan(&tft);
TFT_eSprite sprBtn0(&tft);

static void renderBtn(uint8_t zone, uint16_t pushX, uint16_t pushY, uint16_t w, uint16_t h) {
    sprBtn0.fillSprite(TFT_BLACK);
    sprBtn0.fillRoundRect(2, 2, w - 4, h - 4, 10, zone == 0 ? TFT_GREEN : TFT_BLUE);
    sprBtn0.setTextColor(TFT_WHITE, zone == 0 ? TFT_GREEN : TFT_BLUE);
    sprBtn0.setTextSize(2);
    sprBtn0.setTextDatum(MC_DATUM);
    char label[8];
    snprintf(label, sizeof(label), "Z%d", zone + 1);
    sprBtn0.drawString(label, w / 2, h / 2);

    // Meme pattern exact que DisplayManager::renderBtnSprite() :
    // nettoyer la colonne puis pousser via pushImage (pas pushSprite).
    tft.fillRect(pushX, pushY, w, 272 - pushY, TFT_BLACK);
    void *ptr = sprBtn0.getPointer();
    Serial.printf("renderBtn z=%u pushX=%u pushY=%u ptr=%p\n", zone, pushX, pushY, ptr);
    tft.pushImage(pushX, pushY, w, h, static_cast<uint16_t *>(ptr));
}

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== Test 4 sprites coexistants + reutilisation - 27 aout 2026 ==="));

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    Serial.println(F("tft.init() + fillScreen(NOIR) fait"));

    // Creation des 4 sprites, memes dimensions que DisplayManager::createSprites().
    void *pTime   = sprTime.createSprite(110, 20);
    void *pSignal = sprSignal.createSprite(20, 16);
    void *pPlan   = sprPlan.createSprite(320, 90);
    void *pBtn0   = sprBtn0.createSprite(154, 120);
    Serial.printf("Sprites: time=%p signal=%p plan=%p btn0=%p\n", pTime, pSignal, pPlan, pBtn0);

    // Header direct, comme drawHomeFull_list().
    tft.fillRect(0, 0, 480, 28, TFT_WHITE);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Header direct", 10, 4);

    // sprTime : contenu + push, comme renderTimeSprite().
    sprTime.fillSprite(TFT_WHITE);
    sprTime.setTextColor(TFT_BLACK, TFT_WHITE);
    sprTime.setTextSize(2);
    sprTime.setTextDatum(MC_DATUM);
    sprTime.drawString("12:34", 55, 10);
    sprTime.pushSprite(182, 6);
    Serial.println(F("sprTime pousse a 182,6"));

    // sprSignal : idem.
    sprSignal.fillSprite(TFT_WHITE);
    sprSignal.fillCircle(10, 8, 6, TFT_BLUE);
    sprSignal.pushSprite(296, 6);
    Serial.println(F("sprSignal pousse a 296,6"));

    // sprPlan : idem.
    sprPlan.fillSprite(TFT_YELLOW);
    sprPlan.setTextColor(TFT_BLACK, TFT_YELLOW);
    sprPlan.drawString("PLAN", 10, 10);
    sprPlan.pushSprite(0, 28);
    Serial.println(F("sprPlan pousse a 0,28"));

    // sprBtn0 : reutilise deux fois, comme renderBtnSprite() pour Z1 puis Z2.
    renderBtn(0, 2, 94, 154, 120);
    renderBtn(1, 162, 94, 154, 120);

    Serial.println(F("Sequence complete - header blanc, heure, signal, plan jaune, 2 boutons attendus"));
}

void loop() {
    delay(1000);
}
