#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"
#include "NTPManager.h"
#include "WeatherManager.h"
#include "RelaisManager.h"
#include "ScheduleManager.h"

// ── Écrans disponibles ─────────────────────────
enum Screen {
    SCREEN_HOME,
    SCREEN_ZONE,
    SCREEN_METEO,
    SCREEN_STATUS
};

class DisplayManager {
public:
    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule);
    void update();

private:
    TFT_eSPI         _tft;
    SPIClass         _touchSPI{VSPI};
    XPT2046_Touchscreen* _touch = nullptr;

    NTPManager*      _ntp      = nullptr;
    WeatherManager*  _weather  = nullptr;
    RelaisManager*   _relais   = nullptr;
    ScheduleManager* _schedule = nullptr;

    Screen   _currentScreen   = SCREEN_HOME;
    uint8_t  _selectedZone    = 0;
    uint32_t _lastDraw        = 0;
    uint32_t _lastTouch       = 0;
    bool     _needsRedraw     = true;

    // ── Rendu écrans ──
    void drawHome();
    void drawZone(uint8_t zone);
    void drawMeteo();
    void drawStatus();

    // ── Composants UI ──
    void drawHeader(const char* title);
    void drawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    const char* label, uint16_t bg, uint16_t fg);
    void drawZoneCard(uint8_t zone, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h);

    // ── Touch ──
    void handleTouch();
    bool isTouched(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   uint16_t tx, uint16_t ty);

    // ── Helpers ──
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
};