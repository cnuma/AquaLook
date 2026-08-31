#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>
#if AQUALOOK_TOUCH_GT911
#include <TAMC_GT911.h>
#else
#include <XPT2046_Touchscreen.h>
#endif
#include "config.h"
#include "ScreenGeometry.h"
#include "NTPManager.h"
#include "WeatherManager.h"
#include "RelaisManager.h"
#include "ScheduleManager.h"
#include "ConfigManager.h"
#include "ScreenManager.h"
#include "FaultManager.h"
#include "EventLog.h"
#include "TimeUtils.h"
#include "WiFiManager.h"
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
    // Dimensions de la dalle. Publiques (et non un #define local a
    // DisplayManager.cpp comme avant le 28 aout 2026) parce que d'autres
    // fichiers dessinent sur le meme ecran - DisplaySplashWrap.cpp,
    // DisplayPlanningDecor.cpp - et recopiaient jusqu'ici 320/240 a la
    // main, valeurs qui deviennent fausses sur l'ESP32-S3 (480x272).
    // Les valeurs elles-memes vivent dans ScreenGeometry.h, qui peut etre
    // inclus par TJpg_Decoder.h la ou cet en-tete-ci ne le peut pas.
    static constexpr uint16_t SCREEN_W = AquaLook::Panel::WIDTH;
    static constexpr uint16_t SCREEN_H = AquaLook::Panel::HEIGHT;

    /// Remplit l'ecran de violet avec un message, et rallume le
    /// retroeclairage. Appelee juste avant le redemarrage en maintenance :
    /// la dalle n'ayant pas de broche de reset, cette image survit au
    /// redemarrage de l'ESP32 et reste affichee pendant toute la mise a
    /// jour, ou plus aucun code d'affichage ne tourne.
    void showUpdateScreen(const char* title, const char* message);

    /// Fait clignoter en blanc la LED de la zone sur le ruban WS2812, pour
    /// l'identifier physiquement au moment du raccordement. S'arrête seule
    /// au bout de durationMs. zone >= MAX_ZONES annule l'identification.
    /// Sans effet visible sur la carte historique, qui n'a pas de ruban.
    void identifyZone(uint8_t zone, uint32_t durationMs) {
        _screenMgr.identifyZone(zone, durationMs);
    }

    // ── Splash screen (boot) ──────────────────
    /// Appelé AVANT begin() — initialise juste le TFT + LittleFS
    void initTft();
    /// Affiche l'image splash + barre de progression
    /// step 0..SPLASH_STEPS-1, label = nom de l'étape courante
    void showSplash(uint8_t step, const char* label);

    void begin(NTPManager* ntp, WeatherManager* weather,
               RelaisManager* relais, ScheduleManager* schedule,
               ConfigManager* config, WiFiManager* wifi = nullptr);
    void update();

    // Demande un rafraichissement dynamique au prochain passage dans update().
    // Contrairement a EventBus::displayDirty, cette demande ne provoque ni
    // fillScreen() ni rendu complet et evite le scintillement lors des
    // transitions normales ON/OFF.
    void requestDynamicRefresh() {
        _lastUpdate = millis() - _refreshNomMs;
    }

    void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
        _outputs = outputs;
        _relais.outputs = outputs;
    }

    static constexpr uint8_t SPLASH_STEPS = 8;

    // Libere/recree temporairement _sprBtn0 (154x120, ~37 Ko) ET _sprPlan
    // (320x90, ~58 Ko), ~95 Ko au total — voir ROADMAP.md, "constat du 16
    // aout 2026" : un premier essai avec _sprBtn0 seul (~37 Ko) a change
    // l'erreur mbedTLS (memoire -> certificat) mais pas suffit ; le test de
    // reference reussi (CHECK_VERSION en mode maintenance) disposait de
    // ~245 Ko libres contre ~17 Ko ici au repos, d'ou l'ajout de _sprPlan.
    // A appeler UNIQUEMENT depuis la boucle principale, jamais depuis un
    // callback AsyncTCP : TFT_eSprite n'est pas thread-safe, et libere/
    // recreer pendant qu'un rendu concurrent y dessine toucherait un
    // pointeur invalide. Dans ce projet, WebManager::update() et
    // DisplayManager::update() sont tous deux appeles depuis loop()
    // (main.cpp), jamais depuis une autre tache : suspendre puis reprendre
    // a l'interieur d'un seul appel a WebManager::update(), avant que
    // DisplayManager::update() ne soit rappele dans la meme iteration, est
    // donc sur. Pendant la suspension, tout rendu qui toucherait ces
    // sprites planterait — ne rien faire d'autre entre les deux appels.
    // Idempotents : l'etat reel est suivi par _spritesFreed, si bien qu'un
    // double appel ne fuit pas (createSprite sur un sprite deja alloue
    // perdrait l'ancien tampon) et qu'un appel redondant ne coute rien.
    void suspendForMemoryRelief() {
        if (_spritesFreed) return;
        _sprBtn0.deleteSprite();
        _sprPlan.deleteSprite();
        _spritesFreed = true;
    }
    // Retourne false si l'un des tampons n'a pas pu etre alloue. Dans ce cas
    // _spritesFreed reste vrai : rien n'est dessine ce tour-ci, et une nouvelle
    // tentative aura lieu plus tard — le systeme se repare seul des que la
    // memoire se libere.
    //
    // Sans ce controle, l'echec serait totalement muet : TFT_eSprite teste
    // _created dans chaque primitive et sort sans rien faire, donc l'ecran
    // resterait fige sur son dernier contenu, sans erreur ni trace. C'est
    // exactement le type de panne silencieuse que l'on veut supprimer.
    bool resumeAfterMemoryRelief() {
        if (!_spritesFreed) return true;

        const bool btnOk  = _sprBtn0.createSprite(PL_BTN_W, PL_BTN_H) != nullptr;
        const bool planOk = _sprPlan.createSprite(PL_PLAN_W, PL_PLAN_H) != nullptr;

        if (!btnOk || !planOk) {
            // Liberer le tampon partiellement obtenu : le garder ne servirait
            // a rien et retiendrait de la memoire dont l'autre a besoin.
            _sprBtn0.deleteSprite();
            _sprPlan.deleteSprite();
            FaultManager::setActive(FaultId::DISPLAY_ALLOC, true);
            FaultManager::notifyError();
            EventLog::log(LOG_ERROR,
                          "Affichage: allocation des tampons impossible "
                          "(bouton=%s planning=%s), rendu suspendu",
                          btnOk ? "ok" : "echec",
                          planOk ? "ok" : "echec");
            return false;   // _spritesFreed reste vrai -> nouvelle tentative
        }

        _spritesFreed = false;
        FaultManager::setActive(FaultId::DISPLAY_ALLOC, false);
        return true;
    }

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
#if AQUALOOK_TOUCH_GT911
    // Pas de bus SPI dedie au tactile en GT911 (I2C) ; VSPI n'existe de
    // toute facon pas sur ESP32-S3 (nommage different des peripheriques
    // SPI materiels par rapport a l'ESP32 d'origine).
    TAMC_GT911 _touch { AQ_S3_TOUCH_SDA, AQ_S3_TOUCH_SCL, AQ_S3_TOUCH_INT,
                         AQ_S3_TOUCH_RST, AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT };
