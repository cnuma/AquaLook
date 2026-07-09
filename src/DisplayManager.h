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
#include "EquipmentOutputRuntimeAdapter.h"

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

// ── Écrans disponibles ─────────────────────────
enum class Screen : uint8_t {
    HOME, ZONE, STATUS, SYSTEM, ADMIN
};

// ── Mode layout HOME selon nb zones ───────────
// Sélectionné automatiquement dans begin() depuis configMgr.nbZones()
enum class HomeMode : uint8_t {
    LIST,    // 1-4 zones  : liste scrollable, planning J+J+1
    GRID2,   // 5-8 zones  : 2 colonnes, météo bande fine
    GRID4    // 9-16 zones : grille 4×N, status bar
};

// ── Pages ADMIN ────────────────────────────────
enum class AdminPage : uint8_t {
    WIFI   = 0,   // SSID actuel + bouton portail captif
    NTP    = 1,   // serveur + GMT + DST
    OWM    = 2,   // clé masquée + lat/lon + unités
    ZONES  = 3,   // noms Z1/Z2 + durée max
    SYSTEM = 4,   // IP, RAM, uptime, reset
    LOGS   = 5,   // journal d'événements EventLog (session courante)
    _COUNT = 6
};

class DisplayManager {
public:
    // ── Splash screen (boot) ──────────────────
    /// Appelé AVANT begin() — initialise juste le TFT + LittleFS
    void initTft();
    /// Affiche l'image splash + barre de progression
    /// step 0..SPLASH_STEPS-1, label = nom de l'étape courante
    void showSplash(uint8_t step, const char* label);

    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule,
               ConfigManager* config);
    void update();

    void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
        _outputs = outputs;
        _relais.outputs = outputs;
    }

    static constexpr uint8_t SPLASH_STEPS = 8;

