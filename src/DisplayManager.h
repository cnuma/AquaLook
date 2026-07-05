#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"
#include "NTPManager.h"
#include "WeatherManager.h"
#include "RelaisManager.h"
#include "ScheduleManager.h"
#include "ConfigManager.h"
#include "ScreenManager.h"

// ═══════════════════════════════════════════════════════════════
//  Layout HOME 320×240 (rotation 1)
//
//  y=  0.. 27  Header 28px — [≡] menu | titre | HH:MM+temp | signal
//  y= 28..127  _sprPlan  320×100  — planning 7j + météo
//  y=128..135  gap 8px
//  y=136..233  _sprBtn0  154×98  push x=2  (Zone 1)
//              puis      154×98  push x=162 (Zone 2) — sprite réutilisé
//  y=234..239  marge 6px
//
//  Layout ADMIN 320×240
//  y=  0.. 27  Header 28px — [←] retour | ADMIN | [page X/N]
//  y= 28..199  Contenu page courante
//  y=200..239  Barre navigation — [<] | [page N nom] | [>]
//
//  Politique de refresh :
//    Nominal              → DISPLAY_REFRESH_MS (5s)
//    Arrosage actif       → DISPLAY_REFRESH_ACTIVE_MS (1s)   invariant I21
//    EventBus::displayDirty → immédiat (prochain tick update())
// ═══════════════════════════════════════════════════════════════

enum class Screen : uint8_t {
    HOME, ZONE, STATUS, SYSTEM, ADMIN
};

enum class HomeMode : uint8_t {
    LIST,
    GRID2,
    GRID4
};

enum class AdminPage : uint8_t {
    WIFI   = 0,
    NTP    = 1,
    OWM    = 2,
    ZONES  = 3,
    SYSTEM = 4,
    LOGS   = 5,
    _COUNT = 6
};

class DisplayManager {
public:
    void initTft();
    void showSplash(uint8_t step, const char* label);

    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule,
               ConfigManager* config);
    void update();

    static constexpr uint8_t SPLASH_STEPS = 8;