#else
    SPIClass            _touchSPI { VSPI };
    XPT2046_Touchscreen _touch    { TOUCH_CS, TOUCH_IRQ };
#endif

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
    WiFiManager*     _wifi     = nullptr;
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
    // Les deux gros sprites (~95 Ko a eux deux) sont-ils actuellement liberes ?
    // Voir suspendForMemoryRelief() et l'invariant applique dans update().
    bool      _spritesFreed = false;
    uint32_t  _lastSpriteRetryMs = 0;
    static constexpr uint32_t SPRITE_RETRY_INTERVAL_MS = 2000UL;
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
#if AQUALOOK_BOARD_S3
    static constexpr uint16_t PL_PLAN_H   = 126;  // 62 + 4*16
#else
    static constexpr uint16_t PL_PLAN_H   = 90;   // hauteur sprite planning
#endif
    // Largeur du sprite planning. Doit imperativement etre utilisee A LA
    // FOIS a la creation du sprite et au pushImage() qui l'envoie a
    // l'ecran : le tampon est lineaire (largeur x hauteur x 2 octets,
    // sans remplissage de fin de ligne), donc pousser une largeur
    // differente de celle allouee decale chaque ligne par rapport a la
    // precedente. C'etait le cas avant le 28 aout 2026 (createSprite(320)
    // mais pushImage(SCREEN_W)) : invisible sur la carte historique ou
    // SCREEN_W vaut justement 320, mais sur l'ESP32-S3 (SCREEN_W=480) le
    // bandeau partait en cisaillement - traits verticaux en pointilles
    // obliques et libelles de jours haches, constate sur materiel reel.
    // 7 colonnes de PL_DAY_W + PL_LABEL_W = 316, d'ou 320.
    static constexpr uint16_t PL_PLAN_W   = SCREEN_W;  // largeur sprite planning