private:
    struct OutputAwareRelayState {
        RelaisManager* relay = nullptr;
        AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs = nullptr;

        OutputAwareRelayState& operator=(RelaisManager* value) {
            relay = value;
            return *this;
        }

        explicit operator bool() const {
            return relay != nullptr;
        }

        OutputAwareRelayState* operator->() {
            return this;
        }

        const OutputAwareRelayState* operator->() const {
            return this;
        }

        bool getState(uint8_t zone) const {
            if (outputs) {
                const AquaLook::Domain::EquipmentStateValue state =
                    outputs->getZoneValveState(zone);

                if (state.validity == AquaLook::Domain::StateValidity::VALID &&
                    state.kind == AquaLook::Domain::StateValueKind::BINARY) {
                    return state.value != 0;
                }
            }

            return relay ? relay->getState(zone) : false;
        }
    };

    // ── Hardware ──────────────────────────────
    TFT_eSPI            _tft;
    SPIClass            _touchSPI { VSPI };
    XPT2046_Touchscreen _touch    { TOUCH_CS, TOUCH_IRQ };

    // ── Sprites HOME ──────────────────────────
    // Invariant I15 : sprite bouton unique, rendu successif Z1 puis Z2
    TFT_eSprite _sprTime   { &_tft };  //  88×16
    TFT_eSprite _sprSignal { &_tft };  //  20×16
    TFT_eSprite _sprPlan   { &_tft };  // 320×100
    TFT_eSprite _sprBtn0   { &_tft };  // 154×98  (réutilisé Z1+Z2)
    bool        _spritesReady  = false;
    bool        _splashActive  = false;
    bool        _tftInited     = false;

    // ── Managers ──────────────────────────────
    NTPManager*      _ntp      = nullptr;
    WeatherManager*  _weather  = nullptr;
    OutputAwareRelayState _relais;
    AquaLook::Runtime::EquipmentOutputRuntimeAdapter* _outputs = nullptr;
    ScheduleManager* _schedule = nullptr;
    ConfigManager*   _config   = nullptr;
    ScreenManager    _screenMgr;  // veille + LED

    // ── État UI ───────────────────────────────
    Screen    _screen          = Screen::HOME;
    uint8_t   _selectedZone    = 0;
    AdminPage _adminPage       = AdminPage::WIFI;
    HomeMode  _homeMode        = HomeMode::LIST;   // calculé dans begin()
    uint8_t   _nbZones         = 2;               // copie locale depuis config
    uint8_t   _listScrollOff   = 0;               // offset scroll liste (mode LIST)
    bool      _listShowForce   = false;           // LIST >4z / GRID2 : sous-vue marche forcée
    uint8_t   _grid4View       = 0;               // GRID4 : 0=plan Z1-8, 1=plan Z9-16, 2=marche forcée
    bool      _needsFullRedraw = true;
    uint32_t  _lastUpdate      = 0;
    uint32_t  _lastTouch       = 0;
    uint32_t  _lastTap         = 0;   // debounce action touch

    // ── Cache HOME (invariant I16 — redraw boutons seuil 2%) ──
    struct HomeCache {
        String  hhMM       = "";
        bool    z0Active   = false;
        bool    z1Active   = false;
        uint8_t z0Pct      = 255;
        uint8_t z1Pct      = 255;
        int8_t  rssi       = 0;
        float   rainMm     = -1.0f;
        bool    ntpSynced  = false;
        int8_t  todayIdx   = -99;  // force re-render planning au premier tick synced
    } _hc;

    // ── Constantes layout HOME ─────────────────
    //
    //  Mode LIST — 1-2 zones (sprites larges) :
    //    y=  0.. 27  Header         28px
    //    y= 28..117  _sprPlan       90px  (PL_PLAN_H)
    //      L HDR jours+météo  0..27  28px  (PL_HDR_H)
    //      L Zone 0..N row    28..27+n*15  15px/zone  (PL_ZONE_H)
    //    y=119..239  _sprBtn0 x2   121px  (PL_BTN_H)
    //
    //  Mode LIST — 3-4 zones (boutons compacts côte à côte) :
    //    y=  0.. 27  Header         28px
    //    y= 28..117  _sprPlan       90px
    //    y=119..239  4 × ZoneBtnCompact 78×120px, gap 2px
    //
    //  Mode LIST — >4 zones (2 sous-vues, bouton bascule bas) :
    //    Sous-vue PLAN  : planning seul, pleine hauteur après header
    //    Sous-vue FORCE : drawZoneRow scrollable + bouton bascule bas
    //
    static constexpr uint16_t PL_PLAN_Y   = 28;   // y départ sprite planning
    static constexpr uint16_t PL_PLAN_H   = 90;   // hauteur sprite planning
    static constexpr uint16_t PL_HDR_H    = 28;   // ligne jours + icônes météo
    static constexpr uint16_t PL_ZONE_H   = 15;   // hauteur d'une ligne zone planning
    static constexpr uint16_t PL_Z0_ROW_Y = 28;   // = PL_HDR_H
    static constexpr uint16_t PL_Z1_ROW_Y = 43;
    static constexpr uint16_t PL_Z2_ROW_Y = 58;
    static constexpr uint16_t PL_Z3_ROW_Y = 73;
    static constexpr uint16_t PL_DAY_W    = 42;
    static constexpr uint16_t PL_LABEL_W  = 22;
    // Boutons zones (1-2 zones, sprites larges)
    static constexpr uint16_t PL_BTN_Y    = 119;  // PL_PLAN_Y + PL_PLAN_H + 1
    static constexpr uint16_t PL_BTN_W    = 154;
    static constexpr uint16_t PL_BTN_H    = 120;
    static constexpr uint16_t PL_BTN_Z1_X = 2;
    static constexpr uint16_t PL_BTN_Z2_X = 162;
    static constexpr uint16_t PL_CBTN_W   = 78;
    static constexpr uint16_t PL_CBTN_H   = 120;
    static constexpr uint16_t PL_CBTN_Y   = 119;
    static constexpr uint16_t PL_CBTN_GAP = 2;
    // PL_PLAN_GAP : était constexpr, maintenant membre runtime _planGap (chargé depuis CfgDisplay)

    static constexpr uint16_t G2_HDR_H     = 25;
    static constexpr uint16_t G2_CONTENT_Y = 25;
    static constexpr uint16_t G2_CONTENT_H = 215;
    static constexpr uint16_t G2_PLAN_W    = 64;
    static constexpr uint16_t G2_GRID_X    = 65;
    static constexpr uint16_t G2_GRID_W    = 255;
    static constexpr uint16_t G2_GW        = 126;
    static constexpr uint16_t G2_GH        = 50;
    // G2_GPAD : était constexpr, maintenant membre runtime _g2Gpad

    static constexpr uint16_t G4_HDR_H     = 20;
    static constexpr uint16_t G4_CONTENT_Y = 20;
    static constexpr uint16_t G4_CONTENT_H = 198;
    static constexpr uint16_t G4_TAB_Y     = 218;
    static constexpr uint16_t G4_TAB_H     = 22;
    static constexpr uint16_t G4_GW        = 78;
    static constexpr uint16_t G4_GH        = 48;
    // G4_GPAD : était constexpr, maintenant membre runtime _g4Gpad
    static constexpr uint16_t G4_PLAN_HDR_H  = 28;
    static constexpr uint16_t G4_PLAN_ZONE_H = 21;

    static constexpr uint16_t ADM_CONTENT_Y = 28;
    static constexpr uint16_t ADM_CONTENT_H = 172;
    static constexpr uint16_t ADM_NAV_Y     = 200;
    static constexpr uint16_t ADM_NAV_H     = 40;

    // ── Timing et layout runtime ────────────────────────────────
    // Valeurs par défaut — surchargées par CfgDisplay dans begin()
    // puis à chaque EventBus::displayDirty via applyDisplayConfig().
    uint32_t _refreshNomMs = 5000;  // invariant I21
    uint32_t _refreshActMs = 1000;  // invariant I21 (actif)
    uint8_t  _planGap      = 6;     // air planning / boutons
    uint8_t  _g2Gpad       = 1;     // padding grille GRID2
    uint8_t  _g4Gpad       = 1;     // padding grille GRID4
    uint8_t  _planHdrH     = 28;    // hauteur header planning (dépend des options météo)
    uint8_t  _planZoneH    = 15;    // hauteur ligne zone planning (réduite si temp affichée)

    // ── Rendu complet (sur _needsFullRedraw) ──
    void drawHomeFull();           // dispatcher → mode courant
    void drawHomeFull_list();      // 1-4 zones : liste + planning J/J+1
    void drawHomeFull_grid2();     // 5-8 zones : 2 colonnes
    void drawHomeFull_grid4();     // 9-16 zones : grille 4×N
    void drawZoneFull(uint8_t zone);
    void drawStatusFull();
    void drawSystemFull();
    void drawAdminFull();

    // ── Mise à jour dynamique (périodique) ────
    void updateHomeDynamic();
    void updateHomeDynamic_list();
    void updateHomeDynamic_grid2();
    void updateHomeDynamic_grid4();
    void updateZoneDynamic(uint8_t zone);
    void updateStatusDynamic();
    void updateSystemDynamic();
    void updateAdminDynamic();

    // ── Sprites HOME ──────────────────────────
    void createSprites();
    void renderTimeSprite();
    void renderSignalSprite();
    void renderPlanSprite();                                         // LIST : 7 cols, PL_PLAN_H
    void renderPlanSpriteFull(uint16_t destY, uint16_t h,
                               uint8_t zStart, uint8_t zEnd);        // GRID4 : 7 cols, N zones
    void renderPlanSpriteCompact(uint16_t sprH, uint16_t destY,
                                  uint16_t planW = 320);              // GRID2 : 2 cols
    void renderBtnSprite(uint8_t zone, uint16_t pushY = PL_BTN_Y);

    // ── Pages ADMIN ────────────────────────────
    void drawAdminHeader();
    void drawAdminNav();
    void drawAdminPageContent();
    void drawAdminPageWifi();
    void drawAdminPageNtp();
    void drawAdminPageOwm();
    void drawAdminPageZones();
    void drawAdminPageSystem();
    void drawAdminPageLogs();    // journal EventLog — liste scrollable

    // ── Icônes météo vectorielles ──────────────
    void drawWeatherIcon(TFT_eSprite& spr, uint16_t x, uint16_t y,
                         float rainMm, float tempC, bool valid,
                         bool showTemp = false);  // showTemp=false pour planning compact

    // ── Touch ─────────────────────────────────
    void handleTouch();
    void handleTouchHome(uint16_t tx, uint16_t ty);
    void handleTouchHome_list(uint16_t tx, uint16_t ty);
    void handleTouchHome_grid2(uint16_t tx, uint16_t ty);
    void handleTouchHome_grid4(uint16_t tx, uint16_t ty);
    void handleTouchZone(uint16_t tx, uint16_t ty);
    void handleTouchStatus(uint16_t tx, uint16_t ty);
    void handleTouchSystem(uint16_t tx, uint16_t ty);
    void handleTouchAdmin(uint16_t tx, uint16_t ty);
    bool getTouchPoint(uint16_t& tx, uint16_t& ty);

    // ── Helpers UI ────────────────────────────
    void  goTo(Screen s);
    void  drawZoneRow(uint8_t zone, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h);   // rangée compacte liste (>4z)
    void  drawZoneBtnCompact(uint8_t zone, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h); // bouton dense 3-4z
    void  drawZoneBtn(uint8_t zone, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h);   // bouton grille compact
    void  adminNext();
    void  adminPrev();
    bool  hitTest(uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh,
                  uint16_t tx, uint16_t ty);
    void  drawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const char* label, uint16_t bg, uint16_t fg);
    void  drawHeader(const char* title, bool backBtn = false);
    void  drawSplashBar(uint8_t step, const char* label);
    // Composants visuels réutilisables — redesign session 17/06/2026 :
    // remplacent les aplats fillRoundRect dispersés par un rendu cohérent
    // (profondeur, identité couleur de zone) sans changer la géométrie
    // (x,y,w,h) ni les zones de touch des appelants.
    // Cible générique TFT_eSPI& : TFT_eSprite hérite de TFT_eSPI, donc un
    // même appel fonctionne sur _tft (dessin direct) ou sur un sprite
    // (_sprBtn0, _sprPlan...) sans dupliquer le code.
    void  drawCardBg(TFT_eSPI& gfx, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     uint8_t radius, uint16_t bg, uint16_t border, bool elevated);
    void  drawAccentBar(TFT_eSPI& gfx, uint16_t x, uint16_t y, uint16_t h,
                        uint16_t radius, uint16_t color);
    void  drawMenuIcon(TFT_eSPI& gfx, uint16_t x, uint16_t y, uint16_t color);
    static bool tftOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    int   jsToEsp(int tmWday);
    int   todayEspIdx();
    const char* adminPageName(AdminPage p);
    float       zonePct(uint8_t zone);  // fraction durée écoulée [0..1]
    void        applyDisplayConfig();   // lit ConfigManager::display() → Theme:: + membres runtime
};