private:
    TFT_eSPI            _tft;
    SPIClass            _touchSPI { VSPI };
    XPT2046_Touchscreen _touch    { TOUCH_CS, TOUCH_IRQ };

    TFT_eSprite _sprTime   { &_tft };
    TFT_eSprite _sprSignal { &_tft };
    TFT_eSprite _sprPlan   { &_tft };
    TFT_eSprite _sprBtn0   { &_tft };
    bool        _spritesReady  = false;
    bool        _splashActive  = false;
    bool        _tftInited     = false;

    NTPManager*      _ntp      = nullptr;
    WeatherManager*  _weather  = nullptr;
    RelaisManager*   _relais   = nullptr;
    ScheduleManager* _schedule = nullptr;
    ConfigManager*   _config   = nullptr;
    ScreenManager    _screenMgr;

    Screen    _screen          = Screen::HOME;
    uint8_t   _selectedZone    = 0;
    AdminPage _adminPage       = AdminPage::WIFI;
    HomeMode  _homeMode        = HomeMode::LIST;
    uint8_t   _nbZones         = 2;
    uint8_t   _listScrollOff   = 0;
    bool      _listShowForce   = false;
    uint8_t   _grid4View       = 0;
    bool      _needsFullRedraw = true;
    uint32_t  _lastUpdate      = 0;
    uint32_t  _lastTouch       = 0;
    uint32_t  _lastTap         = 0;

    struct HomeCache {
        String  hhMM       = "";
        bool    z0Active   = false;
        bool    z1Active   = false;
        uint8_t z0Pct      = 255;
        uint8_t z1Pct      = 255;
        int8_t  rssi       = 0;
        float   rainMm     = -1.0f;
        bool    ntpSynced  = false;
        int8_t  todayIdx   = -99;
    } _hc;

    static constexpr uint16_t PL_PLAN_Y   = 28;
    static constexpr uint16_t PL_PLAN_H   = 90;
    static constexpr uint16_t PL_HDR_H    = 28;
    static constexpr uint16_t PL_ZONE_H   = 15;
    static constexpr uint16_t PL_Z0_ROW_Y = 28;
    static constexpr uint16_t PL_Z1_ROW_Y = 43;
    static constexpr uint16_t PL_Z2_ROW_Y = 58;
    static constexpr uint16_t PL_Z3_ROW_Y = 73;
    static constexpr uint16_t PL_DAY_W    = 42;
    static constexpr uint16_t PL_LABEL_W  = 22;
    static constexpr uint16_t PL_BTN_Y    = 119;
    static constexpr uint16_t PL_BTN_W    = 154;
    static constexpr uint16_t PL_BTN_H    = 120;
    static constexpr uint16_t PL_BTN_Z1_X = 2;
    static constexpr uint16_t PL_BTN_Z2_X = 162;
    static constexpr uint16_t PL_CBTN_W   = 78;
    static constexpr uint16_t PL_CBTN_H   = 120;
    static constexpr uint16_t PL_CBTN_Y   = 119;
    static constexpr uint16_t PL_CBTN_GAP = 2;

    static constexpr uint16_t G2_HDR_H     = 25;
    static constexpr uint16_t G2_CONTENT_Y = 25;
    static constexpr uint16_t G2_CONTENT_H = 215;
    static constexpr uint16_t G2_PLAN_W    = 64;
    static constexpr uint16_t G2_GRID_X    = 65;
    static constexpr uint16_t G2_GRID_W    = 255;
    static constexpr uint16_t G2_GW        = 126;
    static constexpr uint16_t G2_GH        = 50;

    static constexpr uint16_t G4_HDR_H     = 20;
    static constexpr uint16_t G4_CONTENT_Y = 20;
    static constexpr uint16_t G4_CONTENT_H = 198;
    static constexpr uint16_t G4_TAB_Y     = 218;
    static constexpr uint16_t G4_TAB_H     = 22;
    static constexpr uint16_t G4_GW        = 78;
    static constexpr uint16_t G4_GH        = 48;
    static constexpr uint16_t G4_PLAN_HDR_H  = 28;
    static constexpr uint16_t G4_PLAN_ZONE_H = 21;

    static constexpr uint16_t ADM_CONTENT_Y = 28;
    static constexpr uint16_t ADM_CONTENT_H = 172;
    static constexpr uint16_t ADM_NAV_Y     = 200;
    static constexpr uint16_t ADM_NAV_H     = 40;

    uint32_t _refreshNomMs = 5000;
    uint32_t _refreshActMs = 1000;
    uint8_t  _planGap      = 6;
    uint8_t  _g2Gpad       = 1;
    uint8_t  _g4Gpad       = 1;
    uint8_t  _planHdrH     = 28;
    uint8_t  _planZoneH    = 15;

    void drawHomeFull();
    void drawHomeFull_list();
    void drawHomeFull_grid2();
    void drawHomeFull_grid4();
    void drawZoneFull(uint8_t zone);
    void drawStatusFull();
    void drawSystemFull();
    void drawAdminFull();

    void updateHomeDynamic();
    void updateHomeDynamic_list();
    void updateHomeDynamic_grid2();
    void updateHomeDynamic_grid4();
    void updateZoneDynamic(uint8_t zone);
    void updateStatusDynamic();
    void updateSystemDynamic();
    void updateAdminDynamic();

    void createSprites();
    void renderTimeSprite();
    void renderSignalSprite();
    void renderPlanSprite();
    void renderPlanSpriteFull(uint16_t destY, uint16_t h,
                              uint8_t zStart, uint8_t zEnd);
    void renderPlanSpriteCompact(uint16_t sprH, uint16_t destY,
                                 uint16_t planW = 320);
    void renderBtnSprite(uint8_t zone, uint16_t pushY = PL_BTN_Y);

    void drawAdminHeader();
    void drawAdminNav();
    void drawAdminPageContent();
    void drawAdminPageWifi();
    void drawAdminPageNtp();
    void drawAdminPageOwm();
    void drawAdminPageZones();
    void drawAdminPageSystem();
    void drawAdminPageLogs();

    void handleTouch();
    void handleHomeTouch(int16_t x, int16_t y);
    void handleZoneTouch(int16_t x, int16_t y);
    void handleAdminTouch(int16_t x, int16_t y);

    void applyDisplayConfig();
    void drawHeader(const char* title, bool showBack = false);
    void drawSplashBar(uint8_t step, const char* label);
    static bool tftOutputCallback(int16_t x, int16_t y,
                                  uint16_t w, uint16_t h,
                                  uint16_t* bitmap);
};