#if AQUALOOK_BOARD_S3
    // 480x272 : l'en-tete meteo est nettement plus haut qu'en 320x240 pour
    // porter, par jour, ce que montre deja la page Web - icone, pastilles
    // de temperature min et max, vent (fleche + cardinal + km/h) et pluie
    // en mm avec sa jauge. Les 65 px de large par colonne (contre 42) le
    // permettent enfin.
    static constexpr uint16_t PL_HDR_H    = 62;
    static constexpr uint16_t PL_ZONE_H   = 16;
#else
    static constexpr uint16_t PL_HDR_H    = 28;   // ligne jours + icônes météo
    static constexpr uint16_t PL_ZONE_H   = 15;   // hauteur d'une ligne zone planning
#endif
    static constexpr uint16_t PL_Z0_ROW_Y = 28;   // = PL_HDR_H
    static constexpr uint16_t PL_Z1_ROW_Y = 43;
    static constexpr uint16_t PL_Z2_ROW_Y = 58;
    static constexpr uint16_t PL_Z3_ROW_Y = 73;
    static constexpr uint16_t PL_LABEL_W  = 22;
    // Derivee de la largeur d'ecran plutot que figee : redonne exactement
    // 42 px sur la carte historique ((320-22)/7), et 65 px sur la
    // JC4827W543C_I ((480-22)/7). Aucun ecart pour la carte de production,
    // et l'espace supplementaire du 480 profite automatiquement aux
    // colonnes meteo.
    static constexpr uint16_t PL_DAY_W    = (SCREEN_W - PL_LABEL_W) / 7;
    // Bandeau d'en-tete : elements cales sur le bord DROIT de l'ecran.
    // Exprimes en retrait depuis SCREEN_W et non en absolu (182/296/285
    // avant le 28 aout 2026) - ces valeurs figees valaient pour une dalle
    // de 320 px et laissaient l'horloge et l'icone signal flotter au
    // milieu du bandeau sur l'ESP32-S3 (480 px). Les retraits ci-dessous
    // redonnent exactement les anciennes positions quand SCREEN_W = 320.
    static constexpr uint16_t HDR_TIME_W   = 110;  // largeur sprite heure
    static constexpr uint16_t HDR_SIGNAL_W = 20;   // largeur sprite signal
    static constexpr uint16_t HDR_SIGNAL_X = SCREEN_W - HDR_SIGNAL_W - 4;
    static constexpr uint16_t HDR_TIME_X   = SCREEN_W - HDR_TIME_W - 28;
    static constexpr uint16_t HDR_UPDATE_X = SCREEN_W - 35;  // pastille MAJ
    // Boutons zones (1-2 zones, sprites larges)
#if AQUALOOK_BOARD_S3
    // Deux cartes de 228 px separees et bordees de 8 px de marge :
    // 8 + 228 + 8 + 228 + 8 = 480. Elles gagnent 74 px de large et 36 de
    // haut par rapport au 320x240, ce qui laisse enfin la place a une
    // vraie hierarchie visuelle plutot qu'a trois lignes serrees.
    static constexpr uint16_t PL_BTN_Y    = 128;
    static constexpr uint16_t PL_BTN_W    = 228;
    static constexpr uint16_t PL_BTN_H    = 138;
    static constexpr uint16_t PL_BTN_Z1_X = 8;
    static constexpr uint16_t PL_BTN_Z2_X = 244;
    static constexpr uint16_t PL_CBTN_W   = 117;  // 4 colonnes : 4*117 + 3*4 = 480
    static constexpr uint16_t PL_CBTN_H   = 138;
    static constexpr uint16_t PL_CBTN_Y   = 128;
    static constexpr uint16_t PL_CBTN_GAP = 4;
#else
    static constexpr uint16_t PL_BTN_Y    = 119;  // PL_PLAN_Y + PL_PLAN_H + 1
    static constexpr uint16_t PL_BTN_W    = 154;
    static constexpr uint16_t PL_BTN_H    = 120;
    static constexpr uint16_t PL_BTN_Z1_X = 2;
    static constexpr uint16_t PL_BTN_Z2_X = 162;
    static constexpr uint16_t PL_CBTN_W   = 78;
    static constexpr uint16_t PL_CBTN_H   = 120;
    static constexpr uint16_t PL_CBTN_Y   = 119;
    static constexpr uint16_t PL_CBTN_GAP = 2;
#endif
    // PL_PLAN_GAP : était constexpr, maintenant membre runtime _planGap (chargé depuis CfgDisplay)

#if AQUALOOK_BOARD_S3
    static constexpr uint16_t G2_HDR_H     = 28;
    static constexpr uint16_t G2_CONTENT_Y = 28;
    static constexpr uint16_t G2_CONTENT_H = SCREEN_H - 28;
#else
    static constexpr uint16_t G2_HDR_H     = 25;
    static constexpr uint16_t G2_CONTENT_Y = 25;
    static constexpr uint16_t G2_CONTENT_H = 215;
#endif
    static constexpr uint16_t G2_GRID_W    = 255;
    // Geometrie de la grille 5-8 zones : calculee par updateGrid2Geometry()
    // et non figee, pour occuper toute la largeur disponible et adapter la
    // hauteur des cartes au nombre reel de zones. Elle est lue par le
    // dessin, la mise a jour ET le test tactile : une seule source evite
    // qu'un bouton reponde ailleurs qu'a l'endroit ou il s'affiche.
    uint16_t _g2PlanW = 64;
    uint16_t _g2GridX = 65;
    uint16_t _g2Gw    = 126;
    uint16_t _g2Gh    = 50;
    // Geometrie interne de la colonne planning du mode 5-8 zones. Partagee
    // avec DisplayPlanningDecor, qui superpose ses hachures aux cellules
    // tracees par renderPlanSpriteCompact() : ce fichier recopiait
    // labelW=12 / colW=26 / hdrH=42, valeurs calees sur une colonne de
    // 64 px. Passee a 150 px, les hachures tombaient sur la mauvaise
    // journee et ne remplissaient pas la cellule.
    uint16_t _g2GridY = 25;
    uint16_t _g2PlanHdrH  = 42;
    uint16_t _g2PlanColW  = 26;
    uint16_t _g2PlanZoneH = 21;
    static constexpr uint16_t G2_PLAN_LABEL_W = 12;
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
    bool isWifiSearching() const;
    void renderPlanSprite();                                         // LIST : 7 cols, PL_PLAN_H
    void renderPlanSpriteFull(uint16_t destY, uint16_t h,
                               uint8_t zStart, uint8_t zEnd);        // GRID4 : 7 cols, N zones
    void renderPlanSpriteCompact(uint16_t sprH, uint16_t destY,
                                  uint16_t planW = 320);              // GRID2 : 2 cols
    String nextSlotLabel(uint8_t zone);
#if AQUALOOK_BOARD_S3
    // Encart meteo detaille du jour touche dans le bandeau planning.
    // -1 = ferme. Tant qu'il est ouvert, update() suspend le rafraichissement
    // dynamique : sans cela le rendu periodique repasserait par-dessus.
    int8_t _wxPopupDay = -1;
    void drawWeatherPopup(uint8_t dayCol);
    bool handleWeatherPopupTouch(uint16_t tx, uint16_t ty);
#endif
    void updateGrid2Geometry();
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

    // Masque des zones dont l'arrosage du jour est suspendu par la pluie
    // (bit z levé = zone z bloquée). Alimente le voyant WS2812, qui montre
    // ces zones en orange — la même teinte que le planning leur donne déjà.
    // S'appuie sur rainBlocksDay(), le prédicat partagé avec les trois
    // rendus de planning, pour qu'écran et ruban ne puissent pas diverger.
    uint16_t rainBlockedMaskToday();

#if AQUALOOK_BOARD_S3
    // Résultat mémorisé du précédent appel : la fonction recopie le planning
    // complet de chaque zone, trop cher pour chaque tour de boucle, alors que
    // l'état décrit ne bouge qu'au rythme des prévisions météo.
    //
    // Déclarés sous garde : la carte historique n'appelle jamais ce calcul,
    // elle n'a pas à en porter les octets.
    uint16_t _rainMaskCache = 0U;
    uint32_t _rainMaskAtMs  = 0U;
#endif
    const char* adminPageName(AdminPage p);
    float       zonePct(uint8_t zone);  // fraction durée écoulée [0..1]
    void        applyDisplayConfig();   // lit ConfigManager::display() → Theme:: + membres runtime
};
