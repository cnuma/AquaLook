#include "DisplayManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "esp_log.h"
#include <WiFi.h>

// ── Couleurs et police — voir Theme.h ───────────
// Les polices GFXFF (FreeSans*, FreeSansBold*) sont déjà incluses
// globalement par TFT_eSPI.h → gfxfont.h — ne pas les réinclure ici
// (redefinition error à la compilation sinon).
#include "Theme.h"
#include "ConfigManager.h"

// ─────────────────────────────────────────────────────────────
//  Conversion couleur #rrggbb → RGB565 (statique, usage interne)
//  Sens inverse (RGB565 → #rrggbb) géré côté serveur / app.js.
// ─────────────────────────────────────────────────────────────
static uint16_t hexToRgb565(const char* hex) {
    if (!hex || hex[0] != '#' || strlen(hex) != 7) return 0;
    unsigned r = 0, g = 0, b = 0;
    sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

#define SCREEN_W   320
#define SCREEN_H   240

// Cache de rendu des boutons de zones.
// Objectif : ne jamais redessiner une carte complète chaque seconde.
// Seules les petites zones contenant le timer et la progression sont rafraîchies.
static int8_t   s_zoneActiveCache[16];
static uint32_t s_zoneRemainSecCache[16];

static void resetZoneRefreshCache() {
    for (uint8_t i = 0; i < 16; ++i) {
        s_zoneActiveCache[i] = -1;
        s_zoneRemainSecCache[i] = UINT32_MAX;
    }
}

// Nom affiché dans les boutons d'action.
// Les bulles colorées restent exclusivement dans la zone planning.
static const char* zoneButtonName(const ConfigManager* config, uint8_t zone,
                                  char* fallback, size_t fallbackLen) {
    if (config) {
        const CfgZone& cfgZone = config->zone(zone);
        if (cfgZone.name[0] != '\0') return cfgZone.name;
    }
    snprintf(fallback, fallbackLen, "Zone %u", (unsigned)(zone + 1));
    return fallback;
}

// ═══════════════════════════════════════════════════════════════
//  Splash screen
// ═══════════════════════════════════════════════════════════════

// Callback TJpgDec → TFT (méthode statique requise par la lib)
bool DisplayManager::tftOutputCallback(int16_t x, int16_t y,
                                        uint16_t w, uint16_t h,
                                        uint16_t* bitmap) {
    if (y >= 240) return true;  // hors écran
    // TFT_eSPI pushImage gère le clipping
    extern TFT_eSPI _tftInstance;  // forward — remplacé par instance membre
    // On passe par un sprite temporaire pour éviter le flicker
    // Non applicable ici : TJpgDec appelle directement le TFT
    return true;
}

// Initialisation TFT minimale AVANT begin() complet
// Permet d'afficher le splash pendant le boot
void DisplayManager::initTft() {
    if (_tftInited) return;
    _tft.init();
    _tft.setRotation(1);
    _tft.fillScreen(TFT_WHITE);
    _tftInited = true;

    // Configurer TJpgDec
    TJpgDec.setJpgScale(1);          // pas de mise à l'échelle — image déjà 320×240
    TJpgDec.setSwapBytes(true);       // ESP32 : inverser octets pour RGB565
    TJpgDec.setCallback(             // callback de rendu pixel par pixel
        [](int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) -> bool {
            if (y >= 200) return true;  // zone réservée barre de progression
            // Écriture directe sur le TFT
            static TFT_eSPI* tftPtr = nullptr;
            // Accès via adresse statique — initialisé lors du premier appel
            if (!tftPtr) {
                // Hack nécessaire : TJpgDec ne passe pas de contexte utilisateur
                // On stocke le pointeur au premier appel via une variable statique
                // initialisée depuis showSplash()
                extern TFT_eSPI* g_tftPtr;
                tftPtr = g_tftPtr;
            }
            if (tftPtr) tftPtr->pushImage(x, y, w, h, bmp);
            return true;
        }
    );
    Serial.println("[Splash] TFT initialisé");
}

// Pointeur global pour le callback TJpgDec (nécessité de la lib)
TFT_eSPI* g_tftPtr = nullptr;

void DisplayManager::showSplash(uint8_t step, const char* label) {
    if (!_tftInited) initTft();

    // Initialiser le pointeur global pour le callback
    g_tftPtr = &_tft;

    if (step == 0) {
        // Première étape : décoder et afficher l'image JPEG
        _tft.fillScreen(TFT_WHITE);

        if (LittleFS.exists("/splash.jpg")) {
            // Décoder depuis LittleFS
            TJpgDec.drawFsJpg(0, 0, "/splash.jpg", LittleFS);
            Serial.println("[Splash] Image JPEG affichée");
        } else {
            // Fallback texte si splash.jpg absent
            _tft.setTextColor(Theme::SPLASH_ACCENT, TFT_WHITE);  // bleu AquaLook
            _tft.setFreeFont(THEME_FONT_SPLASH);
            _tft.setTextSize(1);
            _tft.setTextDatum(MC_DATUM);
            _tft.drawString("AquaLook", 160, 80);
            _tft.setFreeFont(nullptr);
            _tft.setTextSize(1);
            _tft.setTextColor(Theme::SPLASH_MUTED, TFT_WHITE);
            _tft.drawString("IRRIGATION CONTROLLER", 160, 110);
            _tft.setTextSize(1);
            _tft.setTextColor(Theme::SPLASH_MUTED2, TFT_WHITE);
            _tft.drawString("ESP32 | Arduino", 160, 126);
            _tft.setTextDatum(TL_DATUM);
            Serial.println("[Splash] Fallback texte (splash.jpg absent)");
        }
    }

    // Barre de progression en bas — toujours mise à jour
    drawSplashBar(step, label);
}

void DisplayManager::drawSplashBar(uint8_t step, const char* label) {
    // Zone barre : y=200..239 (40px)
    const uint16_t BAR_Y     = 200;
    const uint16_t BAR_H     = 40;
    const uint16_t BAR_PAD_X = 20;
    const uint16_t BAR_W     = SCREEN_W - 2 * BAR_PAD_X;
    const uint16_t BAR_INNER = 10;  // hauteur de la barre de remplissage

    // Fond zone barre
    _tft.fillRect(0, BAR_Y, SCREEN_W, BAR_H, TFT_WHITE);
    _tft.drawFastHLine(0, BAR_Y, SCREEN_W, Theme::SPLASH_TRACK);  // ligne séparatrice grise

    // Label étape centré
    _tft.setFreeFont(nullptr);
    _tft.setTextSize(1);
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextColor(Theme::SPLASH_MUTED, TFT_WHITE);  // gris moyen
    _tft.drawString(label, SCREEN_W / 2, BAR_Y + 6);

    // Fond barre gris clair
    _tft.fillRoundRect(BAR_PAD_X, BAR_Y + 20, BAR_W, BAR_INNER, 5, Theme::SPLASH_TRACK);

    // Remplissage bleu proportionnel à l'étape
    uint16_t filled = (uint16_t)(BAR_W * (step + 1) / SPLASH_STEPS);
    if (filled > 0)
        _tft.fillRoundRect(BAR_PAD_X, BAR_Y + 20, filled, BAR_INNER, 5, Theme::SPLASH_ACCENT);

    // Pourcentage
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", (step + 1) * 100 / SPLASH_STEPS);
    _tft.setTextColor(Theme::SPLASH_ACCENT, TFT_WHITE);
    _tft.setTextDatum(TR_DATUM);
    _tft.drawString(pct, SCREEN_W - BAR_PAD_X, BAR_Y + 6);
    _tft.setTextDatum(TL_DATUM);

    Serial.printf("[Splash] Etape %d/%d : %s\n", step + 1, SPLASH_STEPS, label);
}


// ═══════════════════════════════════════════════════════════════
//  begin()
// ═══════════════════════════════════════════════════════════════
void DisplayManager::begin(NTPManager* ntp, WeatherManager* weather,
                            RelaisManager* relais, ScheduleManager* schedule,
                            ConfigManager* config) {
    _ntp      = ntp;
    _weather  = weather;
    _relais   = relais;
    _schedule = schedule;
    _config   = config;

    // Charger la palette et les tokens de layout depuis la config persistée
    // (avant tout fillScreen — Theme::BG doit avoir la bonne valeur dès maintenant)
    applyDisplayConfig();
    if (!_tftInited) {
        _tft.init();
        _tft.setRotation(1);
        _tftInited = true;
    }
    _tft.fillScreen(Theme::BG);
    _tft.setTextDatum(TL_DATUM);

    // Invariant I5 : XPT2046 direct, bus VSPI séparé
    // Note : le warning addApbChangeCallback vient de TFT_eSPI qui ré-enregistre
    // son callback APB lors du second passage dans begin(). Suppression du log parasite
    // en désactivant temporairement les logs ESP pendant l'init du touch.
    esp_log_level_set("*", ESP_LOG_ERROR);  // masquer le warning doublon SPI
    _touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    esp_log_level_set("*", ESP_LOG_WARN);   // restaurer
    _touch.begin(_touchSPI);
    _touch.setRotation(1);

    // Déterminer le mode HOME selon le nb de zones actives
    _nbZones = _config ? _config->nbZones() : NB_ZONES;
    if (_nbZones > 8) _nbZones = 8;  // mode 16 zones retiré de l'interface
    if (_nbZones <= 4) _homeMode = HomeMode::LIST;
    else               _homeMode = HomeMode::GRID2;

    resetZoneRefreshCache();
    createSprites();
    _screenMgr.begin(_config);
    _needsFullRedraw = true;
    Serial.printf("[Display] Heap libre : %u octets, %d zones, mode=%d\n",
                  ESP.getFreeHeap(), _nbZones, (uint8_t)_homeMode);
    Serial.println("[Display] OK");
}

// ─────────────────────────────────────────────
void DisplayManager::createSprites() {
    _sprTime.createSprite(110, 20);  // heure size2 + température size1 côte à côte
    _sprSignal.createSprite(20, 16);
    _sprPlan.createSprite(320, PL_PLAN_H);
    _sprBtn0.createSprite(PL_BTN_W, PL_BTN_H);
    _spritesReady = true;
}

// ═══════════════════════════════════════════════════════════════
//  update() — boucle principale non bloquante
//
//  Politique de refresh (invariant I21) :
//    - Arrosage actif → 1s
//    - EventBus::displayDirty → immédiat
//    - Nominal → 5s
// ═══════════════════════════════════════════════════════════════
void DisplayManager::update() {
    const uint32_t now = millis();

    // ScreenManager — veille/réveil/LED
    // Vérification sur toutes les zones actives (pas uniquement Z0/Z1)
    bool anyActive = false;
    if (_relais) {
        for (uint8_t z = 0; z < _nbZones; z++) {
            if (_relais->getState(z)) { anyActive = true; break; }
        }
    }
    _screenMgr.update(anyActive);

    // Si en veille : ne pas redessiner, juste gérer le touch pour réveil
    if (_screenMgr.isAsleep()) {
        if (now - _lastTouch >= 80) {
            _lastTouch = now;
            uint16_t tx, ty;
            if (getTouchPoint(tx, ty)) {
                _screenMgr.wakeUp();          // réveil
                _needsFullRedraw = true;       // redraw complet au réveil
                _lastTap = now;                // reset debounce
            }
        }
        return;  // pas de redraw en veille
    }

    // Poll touch (80ms)
    if (now - _lastTouch >= 80) {
        _lastTouch = now;
        handleTouch();
    }

    // Hot-reload des tokens de design — invariant I31 :
    // applyDisplayConfig() DOIT être appelé avant que displayDirty soit
    // consommé et que _needsFullRedraw soit positionné. Sinon le redraw
    // se produit avec les anciennes couleurs (Theme:: pas encore mises à jour).
    if (EventBus::displayDirty) {
        applyDisplayConfig();
    }

    // Redraw immédiat sur EventBus
    if (EventBus::displayDirty) {
        EventBus::displayDirty = false;
        _needsFullRedraw = true;
    }

    // Invariant I4 : fillScreen uniquement ici, sur _needsFullRedraw
    if (_needsFullRedraw) {
        _needsFullRedraw = false;
        _tft.fillScreen(Theme::BG);
        switch (_screen) {
            case Screen::HOME:   drawHomeFull();              break;
            case Screen::ZONE:   drawZoneFull(_selectedZone); break;
            case Screen::STATUS: drawStatusFull();            break;
            case Screen::SYSTEM: drawSystemFull();            break;
            case Screen::ADMIN:  drawAdminFull();             break;
        }
        // Le rendu complet contient déjà toutes les informations dynamiques.
        // Repartir du temps courant évite un second refresh immédiat au boot,
        // qui pouvait recouvrir le haut des boutons avec le sprite planning.
        _lastUpdate = now;
        return;
    }

    uint32_t interval = anyActive ? _refreshActMs : _refreshNomMs;

    if (now - _lastUpdate >= interval) {
        _lastUpdate = now;
        switch (_screen) {
            case Screen::HOME:   updateHomeDynamic();              break;
            case Screen::ZONE:   updateZoneDynamic(_selectedZone); break;
            case Screen::STATUS: updateStatusDynamic();            break;
            case Screen::SYSTEM: updateSystemDynamic();            break;
            case Screen::ADMIN:  updateAdminDynamic();             break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  Helpers navigation
// ═══════════════════════════════════════════════════════════════
void DisplayManager::goTo(Screen s) {
    _screen          = s;
    _needsFullRedraw = true;
}

void DisplayManager::adminNext() {
    uint8_t p = (uint8_t)_adminPage;
    p = (p + 1) % (uint8_t)AdminPage::_COUNT;
    _adminPage = (AdminPage)p;
    _needsFullRedraw = true;
}

void DisplayManager::adminPrev() {
    uint8_t p = (uint8_t)_adminPage;
    p = (p == 0) ? (uint8_t)AdminPage::_COUNT - 1 : p - 1;
    _adminPage = (AdminPage)p;
    _needsFullRedraw = true;
}

bool DisplayManager::hitTest(uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh,
                              uint16_t tx, uint16_t ty) {
    return tx >= bx && tx < bx + bw && ty >= by && ty < by + bh;
}

const char* DisplayManager::adminPageName(AdminPage p) {
    switch (p) {
        case AdminPage::WIFI:   return "WiFi";
        case AdminPage::NTP:    return "NTP";
        case AdminPage::OWM:    return "Meteo";
        case AdminPage::ZONES:  return "Zones";
        case AdminPage::SYSTEM: return "Systeme";
        case AdminPage::LOGS:   return "Logs";
        default:                return "?";
    }
}

// ═══════════════════════════════════════════════════════════════
//  Composants UI communs
// ═══════════════════════════════════════════════════════════════
void DisplayManager::drawHeader(const char* title, bool backBtn) {
    _tft.fillRect(0, 0, SCREEN_W, 28, Theme::SURFACE);
    _tft.drawFastHLine(0, 27, SCREEN_W, Theme::BORDER);
    _tft.setFreeFont(nullptr);
    _tft.setTextSize(1);

    if (backBtn) {
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString("<", 8, 10);
    }

    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    _tft.setTextDatum(MC_DATUM);
    _tft.setFreeFont(THEME_FONT_TITLE);
    _tft.drawString(title, SCREEN_W / 2, 14);
    _tft.setFreeFont(nullptr);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 const char* label, uint16_t bg, uint16_t fg) {
    drawCardBg(_tft, x, y, w, h, Theme::R_SM, bg, Theme::BORDER, true);
    _tft.setTextColor(fg, bg);
    _tft.setTextDatum(MC_DATUM);
    _tft.setFreeFont(THEME_FONT_TITLE);
    _tft.setTextSize(1);
    _tft.drawString(label, x + w / 2, y + h / 2);
    _tft.setFreeFont(nullptr);
    _tft.setTextDatum(TL_DATUM);
}

// ─────────────────────────────────────────────
//  Composants visuels réutilisables — redesign session 17/06/2026
//  Objectif : rendu pro sans toucher à la géométrie (x,y,w,h) ni aux
//  zones de touch des fonctions appelantes — seul le rendu interne change.
// ─────────────────────────────────────────────

// Carte avec effet de profondeur : un rectangle décalé en Theme::SHADOW,
// dessiné avant la carte elle-même, simule une ombre portée discrète
// (pas d'ombre native disponible sur TFT_eSPI). elevated=false pour les
// bandeaux pleine largeur déjà posés à plat sur le fond (pas de flottement).
// gfx : _tft pour un dessin direct, ou un sprite (TFT_eSprite hérite de
// TFT_eSPI, donc accepté sans surcharge supplémentaire).
void DisplayManager::drawCardBg(TFT_eSPI& gfx, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 uint8_t radius, uint16_t bg, uint16_t border,
                                 bool elevated) {
    if (elevated) {
        gfx.fillRoundRect(x + 1, y + 2, w, h, radius, Theme::SHADOW);
    }
    gfx.fillRoundRect(x, y, w, h, radius, bg);
    gfx.drawRoundRect(x, y, w, h, radius, border);
}

// Barre verticale d'identité de zone sur le bord gauche — écho du
// border-left coloré des .zone-card en CSS (style.css). L'insertion
// verticale est bornée par le rayon de la carte : en dessous de ce
// seuil, le contour arrondi n'a pas encore rejoint le bord gauche
// plein, et une barre droite y déborderait visuellement de la carte.
void DisplayManager::drawAccentBar(TFT_eSPI& gfx, uint16_t x, uint16_t y, uint16_t h,
                                    uint16_t radius, uint16_t color) {
    uint16_t inset = radius + 1;
    if (h <= 2 * inset) return;  // carte trop petite pour une barre nette
    gfx.fillRect(x, y + inset, Theme::ACCENT_BAR_W, h - 2 * inset, color);
}

// Icône menu (3 barres) dessinée explicitement — plus net que le glyphe
// '=' de la police bitmap, surtout en petite taille (header GRID4 20px).
void DisplayManager::drawMenuIcon(TFT_eSPI& gfx, uint16_t x, uint16_t y, uint16_t color) {
    for (uint8_t i = 0; i < 3; i++) {
        gfx.fillRect(x, y + i * 6, 16, 2, color);
    }
}

// ═══════════════════════════════════════════════════════════════
//  Touch
// ═══════════════════════════════════════════════════════════════
bool DisplayManager::getTouchPoint(uint16_t& tx, uint16_t& ty) {
    if (!_touch.tirqTouched() || !_touch.touched()) return false;
    TS_Point p = _touch.getPoint();

    int16_t xMin = _config ? _config->touch().xMin : TOUCH_X_MIN;
    int16_t xMax = _config ? _config->touch().xMax : TOUCH_X_MAX;
    int16_t yMin = _config ? _config->touch().yMin : TOUCH_Y_MIN;
    int16_t yMax = _config ? _config->touch().yMax : TOUCH_Y_MAX;

    tx = (uint16_t)constrain(map(p.x, xMin, xMax, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
    ty = (uint16_t)constrain(map(p.y, yMin, yMax, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
    return true;
}

void DisplayManager::handleTouch() {
    uint16_t tx, ty;
    if (!getTouchPoint(tx, ty)) return;

    // Debounce : ignorer les taps trop rapprochés (doigt maintenu)
    const uint32_t now = millis();
    if (now - _lastTap < 500) return;
    _lastTap = now;

    // Tout tap réinitialise le timer de veille
    _screenMgr.wakeUp();

    switch (_screen) {
        case Screen::HOME:   handleTouchHome(tx, ty);   break;
        case Screen::ZONE:   handleTouchZone(tx, ty);   break;
        case Screen::STATUS: handleTouchStatus(tx, ty); break;
        case Screen::SYSTEM: handleTouchSystem(tx, ty); break;
        case Screen::ADMIN:  handleTouchAdmin(tx, ty);  break;
    }
}

void DisplayManager::handleTouchHome(uint16_t tx, uint16_t ty) {
    switch (_homeMode) {
        case HomeMode::LIST:  handleTouchHome_list(tx, ty);  break;
        case HomeMode::GRID2: handleTouchHome_grid2(tx, ty); break;
        case HomeMode::GRID4: handleTouchHome_grid4(tx, ty); break;
    }
}

void DisplayManager::handleTouchHome_list(uint16_t tx, uint16_t ty) {
    // [≡] menu → ADMIN
    if (hitTest(0, 0, 28, 28, tx, ty)) { goTo(Screen::ADMIN); return; }

    // Calcul btnY dynamique (cohérent avec drawHomeFull_list)
    uint16_t planH = _planHdrH + min(_nbZones, (uint8_t)4) * _planZoneH;
    uint16_t btnY  = PL_PLAN_Y + planH + _planGap;
    uint16_t btnH  = SCREEN_H - btnY;

    if (_nbZones <= 4) {
        // ── 1-4 zones : planning + boutons sur un seul écran ──

        // Tap planning → écran Zone
        if (ty >= PL_PLAN_Y + _planHdrH && ty < PL_PLAN_Y + planH) {
            uint8_t lineIdx = (ty - PL_PLAN_Y - _planHdrH) / _planZoneH;
            if (lineIdx < _nbZones) { _selectedZone = lineIdx; goTo(Screen::ZONE); }
            return;
        }

        // Tap boutons zones
        if (ty >= btnY) {
            if (_nbZones <= 2) {
                // Toggle direct start/stop — cohérent avec les modes 3-4z, GRID2, GRID4
                for (uint8_t z = 0; z < _nbZones; z++) {
                    uint16_t bx = (z == 0) ? PL_BTN_Z1_X : PL_BTN_Z2_X;
                    if (hitTest(bx, btnY, PL_BTN_W, btnH, tx, ty)) {
                        if (_relais && _relais->getState(z)) {
                            if (_schedule) _schedule->stopManualWatering(z);
                        } else {
                            if (_schedule) _schedule->startManualWatering(z);
                        }
                        EventBus::displayDirty = true;
                        return;
                    }
                }
            } else {
                // Boutons compacts côte à côte
                for (uint8_t z = 0; z < _nbZones; z++) {
                    uint16_t bx = z * (PL_CBTN_W + PL_CBTN_GAP);
                    if (hitTest(bx, btnY, PL_CBTN_W, btnH, tx, ty)) {
                        if (_relais && _relais->getState(z)) {
                            if (_schedule) _schedule->stopManualWatering(z);
                        } else {
                            if (_schedule) _schedule->startManualWatering(z);
                        }
                        EventBus::displayDirty = true; return;
                    }
                }
            }
        }

    } else {
        // ── >4 zones : bouton bascule bas ──
        if (ty >= 220) {
            _listShowForce  = !_listShowForce;
            _listScrollOff  = 0;
            _needsFullRedraw = true;
            return;
        }

        if (!_listShowForce) {
            // Sous-vue PLANNING : tap sur ligne zone → ZONE screen
            if (ty >= PL_PLAN_Y + _planHdrH && ty < PL_PLAN_Y + PL_PLAN_H) {
                uint8_t lineIdx = (ty - PL_PLAN_Y - _planHdrH) / _planZoneH;
                uint8_t zone = _listScrollOff + lineIdx;
                if (zone < _nbZones) { _selectedZone = zone; goTo(Screen::ZONE); }
                return;
            }
            // Tap sur rangée compacte
            const uint16_t ROW_H = 23, ROW_GAP = 1;
            uint8_t maxRows = (220 - PL_BTN_Y) / (ROW_H + ROW_GAP);
            for (uint8_t i = 0; i < maxRows; i++) {
                uint16_t ry = PL_BTN_Y + i * (ROW_H + ROW_GAP);
                if (hitTest(2, ry, SCREEN_W - 4, ROW_H, tx, ty)) {
                    uint8_t zone = _listScrollOff + i;
                    if (zone < _nbZones) { _selectedZone = zone; goTo(Screen::ZONE); }
                    return;
                }
            }
        } else {
            // Sous-vue MARCHE FORCEE : tap sur rangée → toggle arrosage
            const uint16_t ROW_H = 32, ROW_GAP = 2;
            uint8_t maxRows = (220 - 28) / (ROW_H + ROW_GAP);
            for (uint8_t i = 0; i < maxRows; i++) {
                uint16_t ry = 28 + i * (ROW_H + ROW_GAP);
                if (hitTest(2, ry, SCREEN_W - 4, ROW_H, tx, ty)) {
                    uint8_t zone = _listScrollOff + i;
                    if (zone < _nbZones) {
                        if (_relais && _relais->getState(zone)) {
                            if (_schedule) _schedule->stopManualWatering(zone);
                        } else {
                            if (_schedule) _schedule->startManualWatering(zone);
                        }
                        EventBus::displayDirty = true;
                    }
                    return;
                }
            }
        }
    }
}

void DisplayManager::handleTouchZone(uint16_t tx, uint16_t ty) {
    // Bouton arroser/arrêter
    if (hitTest(2, 176, 230, 40, tx, ty)) {
        if (_relais && _relais->getState(_selectedZone)) {
            // Arrêt arrosage manuel : retour HOME automatique (invariant I26)
            if (_schedule) _schedule->stopManualWatering(_selectedZone);
            goTo(Screen::HOME);
        } else {
            // Démarrage arrosage manuel : retour HOME automatique (invariant I26)
            // Le refresh 1s sur HOME affichera le temps restant en temps réel
            if (_schedule) _schedule->startManualWatering(_selectedZone);
            goTo(Screen::HOME);
        }
        return;
    }
    // Retour
    if (hitTest(236, 176, 82, 40, tx, ty)) { goTo(Screen::HOME); return; }
}

void DisplayManager::handleTouchStatus(uint16_t tx, uint16_t ty) {
    if (hitTest(110, 200, 100, 36, tx, ty)) goTo(Screen::HOME);
}

void DisplayManager::handleTouchSystem(uint16_t tx, uint16_t ty) {
    if (hitTest(2, 200, 152, 36, tx, ty))   goTo(Screen::STATUS);
    if (hitTest(162, 200, 156, 36, tx, ty)) goTo(Screen::HOME);
}

void DisplayManager::handleTouchAdmin(uint16_t tx, uint16_t ty) {
    // [←] retour HOME
    if (hitTest(0, 0, 40, 28, tx, ty)) { goTo(Screen::HOME); return; }
    // Navigation bas : [<] | centre | [>]
    if (hitTest(0, ADM_NAV_Y, 60, ADM_NAV_H, tx, ty))   { adminPrev(); return; }
    if (hitTest(260, ADM_NAV_Y, 60, ADM_NAV_H, tx, ty)) { adminNext(); return; }
    // Page WiFi — bouton portail captif
    if (_adminPage == AdminPage::WIFI) {
        if (hitTest(10, 130, 300, 36, tx, ty)) {
            EventBus::captiveRequested = true;
            goTo(Screen::HOME);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  Sprites HOME
// ═══════════════════════════════════════════════════════════════
void DisplayManager::renderTimeSprite() {
    // Sprite 108x20 : heure size2 (gauche) + température size1 (dessous à droite)
    // séparés visuellement pour éviter la collision
    _sprTime.fillSprite(Theme::SURFACE);
    _sprTime.setFreeFont(nullptr);

    // Heure HH:MM en grand
    _sprTime.setTextSize(2);
    _sprTime.setTextColor(Theme::TEXT, Theme::SURFACE);
    String t = (_ntp && _ntp->isSynced()) ? _ntp->getHHMM() : "--:--";
    _sprTime.drawString(t.c_str(), 0, 2);

    // Température en petit — séparée par un espace visuel
    if (_weather && _weather->hasFetched()) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0fC", _weather->getTempC());
        _sprTime.setTextSize(1);
        _sprTime.setTextColor(Theme::AMBER, Theme::SURFACE);
        _sprTime.drawString(buf, 68, 6);  // x=68 après "HH:MM" size2 = ~60px
    }
    _sprTime.pushSprite(182, 6);
}

void DisplayManager::renderSignalSprite() {
    _sprSignal.fillSprite(Theme::SURFACE);
    int8_t rssi = (int8_t)WiFi.RSSI();
    uint8_t bars = (rssi > -55) ? 4 : (rssi > -70) ? 3 : (rssi > -80) ? 2 : 1;
    if (WiFi.status() != WL_CONNECTED) bars = 0;
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t col = (i < bars) ? Theme::GREEN : Theme::BORDER;
        uint8_t  h   = 4 + i * 3;
        _sprSignal.fillRect(i * 5, 16 - h, 4, h, col);
    }
    _sprSignal.pushSprite(296, 6);
}

void DisplayManager::renderPlanSprite() {
    uint8_t  nbPlan    = min(_nbZones, (uint8_t)4);
    uint16_t spriteH   = _planHdrH + nbPlan * _planZoneH;  // hauteur utile réelle

    _sprPlan.fillSprite(Theme::BG);

    // ── Séparateurs horizontaux — uniquement sur les lignes utilisées ──
    _sprPlan.drawFastHLine(0, _planHdrH - 1, 320, Theme::BORDER);
    for (uint8_t z = 0; z < nbPlan; z++) {
        uint16_t rowY = _planHdrH + z * _planZoneH;
        _sprPlan.drawFastHLine(0, rowY + _planZoneH - 1, 320, Theme::BORDER);
    }

    // ── Noms de jours : première colonne = jour courant ──
    // Même convention que l'interface Web, GRID2 et GRID4.
    // Sans synchronisation NTP, repli provisoire sur lundi.
    const char* jours[] = {"Lu","Ma","Me","Je","Ve","Sa","Di"};
    int todayIdx = todayEspIdx();
    int baseIdx  = (todayIdx >= 0) ? todayIdx : 0;
    for (int col = 0; col < 7; col++) {
        int espIdx = (baseIdx + col) % 7;
        int x = PL_LABEL_W + col * PL_DAY_W + PL_DAY_W / 2 - 8;
        uint16_t col_c = (col == 0 && todayIdx >= 0) ? Theme::CYAN : Theme::MUTED;
        _sprPlan.setFreeFont(nullptr);
        _sprPlan.setTextSize(1);
        _sprPlan.setTextColor(col_c, Theme::BG);
        _sprPlan.drawString(jours[espIdx], x, 2);
        // Séparateur vertical (toute la hauteur du sprite)
        _sprPlan.drawFastVLine(PL_LABEL_W + col * PL_DAY_W, 0, PL_PLAN_H, Theme::BORDER);
    }

    // ── Icônes et/ou températures météo J+0..J+4 ──
    // Rendu conditionnel selon showWeatherIcon / showWeatherTemp
    const CfgDisplay& disp = _config ? _config->display() : CfgDisplay{};
    if (disp.showWeatherIcon || disp.showWeatherTemp) {
        for (uint8_t col = 0; col < 5; col++) {
            ForecastDay fd = _weather ? _weather->getForecastDay(col) : ForecastDay{};
            int cx = PL_LABEL_W + col * PL_DAY_W + PL_DAY_W / 2;
            // Séparation verticale stricte :
            // jours y=2, icône y=13, température y=29.
            // On exploite l'espace disponible au-dessus des boutons zones
            // sans empiéter sur les libellés des jours.
            const uint16_t iconY = 13;
            if (disp.showWeatherIcon) {
                drawWeatherIcon(_sprPlan, cx - 7, iconY, fd.rainMm, fd.tempMax,
                                fd.valid, /*showTemp=*/false);
            }
            if (disp.showWeatherTemp && fd.valid && fd.tempMax > -50.0f) {
                char tbuf[8];
                snprintf(tbuf, sizeof(tbuf), "%.0fC", fd.tempMax);
                const uint16_t tempY = disp.showWeatherIcon ? 29 : 15;
                _sprPlan.setTextSize(1);
                _sprPlan.setTextColor(fd.rainMm > 1.0f ? Theme::BLUE : Theme::AMBER, Theme::BG);
                _sprPlan.drawString(tbuf, cx - 7, tempY);
            }
        }
    }

    // ── Repères couleur des zones + barres de slots ──
    for (uint8_t z = 0; z < nbPlan; z++) {
        uint16_t rowY  = _planHdrH + z * _planZoneH;
        uint16_t col_z = Theme::ZONE_COLORS[z % 4];

        // Bulle de couleur dans la colonne planning.
        // L'identité de la zone est portée ici, pas dans le bouton d'action.
        const int16_t bulletX = PL_LABEL_W / 2;
        const int16_t bulletY = rowY + _planZoneH / 2;
        const int16_t bulletR = max(3, min(5, (int)(_planZoneH / 2 - 2)));
        _sprPlan.fillCircle(bulletX, bulletY, bulletR, col_z);

        // Barres de slots
        if (!_schedule) continue;
        ZoneSchedule zs = _schedule->getZoneSchedule(z);

        for (int col = 0; col < 7; col++) {
            int espIdx = (baseIdx + col) % 7;
            int x0 = PL_LABEL_W + col * PL_DAY_W + 1;
            DaySchedule& ds = (zs.mode == 0) ? zs.daySlots[espIdx] : zs.intervalSlots;
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const TimeSlot& sl = ds.slots[s];
                if (!sl.enabled) continue;
                float frac = (float)(sl.hour * 60 + sl.minute) / 1440.0f;
                int sx = x0 + (int)(frac * (PL_DAY_W - 2));
                int sw = max(2, (int)((float)sl.duration / 1440.0f * (PL_DAY_W - 2)));
                _sprPlan.fillRoundRect(sx, rowY + 2, sw, _planZoneH - 4, 1, col_z);
            }
        }
    }

    // Pousser uniquement la hauteur réellement utilisée.
    // Un pushSprite() complet ferait 90 px de haut et recouvrirait le haut
    // des boutons lorsque leur position dynamique commence avant y=118.
    _tft.pushImage(0, PL_PLAN_Y, SCREEN_W, spriteH,
                   static_cast<uint16_t*>(_sprPlan.getPointer()));
}

// ─────────────────────────────────────────────
//  Planning compact — 2 colonnes (aujourd'hui + demain)
//  Utilisé par GRID2 et GRID4 où la hauteur est contrainte
//  sprH  : hauteur disponible (G2_PLAN_H=50 ou G4_PLAN_H=40)
//  destY : y de destination sur le TFT
// ─────────────────────────────────────────────
void DisplayManager::renderPlanSpriteCompact(uint16_t sprH, uint16_t destY, uint16_t planW) {
    _tft.fillRect(0, destY, planW, sprH, Theme::BG);

    // En mode 8 zones, le bandeau planning est très étroit (64 px).
    // On réserve deux lignes compactes : jour puis météo, sans chevauchement.
    const uint16_t HDR_H       = 24;
    const uint16_t LABEL_W_G2  = 12;
    uint16_t zoneAreaH         = sprH - HDR_H;
    uint8_t  nbPlan            = min(_nbZones, (uint8_t)8);
    uint16_t zoneH             = (nbPlan > 0) ? (zoneAreaH / nbPlan) : zoneAreaH;
    if (zoneH < 4) zoneH = 4;

    // 2 colonnes bornées à planW : aujourd'hui + demain.
    // La colonne des bulles est réduite pour gagner de la largeur utile.
    const uint16_t COL_W = (planW - LABEL_W_G2) / 2;
    int todayIdx = todayEspIdx();
    // Si NTP non synced (todayIdx==-1), partir de lundi (0) sans surlignage
    int baseIdx = (todayIdx >= 0) ? todayIdx : 0;
    const char* jours[] = {"Lu","Ma","Me","Je","Ve","Sa","Di"};

    // ── En-têtes colonnes ──
    for (uint8_t c = 0; c < 2; c++) {
        int      espIdx = (baseIdx + c) % 7;
        uint16_t cx     = LABEL_W_G2 + c * COL_W;
        // Surligner aujourd'hui seulement si NTP synced
        uint16_t col_c  = (c == 0 && todayIdx >= 0) ? Theme::CYAN : Theme::MUTED;
        _tft.setFreeFont(nullptr);
        _tft.setTextSize(1);
        _tft.setTextColor(col_c, Theme::BG);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(jours[espIdx], cx + COL_W / 2, destY + 1);

        // Météo compacte sur une seconde ligne : pictogramme à gauche,
        // température à droite. Les deux tiennent dans une colonne de 26 px.
        ForecastDay fd = _weather ? _weather->getForecastDay(c) : ForecastDay{};
        if (fd.valid && fd.tempMax > -50.0f) {
            const uint16_t wx = cx + 3;
            const uint16_t wy = destY + 11;
            if (fd.rainMm > 1.0f) {
                _tft.fillRoundRect(wx, wy + 1, 8, 4, 2, Theme::MUTED);
                _tft.drawFastVLine(wx + 2, wy + 6, 2, Theme::BLUE);
                _tft.drawFastVLine(wx + 6, wy + 6, 2, Theme::BLUE);
            } else {
                _tft.fillCircle(wx + 4, wy + 4, 3, Theme::AMBER);
            }
            char wbuf[6];
            snprintf(wbuf, sizeof(wbuf), "%.0f", fd.tempMax);
            _tft.setTextColor(fd.rainMm > 1.0f ? Theme::BLUE : Theme::AMBER, Theme::BG);
            _tft.setTextDatum(TR_DATUM);
            _tft.drawString(wbuf, cx + COL_W - 2, destY + 11);
        }
        _tft.setTextDatum(TL_DATUM);
        _tft.drawFastVLine(cx, destY, sprH, Theme::BORDER);
    }
    _tft.drawFastHLine(0, destY + HDR_H - 1, planW, Theme::BORDER);

    // ── Lignes zones ──
    for (uint8_t z = 0; z < nbPlan; z++) {
        uint16_t rowY  = destY + HDR_H + z * zoneH;
        uint16_t col_z = Theme::ZONE_COLORS[z % 4];
        _tft.drawFastHLine(0, rowY + zoneH - 1, planW, Theme::BORDER);

        // Bulle de couleur dans la colonne planning.
        const int16_t bulletX = LABEL_W_G2 / 2;
        const int16_t bulletY = rowY + zoneH / 2;
        const int16_t bulletR = max(2, min(4, (int)(zoneH / 2 - 1)));
        _tft.fillCircle(bulletX, bulletY, bulletR, col_z);

        if (!_schedule) continue;
        ZoneSchedule zs = _schedule->getZoneSchedule(z);
        for (uint8_t c = 0; c < 2; c++) {
            int      espIdx = (baseIdx + c) % 7;
            uint16_t x0     = LABEL_W_G2 + c * COL_W + 1;
            DaySchedule& ds = (zs.mode == 0) ? zs.daySlots[espIdx] : zs.intervalSlots;
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const TimeSlot& sl = ds.slots[s];
                if (!sl.enabled) continue;
                float frac = (float)(sl.hour * 60 + sl.minute) / 1440.0f;
                int sx = x0 + (int)(frac * (COL_W - 2));
                int sw = max(2, (int)((float)sl.duration / 1440.0f * (COL_W - 2)));
                uint16_t barH = max((uint16_t)2, (uint16_t)(zoneH - 4));
                _tft.fillRoundRect(sx, rowY + 2, sw, barH, 1, col_z);
            }
        }
    }
}

// ─────────────────────────────────────────────
//  Planning 7 jours plein écran avec météo — GRID4 vues 0 et 1
//  destY : y départ, h : hauteur disponible
//  zStart..zEnd-1 : plage de zones à afficher (0-7 ou 8-15)
// ─────────────────────────────────────────────
void DisplayManager::renderPlanSpriteFull(uint16_t destY, uint16_t h,
                                           uint8_t zStart, uint8_t zEnd) {
    uint8_t  nbZ    = zEnd - zStart;
    uint16_t zoneH  = (nbZ > 0) ? ((h - G4_PLAN_HDR_H) / nbZ) : 0;
    if (zoneH < 4) zoneH = 4;

    _tft.fillRect(0, destY, SCREEN_W, h, Theme::BG);

    int todayIdx = todayEspIdx();
    int baseIdx  = (todayIdx >= 0) ? todayIdx : 0;
    const char* jours[] = {"Lu","Ma","Me","Je","Ve","Sa","Di"};
    const uint16_t DAY_W = (SCREEN_W - PL_LABEL_W) / 7;

    // ── Jours + météo ──
    for (uint8_t col = 0; col < 7; col++) {
        int      espIdx = (baseIdx + col) % 7;
        uint16_t cx     = PL_LABEL_W + col * DAY_W;
        uint16_t col_c  = (col == 0 && todayIdx >= 0) ? Theme::CYAN : Theme::MUTED;
        _tft.setFreeFont(nullptr);
        _tft.setTextSize(1);
        _tft.setTextColor(col_c, Theme::BG);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(jours[espIdx], cx + DAY_W / 2, destY + 2);
        _tft.setTextDatum(TL_DATUM);
        _tft.drawFastVLine(cx, destY, h, Theme::BORDER);

        // Météo J+0..J+4
        if (col < 5) {
            ForecastDay fd = _weather ? _weather->getForecastDay(col) : ForecastDay{};
            if (fd.valid && fd.tempMax > -50.0f) {
                char wbuf[8]; snprintf(wbuf, sizeof(wbuf), "%.0fC", fd.tempMax);
                _tft.setTextSize(1);
                _tft.setTextColor(fd.rainMm > 1.0f ? Theme::BLUE : Theme::AMBER, Theme::BG);
                _tft.setTextDatum(TC_DATUM);
                _tft.drawString(wbuf, cx + DAY_W / 2, destY + 11);
                _tft.setTextDatum(TL_DATUM);
            }
        }
    }
    _tft.drawFastHLine(0, destY + G4_PLAN_HDR_H - 1, SCREEN_W, Theme::BORDER);

    // ── Lignes zones ──
    for (uint8_t zi = 0; zi < nbZ; zi++) {
        uint8_t  z     = zStart + zi;
        uint16_t rowY  = destY + G4_PLAN_HDR_H + zi * zoneH;
        uint16_t col_z = Theme::ZONE_COLORS[z % 4];
        _tft.drawFastHLine(0, rowY + zoneH - 1, SCREEN_W, Theme::BORDER);

        // Bulle de couleur dans la colonne planning.
        const int16_t bulletX = PL_LABEL_W / 2;
        const int16_t bulletY = rowY + zoneH / 2;
        const int16_t bulletR = max(2, min(4, (int)(zoneH / 2 - 1)));
        _tft.fillCircle(bulletX, bulletY, bulletR, col_z);

        if (!_schedule) continue;
        ZoneSchedule zs = _schedule->getZoneSchedule(z);
        for (uint8_t col = 0; col < 7; col++) {
            int      espIdx = (baseIdx + col) % 7;
            uint16_t x0     = PL_LABEL_W + col * DAY_W + 1;
            DaySchedule& ds = (zs.mode == 0) ? zs.daySlots[espIdx] : zs.intervalSlots;
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const TimeSlot& sl = ds.slots[s];
                if (!sl.enabled) continue;
                float frac = (float)(sl.hour * 60 + sl.minute) / 1440.0f;
                int sx = x0 + (int)(frac * (DAY_W - 2));
                int sw = max(2, (int)((float)sl.duration / 1440.0f * (DAY_W - 2)));
                uint16_t barH = max((uint16_t)2, (uint16_t)(zoneH - 4));
                _tft.fillRoundRect(sx, rowY + 2, sw, barH, 1, col_z);
            }
        }
    }
}

void DisplayManager::renderBtnSprite(uint8_t zone, uint16_t pushY) {
    bool    active   = _relais && _relais->getState(zone);
    uint16_t bg      = active ? Theme::ACTIVE_BG : Theme::SURFACE;
    uint16_t border  = active ? Theme::ACTIVE_BORDER : Theme::BORDER;
    uint16_t zColor  = Theme::ZONE_COLORS[zone % 4];

    // Fond sprite = couleur d'écran réelle, pour que les coins hors du
    // rectangle arrondi se fondent dans le fond une fois la sprite poussée
    _sprBtn0.fillSprite(Theme::BG);
    drawCardBg(_sprBtn0, 0, 0, PL_BTN_W, PL_BTN_H, Theme::R_LG, bg, border, false);
    drawAccentBar(_sprBtn0, 0, 0, PL_BTN_H, Theme::R_LG, zColor);

    _sprBtn0.setFreeFont(nullptr);
    _sprBtn0.setTextSize(1);

    // Nom de zone dans le bouton ; aucune bulle ici.
    char fallbackName[16];
    const char* zoneName = zoneButtonName(_config, zone, fallbackName, sizeof(fallbackName));
    _sprBtn0.setTextColor(Theme::TEXT, bg);
    _sprBtn0.setTextDatum(TC_DATUM);
    _sprBtn0.drawString(zoneName, PL_BTN_W / 2, 4);
    _sprBtn0.setTextDatum(TL_DATUM);

    if (active) {
        uint32_t elapsed = _schedule ? _schedule->getElapsedMs(zone) : 0;
        uint32_t remain  = _schedule ? _schedule->getRemainingMs(zone) : 0;
        uint32_t total   = elapsed + remain;

        // Animation goutte : alterne entre 2 frames toutes les 500ms
        bool dropFrame = (millis() / 500) % 2;
        const char* dropIcon = dropFrame ? "~" : "o";

        // Icône eau animée + "EN COURS" en rouge vif
        _sprBtn0.setTextSize(1);
        _sprBtn0.setTextColor(Theme::AMBER, bg);  // AMBER lisible sur fond rouge
        char iconbuf[8];
        snprintf(iconbuf, sizeof(iconbuf), "%s  ON", dropIcon);
        _sprBtn0.setTextDatum(TC_DATUM);
        _sprBtn0.drawString(iconbuf, PL_BTN_W / 2, 17);
        _sprBtn0.setTextDatum(TL_DATUM);

        // Temps restant en GRAND (size2)
        if (remain > 0) {
            _sprBtn0.setTextSize(2);
            _sprBtn0.setTextColor(Theme::TEXT, bg);
            char rbuf[10];
            snprintf(rbuf, sizeof(rbuf), "%02lu:%02lu",
                     remain / 60000UL, (remain % 60000UL) / 1000UL);
            _sprBtn0.setTextDatum(TC_DATUM);
            _sprBtn0.drawString(rbuf, PL_BTN_W / 2, 30);
            _sprBtn0.setTextDatum(TL_DATUM);
        }

        // Barre de progression
        uint8_t pct = (total > 0) ? (uint8_t)((elapsed * 100UL) / total) : 0;
        uint16_t barW = (uint16_t)((PL_BTN_W - 12) * pct / 100);
        _sprBtn0.fillRoundRect(6, 56, PL_BTN_W - 12, 6, 3, Theme::BORDER);
        if (barW > 0)
            _sprBtn0.fillRoundRect(6, 56, barW, 6, 3, Theme::AMBER);  // AMBER visible sur rouge

        // Temps écoulé en petit
        _sprBtn0.setTextSize(1);
        _sprBtn0.setTextColor(Theme::ON_ACTIVE_TEXT, bg);  // jaune pâle lisible sur rouge
        char ebuf[16];
        snprintf(ebuf, sizeof(ebuf), "+%02lu:%02lu",
                 elapsed / 60000UL, (elapsed % 60000UL) / 1000UL);
        _sprBtn0.drawString(ebuf, 6, 68);

        // Hint arrêt
        _sprBtn0.setTextColor(Theme::ON_ACTIVE_MUTED, bg);  // gris clair lisible sur rouge
        _sprBtn0.setTextDatum(TC_DATUM);
        _sprBtn0.drawString("Appuyer pour arreter", PL_BTN_W / 2, 82);
        _sprBtn0.setTextDatum(TL_DATUM);

    } else {
        // Prochain slot — logique différenciée jours fixes vs intervalle
        String next = "--:--";
        if (_schedule && _ntp && _ntp->isSynced()) {
            ZoneSchedule zs  = _schedule->getZoneSchedule(zone);
            int todayEsp     = todayEspIdx();
            uint32_t epochNow = _ntp->getEpochDay();

            if (zs.mode == 0) {
                // Mode jours fixes : parcourir les 7 prochains jours
                for (int d = 0; d < NB_DAYS; d++) {
                    int dayIdx = (todayEsp + d) % NB_DAYS;
                    DaySchedule& ds = zs.daySlots[dayIdx];
                    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                        if (!ds.slots[s].enabled) continue;
                        // Aujourd'hui : vérifier si l'heure n'est pas passée
                        if (d == 0) {
                            int curH = _ntp->getHour();
                            int curM = _ntp->getMinute();
                            int slotMin = ds.slots[s].hour * 60 + ds.slots[s].minute;
                            int nowMin  = curH * 60 + curM;
                            if (slotMin <= nowMin) continue; // déjà passé
                        }
                        // Calculer le nom du jour (Lu, Ma...)
                        const char* JOURS[] = {"lundi","mardi","mercredi","jeudi","vendredi","samedi","dimanche"};
                        char buf2[18];
                        if (d == 0)      snprintf(buf2, sizeof(buf2), "auj. %02d:%02d",   ds.slots[s].hour, ds.slots[s].minute);
                        else if (d == 1) snprintf(buf2, sizeof(buf2), "demain %02d:%02d", ds.slots[s].hour, ds.slots[s].minute);
                        else             snprintf(buf2, sizeof(buf2), "%s %02d:%02d",      JOURS[dayIdx], ds.slots[s].hour, ds.slots[s].minute);
                        next = buf2;
                        d = NB_DAYS; break;
                    }
                }
            } else {
                // Mode intervalle : calculer la prochaine date depuis lastWateredDay
                uint32_t lastDay = zs.lastWateredDay;
                uint32_t nextDay = (lastDay > 0)
                    ? lastDay + zs.intervalDays
                    : epochNow;  // jamais arrosé → aujourd'hui

                // Si nextDay est passé, avancer par intervalles jusqu'au futur
                while (nextDay < epochNow) nextDay += zs.intervalDays;

                // Trouver le premier slot activé
                for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                    if (!zs.intervalSlots.slots[s].enabled) continue;
                    // Convertir nextDay en index jour semaine ESP (0=lun)
                    // epoch day 0 = jeudi 1/1/1970 → jeudi = idx 3
                    uint8_t nextEspIdx = (uint8_t)((nextDay + 3) % 7);
                    const char* JOURS[] = {"lundi","mardi","mercredi","jeudi","vendredi","samedi","dimanche"};
                    int32_t daysAhead = (int32_t)(nextDay - epochNow);
                    char buf2[18];
                    if (daysAhead <= 0)      snprintf(buf2, sizeof(buf2), "auj. %02d:%02d",   zs.intervalSlots.slots[s].hour, zs.intervalSlots.slots[s].minute);
                    else if (daysAhead == 1) snprintf(buf2, sizeof(buf2), "demain %02d:%02d", zs.intervalSlots.slots[s].hour, zs.intervalSlots.slots[s].minute);
                    else                     snprintf(buf2, sizeof(buf2), "%s %02d:%02d",      JOURS[nextEspIdx], zs.intervalSlots.slots[s].hour, zs.intervalSlots.slots[s].minute);
                    next = buf2;
                    break;
                }
            }
        }
        _sprBtn0.setTextColor(Theme::MUTED, bg);
        _sprBtn0.drawString("Prochain :", 6, 20);
        // Size 2 si <= 11 chars (ex "dem 06:30"), sinon size 1 pour les noms longs
        _sprBtn0.setTextSize(next.length() <= 11 ? 2 : 1);
        _sprBtn0.setTextColor(Theme::TEXT, bg);
        _sprBtn0.drawString(next.c_str(), 6, next.length() <= 11 ? 32 : 38);
        _sprBtn0.setTextSize(1);

        // Météo
        if (_weather && _weather->hasFetched()) {
            ForecastDay fd = _weather->getForecastDay(0);
            char wbuf[20];
            if (fd.tempMax > -50.0f)
                snprintf(wbuf, sizeof(wbuf), "%s %.0fmm %.0fC",
                         fd.rainMm > 1.0f ? "~" : "o",
                         fd.rainMm, fd.tempMax);
            else
                snprintf(wbuf, sizeof(wbuf), "%s %.0fmm",
                         fd.rainMm > 1.0f ? "~" : "o", fd.rainMm);
            _sprBtn0.setTextColor(fd.rainMm > 1.0f ? Theme::BLUE : Theme::AMBER, bg);
            _sprBtn0.drawString(wbuf, 6, 58);
        }

        _sprBtn0.setTextColor(Theme::MUTED, bg);
        _sprBtn0.setTextDatum(TC_DATUM);
        _sprBtn0.drawString("Appuyer pour arroser", PL_BTN_W / 2, 82);
        _sprBtn0.setTextDatum(TL_DATUM);
    }

    _sprBtn0.pushSprite((zone == 0) ? PL_BTN_Z1_X : PL_BTN_Z2_X, pushY);
}

// ═══════════════════════════════════════════════════════════════
//  Icône météo vectorielle dans un sprite
//  showTemp=true : affiche la température sous l'icône (y+14)
//  showTemp=false : icône seule — pour le planning compact (PL_HDR_H=28px)
// ═══════════════════════════════════════════════════════════════
void DisplayManager::drawWeatherIcon(TFT_eSprite& spr, uint16_t x, uint16_t y,
                                      float rainMm, float tempC, bool valid,
                                      bool showTemp) {
    if (!valid || tempC <= -50.0f) {
        spr.setTextColor(Theme::MUTED, Theme::BG);
        spr.setTextSize(1);
        spr.drawString("--", x, y);
        return;
    }
    if (rainMm > 1.0f) {
        // Nuage (9px haut) + gouttes (4px) = 13px total
        spr.fillRoundRect(x,     y + 1, 14, 5, 2, Theme::MUTED);
        spr.fillRoundRect(x + 3, y,     10, 5, 2, Theme::MUTED);
        spr.drawFastVLine(x + 3,  y + 7, 3, Theme::BLUE);
        spr.drawFastVLine(x + 7,  y + 9, 3, Theme::BLUE);
        spr.drawFastVLine(x + 11, y + 7, 3, Theme::BLUE);
    } else {
        // Soleil compact (rayon 3 + rayons 2px) = 10px total
        spr.fillCircle(x + 7, y + 6, 3, Theme::AMBER);
        spr.drawFastHLine(x,      y + 6, 3, Theme::AMBER);
        spr.drawFastHLine(x + 12, y + 6, 3, Theme::AMBER);
        spr.drawFastVLine(x + 7,  y,     2, Theme::AMBER);
        spr.drawFastVLine(x + 7,  y + 11, 2, Theme::AMBER);
    }
    if (showTemp) {
        char tbuf[6];
        snprintf(tbuf, sizeof(tbuf), "%.0f", tempC);
        spr.setTextSize(1);
        spr.setTextColor(rainMm > 1.0f ? Theme::BLUE : Theme::AMBER, Theme::BG);
        spr.drawString(tbuf, x, y + 14);
    }
}

// ═══════════════════════════════════════════════════════════════
//  HOME
// ═══════════════════════════════════════════════════════════════
// ─────────────────────────────────────────────
//  Dispatcher HOME → mode courant
// ─────────────────────────────────────────────
void DisplayManager::drawHomeFull() {
    switch (_homeMode) {
        case HomeMode::LIST:  drawHomeFull_list();  break;
        case HomeMode::GRID2: drawHomeFull_grid2(); break;
        case HomeMode::GRID4: drawHomeFull_grid4(); break;
    }
}

// ─────────────────────────────────────────────
//  HOME LIST — 1-4 zones sur un écran, >4 zones avec bascule planning/marche forcée
//
//  1-2 zones : planning + 2 sprites larges côte à côte  (PL_BTN_W×PL_BTN_H)
//  3-4 zones : planning + 4 boutons compacts côte à côte (PL_CBTN_W×PL_CBTN_H)
//  >4 zones  : sous-vue PLAN (planning seul) ou FORCE (drawZoneRow scrollable)
//              bouton bascule en bas y=220..239
// ─────────────────────────────────────────────
void DisplayManager::drawHomeFull_list() {
    // ── Header commun ──
    _tft.fillRect(0, 0, SCREEN_W, 28, Theme::SURFACE);
    _tft.drawFastHLine(0, 27, SCREEN_W, Theme::BORDER);
    _tft.setFreeFont(nullptr);
    drawMenuIcon(_tft, 6, 6, Theme::TEXT);
    _tft.setTextSize(1);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("AquaLook", 28, 4);
    if (_config) {
        const char* city = _config->owm().city;
        if (city && city[0]) {
            char cs[11]; strncpy(cs, city, 10); cs[10] = '\0';
            _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
            _tft.drawString(cs, 28, 16);
        }
    }
    _hc = HomeCache{};
    renderTimeSprite();
    renderSignalSprite();

    if (_nbZones <= 4) {
        // ── Planning + boutons sur un seul écran ──
        renderPlanSprite();

        // Hauteur réelle du planning selon nb zones
        uint16_t planH  = _planHdrH + min(_nbZones, (uint8_t)4) * _planZoneH;
        uint16_t btnY   = PL_PLAN_Y + planH + _planGap;
        uint16_t btnH   = SCREEN_H - btnY;         // tout l'espace restant

        if (_nbZones <= 2) {
            // 1-2 zones : sprites larges côte à côte, pushés à btnY dynamique
            for (uint8_t z = 0; z < _nbZones; z++) renderBtnSprite(z, btnY);
        } else {
            // 3-4 zones : boutons compacts côte à côte
            for (uint8_t z = 0; z < _nbZones; z++) {
                uint16_t bx = z * (PL_CBTN_W + PL_CBTN_GAP);
                drawZoneBtnCompact(z, bx, btnY, PL_CBTN_W, btnH);
            }
        }

    } else {
        // ── >4 zones : deux sous-vues avec bouton bascule bas ──
        if (!_listShowForce) {
            // Sous-vue PLANNING — pleine hauteur y=28..219
            renderPlanSprite();
            // Zone restante y=119..219 : afficher rangées en lecture seule
            const uint16_t ROW_H = 23, ROW_GAP = 1;
            uint8_t maxRows = (220 - PL_BTN_Y) / (ROW_H + ROW_GAP);
            for (uint8_t i = 0; i < maxRows && (_listScrollOff + i) < _nbZones; i++) {
                drawZoneRow(_listScrollOff + i, 2,
                            PL_BTN_Y + i * (ROW_H + ROW_GAP),
                            SCREEN_W - 4, ROW_H);
            }
        } else {
            // Sous-vue MARCHE FORCEE — liste scrollable y=28..219
            const uint16_t ROW_H = 32, ROW_GAP = 2;
            uint8_t maxRows = (220 - 28) / (ROW_H + ROW_GAP);
            for (uint8_t i = 0; i < maxRows && (_listScrollOff + i) < _nbZones; i++) {
                drawZoneRow(_listScrollOff + i, 2,
                            28 + i * (ROW_H + ROW_GAP),
                            SCREEN_W - 4, ROW_H);
            }
        }
        // Bouton bascule bas y=220..239
        _tft.fillRect(0, 220, SCREEN_W, 20, Theme::SURFACE2);
        _tft.drawFastHLine(0, 220, SCREEN_W, Theme::BORDER);
        _tft.setTextSize(1);
        _tft.setTextColor(Theme::CYAN, Theme::SURFACE);
        _tft.setTextDatum(MC_DATUM);
        _tft.drawString(_listShowForce ? "< Planning" : "Marche forcee >",
                        SCREEN_W / 2, 230);
        // Scroll indicator si nécessaire
        if (_listScrollOff > 0 || _listScrollOff + 4 < _nbZones) {
            char nav[12]; snprintf(nav, sizeof(nav), "%d/%d",
                                   _listScrollOff + 1, _nbZones);
            _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
            _tft.setTextDatum(MR_DATUM);
            _tft.drawString(nav, SCREEN_W - 4, 230);
        }
        _tft.setTextDatum(TL_DATUM);
    }
}

void DisplayManager::updateHomeDynamic() {
    switch (_homeMode) {
        case HomeMode::LIST:  updateHomeDynamic_list();  break;
        case HomeMode::GRID2: updateHomeDynamic_grid2(); break;
        case HomeMode::GRID4: updateHomeDynamic_grid4(); break;
    }
}

void DisplayManager::updateHomeDynamic_list() {
    // Heure
    String hhMM = (_ntp && _ntp->isSynced()) ? _ntp->getHHMM() : "--:--";
    if (hhMM != _hc.hhMM) { _hc.hhMM = hhMM; renderTimeSprite(); }

    // Signal
    int8_t rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : 0;
    if (rssi != _hc.rssi) { _hc.rssi = rssi; renderSignalSprite(); }

    // Planning (uniquement si visible)
    if (_nbZones <= 4 || !_listShowForce) {
        float   rainMm   = _weather ? _weather->getRainMm() : 0.0f;
        bool    ntpSync  = _ntp && _ntp->isSynced();
        int8_t  todayNow = (int8_t)todayEspIdx();
        if (rainMm != _hc.rainMm || ntpSync != _hc.ntpSynced || todayNow != _hc.todayIdx) {
            _hc.rainMm    = rainMm;
            _hc.ntpSynced = ntpSync;
            _hc.todayIdx  = todayNow;
            renderPlanSprite();
        }
    }

    const uint16_t planH = _planHdrH + min(_nbZones, (uint8_t)4) * _planZoneH;
    const uint16_t btnY  = PL_PLAN_Y + planH + _planGap;
    const uint16_t btnH  = SCREEN_H - btnY;

    for (uint8_t z = 0; z < _nbZones && z < 16; ++z) {
        const bool active = _relais && _relais->getState(z);
        const uint32_t remainMs = (active && _schedule) ? _schedule->getRemainingMs(z) : 0;
        const uint32_t remainSec = active ? (remainMs / 1000UL) : UINT32_MAX;
        const bool stateChanged = (s_zoneActiveCache[z] != (active ? 1 : 0));

        if (stateChanged) {
            // Changement ON/OFF : redessiner le groupe complet de cartes.
            // Sur le mode 1-2 zones, pousser une seule sprite pendant la transition
            // pouvait laisser une partie de la carte voisine non restaurée.
            if (_nbZones <= 2) {
                for (uint8_t i = 0; i < _nbZones; ++i) {
                    renderBtnSprite(i, btnY);
                    const bool iActive = _relais && _relais->getState(i);
                    const uint32_t iRemainMs = (iActive && _schedule)
                        ? _schedule->getRemainingMs(i) : 0;
                    s_zoneActiveCache[i] = iActive ? 1 : 0;
                    s_zoneRemainSecCache[i] = iActive
                        ? (iRemainMs / 1000UL) : UINT32_MAX;
                }
            } else if (_nbZones <= 4) {
                for (uint8_t i = 0; i < _nbZones; ++i) {
                    const uint16_t bx = i * (PL_CBTN_W + PL_CBTN_GAP);
                    drawZoneBtnCompact(i, bx, btnY, PL_CBTN_W, btnH);
                    const bool iActive = _relais && _relais->getState(i);
                    const uint32_t iRemainMs = (iActive && _schedule)
                        ? _schedule->getRemainingMs(i) : 0;
                    s_zoneActiveCache[i] = iActive ? 1 : 0;
                    s_zoneRemainSecCache[i] = iActive
                        ? (iRemainMs / 1000UL) : UINT32_MAX;
                }
            }
            continue;
        }

        if (!active || remainSec == s_zoneRemainSecCache[z]) continue;

        // Zone active : rafraîchir uniquement le timer/progrès, jamais le fond rouge complet.
        if (_nbZones <= 2) {
            const uint16_t x = (z == 0) ? PL_BTN_Z1_X : PL_BTN_Z2_X;
            const uint16_t bg = Theme::ACTIVE_BG;
            const uint32_t elapsed = _schedule ? _schedule->getElapsedMs(z) : 0;
            const uint32_t total = elapsed + remainMs;
            const uint8_t pct = total ? (uint8_t)((elapsed * 100UL) / total) : 0;
            const uint16_t barW = (uint16_t)((PL_BTN_W - 12) * pct / 100);

            _tft.fillRect(x + 5, btnY + 16, PL_BTN_W - 10, 56, bg);
            _tft.setFreeFont(nullptr);
            _tft.setTextDatum(TC_DATUM);
            _tft.setTextSize(1);
            _tft.setTextColor(Theme::AMBER, bg);
            _tft.drawString(((millis() / 500) % 2) ? "~  ON" : "o  ON", x + PL_BTN_W / 2, btnY + 17);
            _tft.setTextSize(2);
            _tft.setTextColor(Theme::TEXT, bg);
            char rbuf[10];
            snprintf(rbuf, sizeof(rbuf), "%02lu:%02lu", remainMs / 60000UL, (remainMs % 60000UL) / 1000UL);
            _tft.drawString(rbuf, x + PL_BTN_W / 2, btnY + 30);
            _tft.fillRoundRect(x + 6, btnY + 56, PL_BTN_W - 12, 6, 3, Theme::BORDER);
            if (barW) _tft.fillRoundRect(x + 6, btnY + 56, barW, 6, 3, Theme::AMBER);
            _tft.setTextDatum(TL_DATUM);
            _tft.setTextSize(1);
            _tft.setTextColor(Theme::ON_ACTIVE_TEXT, bg);
            char ebuf[16];
            snprintf(ebuf, sizeof(ebuf), "+%02lu:%02lu", elapsed / 60000UL, (elapsed % 60000UL) / 1000UL);
            _tft.drawString(ebuf, x + 6, btnY + 68);
        } else if (_nbZones <= 4) {
            const uint16_t x = z * (PL_CBTN_W + PL_CBTN_GAP);
            const uint16_t bg = Theme::ACTIVE_BG;
            const uint32_t elapsed = _schedule ? _schedule->getElapsedMs(z) : 0;
            const uint32_t total = elapsed + remainMs;
            const uint8_t pct = total ? (uint8_t)((elapsed * 100UL) / total) : 0;
            const uint16_t barW = (uint16_t)((PL_CBTN_W - 8) * pct / 100);

            _tft.fillRect(x + 5, btnY + 20, PL_CBTN_W - 10, 42, bg);
            _tft.setFreeFont(nullptr);
            _tft.setTextDatum(TC_DATUM);
            _tft.setTextSize(2);
            _tft.setTextColor(Theme::TEXT, bg);
            char rbuf[10];
            snprintf(rbuf, sizeof(rbuf), "%02lu:%02lu", remainMs / 60000UL, (remainMs % 60000UL) / 1000UL);
            _tft.drawString(rbuf, x + PL_CBTN_W / 2, btnY + 30);
            _tft.fillRoundRect(x + 4, btnY + 56, PL_CBTN_W - 8, 5, 2, Theme::BORDER);
            if (barW) _tft.fillRoundRect(x + 4, btnY + 56, barW, 5, 2, Theme::AMBER);
            _tft.setTextDatum(TL_DATUM);
            _tft.setTextSize(1);
        }
        s_zoneRemainSecCache[z] = remainSec;
    }
}

float DisplayManager::zonePct(uint8_t z) {
    if (!_schedule) return 0.0f;
    uint32_t rem = _schedule->getRemainingMs(z);
    uint32_t tot = _schedule->getRemainingMs(z) + _schedule->getElapsedMs(z);
    if (tot == 0) return 0.0f;
    return 1.0f - (float)rem / (float)tot;
}

// ─────────────────────────────────────────────────────────────
//  applyDisplayConfig — lit CfgDisplay depuis ConfigManager et
//  met à jour Theme:: (inline) + membres runtime.
//  Appelé depuis begin() et sur EventBus::displayDirty.
//  Si _config est null, les valeurs par défaut restent inchangées.
// ─────────────────────────────────────────────────────────────
void DisplayManager::applyDisplayConfig() {
    if (!_config) return;
    const CfgDisplay& d = _config->display();

    // Couleurs — Theme:: inline vars
    Theme::BG           = hexToRgb565(d.cBg);
    Theme::SURFACE      = hexToRgb565(d.cSurface);
    Theme::SURFACE2     = hexToRgb565(d.cSurface2);
    Theme::BORDER       = hexToRgb565(d.cBorder);
    Theme::TEXT         = hexToRgb565(d.cText);
    Theme::TEXT2        = hexToRgb565(d.cText2);
    Theme::MUTED        = hexToRgb565(d.cMuted);
    Theme::ACTIVE_BG    = hexToRgb565(d.cActiveBg);
    Theme::ZONE_COLORS[0] = hexToRgb565(d.cZone0);
    Theme::ZONE_COLORS[1] = hexToRgb565(d.cZone1);
    Theme::ZONE_COLORS[2] = hexToRgb565(d.cZone2);
    Theme::ZONE_COLORS[3] = hexToRgb565(d.cZone3);

    // Formes — Theme:: inline vars
    Theme::R_SM         = d.rSm;
    Theme::R_MD         = d.rMd;
    Theme::R_LG         = d.rLg;
    Theme::ACCENT_BAR_W = d.accentBarW;

    // Timing — membres runtime (invariant I21)
    _refreshNomMs = d.refreshNomMs;
    _refreshActMs = d.refreshActMs;

    // Layout — membres runtime (touch resync automatique car btnY
    // est calculé dynamiquement depuis _planGap à chaque update())
    _planGap = d.planGap;
    _g2Gpad  = d.g2Gpad;
    _g4Gpad  = d.g4Gpad;

    // Hauteur header planning — séparation nette entre jours, icônes et températures.
    // L'espace supplémentaire est pris sur la grande zone des boutons située dessous.
    // Le cas 4 zones reste strictement contenu dans PL_PLAN_H=90px.
    if (!d.showWeatherIcon && !d.showWeatherTemp) {
        _planHdrH  = 16;  // jours seuls
        _planZoneH = 15;
    } else if (d.showWeatherIcon && !d.showWeatherTemp) {
        _planHdrH  = 30;  // jours + icône
        _planZoneH = 15;  // 30 + 4*15 = 90
    } else if (!d.showWeatherIcon && d.showWeatherTemp) {
        _planHdrH  = 28;  // jours + température
        _planZoneH = 15;  // 28 + 4*15 = 88
    } else {
        // Jours + icône + température.
        // Pour 1 à 3 zones, on profite pleinement de l'espace libre des boutons.
        // Pour 4 zones, on compacte légèrement les lignes afin de rester dans 90px.
        _planHdrH  = (_nbZones <= 3) ? 42 : 38;
        _planZoneH = (_nbZones <= 3) ? 15 : 13;
    }
}

// ═══════════════════════════════════════════════════════════════
//  HOME GRID2 — 5-8 zones (2 colonnes 158×54px)
//  Header 28px | Météo bande 20px | Grille 2 cols | Status 20px
// ═══════════════════════════════════════════════════════════════

// Bouton compact grille : nom + état + prochain en petit
void DisplayManager::drawZoneBtn(uint8_t zone, uint16_t x, uint16_t y,
                                  uint16_t w, uint16_t h) {
    bool     active = _relais && _relais->getState(zone);
    uint16_t bg     = active ? Theme::ACTIVE_BG : Theme::SURFACE;
    uint16_t border = active ? Theme::ACTIVE_BORDER : Theme::BORDER;
    uint16_t zColor = Theme::ZONE_COLORS[zone % 4];

    drawCardBg(_tft, x, y, w, h, Theme::R_MD, bg, border, false);
    drawAccentBar(_tft, x, y, h, Theme::R_MD, zColor);
    _tft.setFreeFont(nullptr);
    _tft.setTextSize(1);

    // Nom de zone dans le bouton ; aucune bulle ici.
    char fallbackName[16];
    const char* zoneName = zoneButtonName(_config, zone, fallbackName, sizeof(fallbackName));
    _tft.setTextColor(Theme::TEXT, bg);
    _tft.setTextDatum(TC_DATUM);
    _tft.drawString(zoneName, x + w / 2, y + 3);

    // État ou prochain
    _tft.setTextColor(active ? Theme::TEXT : Theme::MUTED, bg);
    if (active && _schedule) {
        uint32_t rem = _schedule->getRemainingMs(zone);
        char buf[12];
        snprintf(buf, sizeof(buf), "%02lu:%02lu",
                 rem / 60000UL, (rem % 60000UL) / 1000UL);
        _tft.setTextSize(2);
        _tft.drawString(buf, x + w/2, y + 16);
        _tft.setTextSize(1);
    } else {
        // Prochain slot simplifié
        _tft.drawString("Appuyer", x + w/2, y + h - 12);
    }
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawHomeFull_grid2() {
    _tft.fillScreen(Theme::BG);

    // ── Header compact 22px ──
    _tft.fillRect(0, 0, SCREEN_W, G2_HDR_H, Theme::SURFACE);
    _tft.setFreeFont(nullptr);
    drawMenuIcon(_tft, 6, 7, Theme::TEXT);
    _tft.setTextSize(1);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("AquaLook", 24, 8);
    renderTimeSprite();
    renderSignalSprite();

    // ── Séparateur vertical planning | grille ──
    _tft.drawFastVLine(G2_GRID_X - 1, G2_CONTENT_Y, G2_CONTENT_H, Theme::BORDER);

    // ── Colonne gauche : planning aujourd'hui (toutes les zones) ──
    renderPlanSpriteCompact(G2_CONTENT_H, G2_CONTENT_Y, G2_PLAN_W);

    // ── Colonne droite : grille marche forcée ──
    uint8_t maxZ = min(_nbZones, (uint8_t)8);
    for (uint8_t z = 0; z < maxZ; z++) {
        uint8_t  col = z % 2;
        uint8_t  row = z / 2;
        uint16_t bx  = G2_GRID_X + col * (G2_GW + _g2Gpad);
        uint16_t by  = G2_CONTENT_Y + row * (G2_GH + _g2Gpad);
        drawZoneBtn(z, bx, by, G2_GW, G2_GH);
    }
}

void DisplayManager::updateHomeDynamic_grid2() {
    renderTimeSprite();
    renderSignalSprite();
    float   rainMm   = _weather ? _weather->getRainMm() : 0.0f;
    bool    ntpSync  = _ntp && _ntp->isSynced();
    int8_t  todayNow = (int8_t)todayEspIdx();
    if (rainMm != _hc.rainMm || ntpSync != _hc.ntpSynced || todayNow != _hc.todayIdx) {
        _hc.rainMm    = rainMm;
        _hc.ntpSynced = ntpSync;
        _hc.todayIdx  = todayNow;
        renderPlanSpriteCompact(G2_CONTENT_H, G2_CONTENT_Y, G2_PLAN_W);
    }

    const uint8_t maxZ = min(_nbZones, (uint8_t)8);
    for (uint8_t z = 0; z < maxZ; ++z) {
        const uint8_t col = z % 2;
        const uint8_t row = z / 2;
        const uint16_t x = G2_GRID_X + col * (G2_GW + _g2Gpad);
        const uint16_t y = G2_CONTENT_Y + row * (G2_GH + _g2Gpad);
        const bool active = _relais && _relais->getState(z);
        const uint32_t remainMs = (active && _schedule) ? _schedule->getRemainingMs(z) : 0;
        const uint32_t remainSec = active ? (remainMs / 1000UL) : UINT32_MAX;

        if (s_zoneActiveCache[z] != (active ? 1 : 0)) {
            drawZoneBtn(z, x, y, G2_GW, G2_GH);
            s_zoneActiveCache[z] = active ? 1 : 0;
            s_zoneRemainSecCache[z] = remainSec;
            continue;
        }
        if (!active || remainSec == s_zoneRemainSecCache[z]) continue;

        // Mise à jour locale du timer : pas de ré-écriture du fond rouge ni des bordures.
        _tft.fillRect(x + 10, y + 15, G2_GW - 20, 22, Theme::ACTIVE_BG);
        _tft.setFreeFont(nullptr);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextSize(2);
        _tft.setTextColor(Theme::TEXT, Theme::ACTIVE_BG);
        char buf[12];
        snprintf(buf, sizeof(buf), "%02lu:%02lu", remainMs / 60000UL, (remainMs % 60000UL) / 1000UL);
        _tft.drawString(buf, x + G2_GW / 2, y + 16);
        _tft.setTextSize(1);
        _tft.setTextDatum(TL_DATUM);
        s_zoneRemainSecCache[z] = remainSec;
    }
}

void DisplayManager::handleTouchHome_grid2(uint16_t tx, uint16_t ty) {
    // [≡] menu
    if (hitTest(0, 0, 40, G2_HDR_H, tx, ty)) { goTo(Screen::ADMIN); return; }

    // Colonne droite uniquement : grille → toggle arrosage direct
    if (tx >= G2_GRID_X) {
        uint8_t maxZ = min(_nbZones, (uint8_t)8);
        for (uint8_t z = 0; z < maxZ; z++) {
            uint8_t  col = z % 2;
            uint8_t  row = z / 2;
            uint16_t bx  = G2_GRID_X + col * (G2_GW + _g2Gpad);
            uint16_t by  = G2_CONTENT_Y + row * (G2_GH + _g2Gpad);
            if (hitTest(bx, by, G2_GW, G2_GH, tx, ty)) {
                if (_relais && _relais->getState(z)) {
                    if (_schedule) _schedule->stopManualWatering(z);
                } else {
                    if (_schedule) _schedule->startManualWatering(z);
                }
                EventBus::displayDirty = true;
                return;
            }
        }
    }
    // Colonne gauche (planning) : tap sans action
}

// ═══════════════════════════════════════════════════════════════
//  HOME GRID4 — 9-16 zones (grille 4 colonnes 78×46px)
//  Header 20px | Grille 4×N | Status bar 20px
// ═══════════════════════════════════════════════════════════════

void DisplayManager::drawHomeFull_grid4() {
    _tft.fillScreen(Theme::BG);

    // ── Header ultra-compact 20px ──
    _tft.fillRect(0, 0, SCREEN_W, G4_HDR_H, Theme::SURFACE);
    _tft.setFreeFont(nullptr);
    drawMenuIcon(_tft, 4, 3, Theme::TEXT);
    _tft.setTextSize(1);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    _tft.setTextDatum(ML_DATUM);
    _tft.drawString("AquaLook", 24, 10);
    renderTimeSprite();
    renderSignalSprite();
    _tft.setTextDatum(TL_DATUM);

    // ── Contenu selon _grid4View ──
    switch (_grid4View) {
        case 0:
            // Vue 0 : Planning Z1-8
            renderPlanSpriteFull(G4_CONTENT_Y, G4_CONTENT_H, 0, min(_nbZones, (uint8_t)8));
            break;
        case 1:
            // Vue 1 : Planning Z9-16
            renderPlanSpriteFull(G4_CONTENT_Y, G4_CONTENT_H, 8, min(_nbZones, (uint8_t)16));
            break;
        case 2:
            // Vue 2 : Grille marche forcée
            {
                uint8_t maxZ = min(_nbZones, (uint8_t)16);
                for (uint8_t z = 0; z < maxZ; z++) {
                    uint8_t  col = z % 4;
                    uint8_t  row = z / 4;
                    drawZoneBtn(z, 1 + col * (G4_GW + _g4Gpad),
                                G4_CONTENT_Y + row * (G4_GH + _g4Gpad), G4_GW, G4_GH);
                }
            }
            break;
    }

    // ── Bouton bascule bas ──
    _tft.fillRect(0, G4_TAB_Y, SCREEN_W, G4_TAB_H, Theme::SURFACE2);
    _tft.drawFastHLine(0, G4_TAB_Y, SCREEN_W, Theme::BORDER);
    _tft.setTextSize(1);
    _tft.setTextColor(Theme::CYAN, Theme::SURFACE);
    _tft.setTextDatum(MC_DATUM);
    const char* tabLabels[] = { "Plan Z1-8", "Plan Z9-16", "Marche forcee" };
    // Afficher vue précédente et suivante pour indiquer la navigation
    char tabBuf[32];
    snprintf(tabBuf, sizeof(tabBuf), "< %s | %s >",
             tabLabels[_grid4View],
             tabLabels[(_grid4View + 1) % 3]);
    _tft.drawString(tabBuf, SCREEN_W / 2, G4_TAB_Y + G4_TAB_H / 2);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::updateHomeDynamic_grid4() {
    renderTimeSprite();
    renderSignalSprite();

    float   rainMm   = _weather ? _weather->getRainMm() : 0.0f;
    bool    ntpSync  = _ntp && _ntp->isSynced();
    int8_t  todayNow = (int8_t)todayEspIdx();
    bool    planDirty = (rainMm != _hc.rainMm || ntpSync != _hc.ntpSynced || todayNow != _hc.todayIdx);

    if (planDirty) {
        _hc.rainMm    = rainMm;
        _hc.ntpSynced = ntpSync;
        _hc.todayIdx  = todayNow;
    }

    switch (_grid4View) {
        case 0:
            if (planDirty) renderPlanSpriteFull(G4_CONTENT_Y, G4_CONTENT_H, 0, min(_nbZones, (uint8_t)8));
            break;
        case 1:
            if (planDirty) renderPlanSpriteFull(G4_CONTENT_Y, G4_CONTENT_H, 8, min(_nbZones, (uint8_t)16));
            break;
        case 2: {
            const uint8_t maxZ = min(_nbZones, (uint8_t)16);
            for (uint8_t z = 0; z < maxZ; ++z) {
                const uint8_t col = z % 4;
                const uint8_t row = z / 4;
                const uint16_t x = 1 + col * (G4_GW + _g4Gpad);
                const uint16_t y = G4_CONTENT_Y + row * (G4_GH + _g4Gpad);
                const bool active = _relais && _relais->getState(z);
                const uint32_t remainMs = (active && _schedule) ? _schedule->getRemainingMs(z) : 0;
                const uint32_t remainSec = active ? (remainMs / 1000UL) : UINT32_MAX;

                if (s_zoneActiveCache[z] != (active ? 1 : 0)) {
                    drawZoneBtn(z, x, y, G4_GW, G4_GH);
                    s_zoneActiveCache[z] = active ? 1 : 0;
                    s_zoneRemainSecCache[z] = remainSec;
                    continue;
                }
                if (!active || remainSec == s_zoneRemainSecCache[z]) continue;

                _tft.fillRect(x + 8, y + 15, G4_GW - 16, 22, Theme::ACTIVE_BG);
                _tft.setFreeFont(nullptr);
                _tft.setTextDatum(MC_DATUM);
                _tft.setTextSize(2);
                _tft.setTextColor(Theme::TEXT, Theme::ACTIVE_BG);
                char buf[12];
                snprintf(buf, sizeof(buf), "%02lu:%02lu", remainMs / 60000UL, (remainMs % 60000UL) / 1000UL);
                _tft.drawString(buf, x + G4_GW / 2, y + 16);
                _tft.setTextSize(1);
                _tft.setTextDatum(TL_DATUM);
                s_zoneRemainSecCache[z] = remainSec;
            }
            break;
        }
    }
}

void DisplayManager::handleTouchHome_grid4(uint16_t tx, uint16_t ty) {
    // [≡] menu
    if (hitTest(0, 0, 40, G4_HDR_H, tx, ty)) { goTo(Screen::ADMIN); return; }

    // Bouton bascule bas — cycle entre les 3 vues
    if (ty >= G4_TAB_Y) {
        _grid4View = (_grid4View + 1) % 3;
        _needsFullRedraw = true;
        return;
    }

    // Vue marche forcée uniquement : tap bouton → toggle arrosage
    if (_grid4View == 2) {
        uint8_t maxZ = min(_nbZones, (uint8_t)16);
        for (uint8_t z = 0; z < maxZ; z++) {
            uint8_t  col = z % 4;
            uint8_t  row = z / 4;
            uint16_t bx  = 1 + col * (G4_GW + _g4Gpad);
            uint16_t by  = G4_CONTENT_Y + row * (G4_GH + _g4Gpad);
            if (hitTest(bx, by, G4_GW, G4_GH, tx, ty)) {
                if (_relais && _relais->getState(z)) {
                    if (_schedule) _schedule->stopManualWatering(z);
                } else {
                    if (_schedule) _schedule->startManualWatering(z);
                }
                EventBus::displayDirty = true;
                return;
            }
        }
    }
    // Vues planning : tap sans action
}

// ── Rangée zone compact (mode LIST scroll) ────
void DisplayManager::drawZoneRow(uint8_t zone, uint16_t x, uint16_t y,
                                  uint16_t w, uint16_t h) {
    bool     active = _relais && _relais->getState(zone);
    uint16_t bg     = active ? Theme::ACTIVE_BG : Theme::SURFACE;
    uint16_t border = active ? Theme::ACTIVE_BORDER : Theme::BORDER;
    uint16_t zColor = Theme::ZONE_COLORS[zone % 4];

    drawCardBg(_tft, x, y, w, h, Theme::R_SM, bg, border, true);
    drawAccentBar(_tft, x, y, h, Theme::R_SM, zColor);
    _tft.setFreeFont(nullptr);
    _tft.setTextSize(1);

    // Nom de zone dans la rangée ; aucune bulle ici.
    char fallbackName[16];
    const char* zoneName = zoneButtonName(_config, zone, fallbackName, sizeof(fallbackName));
    _tft.setTextColor(Theme::TEXT, bg);
    _tft.drawString(zoneName, x + 14, y + 3);

    // État / Prochain
    _tft.setTextColor(Theme::MUTED, bg);
    if (active && _schedule) {
        uint32_t rem = _schedule->getRemainingMs(zone);
        char buf[16];
        snprintf(buf, sizeof(buf), "ON %02lu:%02lu",
                 rem / 60000UL, (rem % 60000UL) / 1000UL);
        _tft.setTextColor(Theme::AMBER, bg);
        _tft.drawString(buf, x + 14, y + 14);
    } else {
        _tft.drawString("Appuyer pour arroser", x + 14, y + 14);
    }

    // Bouton GO/STOP à droite — couleur vive assumée (action, pas une carte)
    uint16_t btnX = x + w - 36;
    uint16_t btnY = y + h/2 - 10;
    _tft.fillRoundRect(btnX, btnY, 32, 20, Theme::R_SM,
                       active ? Theme::RED : Theme::GREEN);
    _tft.setTextColor(active ? Theme::TEXT : 0x0000, active ? Theme::RED : Theme::GREEN);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(active ? "STOP" : "GO", btnX + 16, btnY + 10);
    _tft.setTextDatum(TL_DATUM);
}


// ── Bouton zone compact (3-4 zones, côte à côte) ──────────────
// Inspiré de renderBtnSprite mais dessiné directement sur TFT
// w=PL_CBTN_W=78, h=PL_CBTN_H=120
void DisplayManager::drawZoneBtnCompact(uint8_t zone, uint16_t x, uint16_t y,
                                         uint16_t w, uint16_t h) {
    bool     active = _relais && _relais->getState(zone);
    uint16_t bg     = active ? Theme::ACTIVE_BG : Theme::SURFACE;
    uint16_t border = active ? Theme::ACTIVE_BORDER : Theme::BORDER;
    uint16_t zColor = Theme::ZONE_COLORS[zone % 4];

    drawCardBg(_tft, x, y, w, h, Theme::R_MD, bg, border, true);
    drawAccentBar(_tft, x, y, h, Theme::R_MD, zColor);
    _tft.setFreeFont(nullptr);
    _tft.setTextSize(1);

    // Nom de zone dans le bouton ; aucune bulle ici.
    char fallbackName[16];
    const char* zoneName = zoneButtonName(_config, zone, fallbackName, sizeof(fallbackName));
    _tft.setTextColor(Theme::TEXT, bg);
    _tft.setTextDatum(TC_DATUM);
    _tft.drawString(zoneName, x + w / 2, y + 4);

    if (active && _schedule) {
        // Temps restant en grand
        uint32_t rem  = _schedule->getRemainingMs(zone);
        uint32_t el   = _schedule->getElapsedMs(zone);
        uint32_t tot  = rem + el;
        char rbuf[10];
        snprintf(rbuf, sizeof(rbuf), "%02lu:%02lu",
                 rem / 60000UL, (rem % 60000UL) / 1000UL);
        _tft.setTextSize(2);
        _tft.setTextColor(Theme::TEXT, bg);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(rbuf, x + w / 2, y + 30);
        _tft.setTextSize(1);
        // Barre de progression
        uint8_t pct  = (tot > 0) ? (uint8_t)((el * 100UL) / tot) : 0;
        uint16_t bw  = (uint16_t)((w - 8) * pct / 100);
        _tft.fillRoundRect(x + 4, y + 56, w - 8, 5, 2, Theme::BORDER);
        if (bw > 0) _tft.fillRoundRect(x + 4, y + 56, bw, 5, 2, Theme::AMBER);
        // Hint arrêt
        _tft.setTextColor(Theme::MUTED, bg);
        _tft.drawString("Appuyer", x + w / 2, y + h - 14);
        _tft.drawString("pour arreter", x + w / 2, y + h - 5);
    } else {
        // Prochain slot simplifié
        _tft.setTextColor(Theme::MUTED, bg);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString("Appuyer", x + w / 2, y + 45);
        _tft.drawString("pour arroser", x + w / 2, y + 56);

        // Météo du jour en texte compact
        if (_weather && _weather->hasFetched()) {
            ForecastDay fd = _weather->getForecastDay(0);
            char wbuf[16];
            if (fd.tempMax > -50.0f)
                snprintf(wbuf, sizeof(wbuf), "%.0fmm %.0fC",
                         fd.rainMm, fd.tempMax);
            else
                snprintf(wbuf, sizeof(wbuf), "%.0fmm", fd.rainMm);
            _tft.setTextColor(fd.rainMm > 1.0f ? Theme::BLUE : Theme::AMBER, bg);
            _tft.drawString(wbuf, x + w / 2, y + 75);
        }
        _tft.setTextDatum(TL_DATUM);
    }
    _tft.setTextDatum(TL_DATUM);
}
void DisplayManager::drawZoneFull(uint8_t zone) {
    const char* name = (_config && _config->zone(zone).name[0])
                       ? _config->zone(zone).name
                       : (zone == 0 ? "Zone 1" : "Zone 2");
    drawHeader(name, true);

    bool active = _relais && _relais->getState(zone);
    _tft.setTextColor(active ? Theme::GREEN : Theme::RED, Theme::BG);
    _tft.setFreeFont(THEME_FONT_HEADLINE);
    _tft.setTextSize(1);
    _tft.drawString(active ? "ARROSAGE EN COURS" : "ARRET", 10, 40);
    _tft.setFreeFont(nullptr);

    if (active && _schedule) {
        uint32_t el  = _schedule->getElapsedMs(zone);
        uint32_t rem = _schedule->getRemainingMs(zone);
        char buf[32];
        _tft.setTextSize(1);
        _tft.setTextColor(Theme::TEXT2, Theme::BG);
        snprintf(buf, sizeof(buf), "Ecoulé : %lum%02lus",
                 el / 60000UL, (el % 60000UL) / 1000UL);
        _tft.drawString(buf, 10, 72);
        snprintf(buf, sizeof(buf), "Reste  : %lum%02lus",
                 rem / 60000UL, (rem % 60000UL) / 1000UL);
        _tft.drawString(buf, 10, 88);
    }

    _tft.setTextSize(1);
    _tft.setTextColor(Theme::MUTED, Theme::BG);
    String reason = (_schedule) ? _schedule->getLastReason(zone) : "";
    if (reason.length()) _tft.drawString(reason.c_str(), 10, 110);

    char btnLabel[24];
    if (active) {
        snprintf(btnLabel, sizeof(btnLabel), "Arreter");
    } else {
        uint16_t dur = _schedule ? _schedule->getManualDurationMin() : 10;
        snprintf(btnLabel, sizeof(btnLabel), "Arroser %dmin", dur);
    }
    drawButton(2, 176, 230, 40, btnLabel, active ? Theme::RED : Theme::GREEN, Theme::TEXT);
    drawButton(236, 176, 82, 40, "Retour", Theme::SURFACE, Theme::TEXT);
}

void DisplayManager::updateZoneDynamic(uint8_t zone) {
    drawZoneFull(zone);  // simple redraw complet — zone est un petit écran
}

// ═══════════════════════════════════════════════════════════════
//  STATUS
// ═══════════════════════════════════════════════════════════════
void DisplayManager::drawStatusFull() {
    drawHeader("Etat");
    drawCardBg(_tft, 6, 32, SCREEN_W - 12, 164, Theme::R_MD, Theme::SURFACE, Theme::BORDER, false);
    _tft.setTextSize(1);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);

    int y = 36;
    auto row = [&](const char* label, const String& val) {
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString(label, 10, y);
        _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
        _tft.drawString(val.c_str(), 120, y);
        y += 18;
    };

    row("WiFi :", WiFi.localIP().toString());
    row("NTP  :", (_ntp && _ntp->isSynced()) ? _ntp->getTimeStr() : "Non sync");
    row("Météo:", _weather ? _weather->getStatusStr() : "--");
    row("Heap :", String(ESP.getFreeHeap()) + " o");
    row("Uptime:", String(millis() / 1000UL) + " s");
    for (uint8_t z = 0; z < NB_ZONES; z++) {
        const char* zn = (_config && _config->zone(z).name[0])
                         ? _config->zone(z).name : (z==0?"Z1":"Z2");
        row(zn, (_relais && _relais->getState(z)) ? "ACTIF" : "Arret");
    }

    drawButton(110, 200, 100, 36, "Retour", Theme::SURFACE, Theme::TEXT);
}

void DisplayManager::updateStatusDynamic() { drawStatusFull(); }

// ═══════════════════════════════════════════════════════════════
//  SYSTEM
// ═══════════════════════════════════════════════════════════════
void DisplayManager::drawSystemFull() {
    drawHeader("Systeme");
    drawCardBg(_tft, 6, 32, SCREEN_W - 12, 164, Theme::R_MD, Theme::SURFACE, Theme::BORDER, false);
    _tft.setTextSize(1);
    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("Interface web :", 10, 36);
    _tft.setTextColor(Theme::CYAN, Theme::SURFACE);
    String ip = WiFi.localIP().toString();
    _tft.drawString(("http://" + ip).c_str(), 10, 52);

    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("RAM libre :", 10, 76);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    _tft.drawString((String(ESP.getFreeHeap()) + " octets").c_str(), 110, 76);

    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("Uptime :", 10, 92);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    uint32_t up = millis() / 1000UL;
    char ubuf[20];
    snprintf(ubuf, sizeof(ubuf), "%luh%02lum", up / 3600UL, (up % 3600UL) / 60UL);
    _tft.drawString(ubuf, 110, 92);

    drawButton(2,   200, 152, 36, "Etat",   Theme::SURFACE, Theme::TEXT);
    drawButton(162, 200, 156, 36, "Accueil", Theme::SURFACE, Theme::TEXT);
}

void DisplayManager::updateSystemDynamic() { drawSystemFull(); }

// ═══════════════════════════════════════════════════════════════
//  ADMIN — structure commune
// ═══════════════════════════════════════════════════════════════
void DisplayManager::drawAdminFull() {
    drawAdminHeader();
    drawAdminPageContent();
    drawAdminNav();
}

void DisplayManager::drawAdminHeader() {
    _tft.fillRect(0, 0, SCREEN_W, 28, Theme::SURFACE);
    _tft.drawFastHLine(0, 27, SCREEN_W, Theme::BORDER);
    // [←] retour
    _tft.setTextColor(Theme::CYAN, Theme::SURFACE);
    _tft.setTextSize(1);
    _tft.drawString("<- Admin", 4, 10);
    // Numéro page
    char pbuf[8];
    snprintf(pbuf, sizeof(pbuf), "%d/%d",
             (uint8_t)_adminPage + 1, (uint8_t)AdminPage::_COUNT);
    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.setTextDatum(TR_DATUM);
    _tft.drawString(pbuf, SCREEN_W - 4, 10);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawAdminNav() {
    _tft.fillRect(0, ADM_NAV_Y, SCREEN_W, ADM_NAV_H, Theme::SURFACE2);
    _tft.drawFastHLine(0, ADM_NAV_Y, SCREEN_W, Theme::BORDER);
    drawButton(0,   ADM_NAV_Y, 60, ADM_NAV_H, "<",  Theme::SURFACE, Theme::TEXT);
    drawButton(260, ADM_NAV_Y, 60, ADM_NAV_H, ">",  Theme::SURFACE, Theme::TEXT);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    _tft.setTextDatum(MC_DATUM);
    _tft.setFreeFont(THEME_FONT_TITLE);
    _tft.drawString(adminPageName(_adminPage), SCREEN_W / 2, ADM_NAV_Y + ADM_NAV_H / 2);
    _tft.setFreeFont(nullptr);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawAdminPageContent() {
    _tft.fillRect(0, ADM_CONTENT_Y, SCREEN_W, ADM_CONTENT_H, Theme::BG);
    drawCardBg(_tft, 6, ADM_CONTENT_Y + 4, SCREEN_W - 12, ADM_CONTENT_H - 8,
              Theme::R_MD, Theme::SURFACE, Theme::BORDER, false);
    switch (_adminPage) {
        case AdminPage::WIFI:   drawAdminPageWifi();   break;
        case AdminPage::NTP:    drawAdminPageNtp();    break;
        case AdminPage::OWM:    drawAdminPageOwm();    break;
        case AdminPage::ZONES:  drawAdminPageZones();  break;
        case AdminPage::SYSTEM: drawAdminPageSystem(); break;
        case AdminPage::LOGS:   drawAdminPageLogs();   break;
        default: break;
    }
}

void DisplayManager::updateAdminDynamic() {
    // Seule la page SYSTEM a du contenu dynamique (RAM, uptime)
    if (_adminPage == AdminPage::SYSTEM ||
        _adminPage == AdminPage::LOGS) drawAdminPageContent();
}

// ─────────────────────────────────────────────
//  Pages ADMIN
// ─────────────────────────────────────────────

void DisplayManager::drawAdminPageWifi() {
    int y = ADM_CONTENT_Y + 8;
    _tft.setTextSize(1);

    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("SSID actuel :", 10, y); y += 16;
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    const char* ssid = (_config && _config->wifi().ssid[0])
                       ? _config->wifi().ssid : "(non configure)";
    _tft.drawString(ssid, 10, y); y += 24;

    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    String stateStr = "Etat WiFi : ";
    stateStr += (WiFi.status() == WL_CONNECTED)
                ? WiFi.localIP().toString()
                : "Deconnecte";
    _tft.drawString(stateStr.c_str(), 10, y); y += 28;

    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("Config via navigateur :", 10, y); y += 14;
    _tft.setTextColor(Theme::CYAN, Theme::SURFACE);
    { String ip = WiFi.localIP().toString();
      _tft.drawString(("http://" + (ip.length() > 3 ? ip : "...")).c_str(), 10, y);
      y += 24; }

    // Bouton portail captif
    drawButton(10, 130, 300, 36, "Lancer portail captif", Theme::AMBER, 0x0000);
}

void DisplayManager::drawAdminPageNtp() {
    int y = ADM_CONTENT_Y + 8;
    _tft.setTextSize(1);

    auto row = [&](const char* label, const String& val) {
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString(label, 10, y);
        _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
        _tft.drawString(val.c_str(), 130, y);
        y += 20;
    };

    if (_config) {
        const CfgNtp& n = _config->ntp();
        row("Serveur :",   String(n.server));
        row("GMT offset :", String(n.gmtOffset / 3600) + "h");
        row("DST offset :", String(n.dstOffset / 3600) + "h");
    }
    row("Sync :", (_ntp && _ntp->isSynced()) ? _ntp->getTimeStr() : "Non sync");

    y += 8;
    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("Config via navigateur :", 10, y); y += 14;
    _tft.setTextColor(Theme::CYAN, Theme::SURFACE);
    { String ip = WiFi.localIP().toString();
      _tft.drawString(("http://" + (ip.length() > 3 ? ip : "...")).c_str(), 10, y); }
}

void DisplayManager::drawAdminPageOwm() {
    int y = ADM_CONTENT_Y + 8;
    _tft.setTextSize(1);

    auto row = [&](const char* label, const String& val) {
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString(label, 10, y);
        _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
        _tft.drawString(val.c_str(), 120, y);
        y += 20;
    };

    if (_config) {
        const CfgOwm& o = _config->owm();
        bool hasKey = (o.apiKey[0] != '\0');
        row("Cle API :",  hasKey ? "****configuree" : "(vide)");
        row("Latitude :", String(o.lat, 3));
        row("Longitude:", String(o.lon, 3));
        row("Unites :",   String(o.units));
    }
    row("Météo :", _weather && _weather->hasFetched()
                  ? _weather->getStatusStr() : "En attente...");

    y += 8;
    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("Config via navigateur :", 10, y); y += 14;
    _tft.setTextColor(Theme::CYAN, Theme::SURFACE);
    { String ip = WiFi.localIP().toString();
      _tft.drawString(("http://" + (ip.length() > 3 ? ip : "...")).c_str(), 10, y); }
}

void DisplayManager::drawAdminPageZones() {
    int y = ADM_CONTENT_Y + 8;
    _tft.setTextSize(1);

    for (uint8_t z = 0; z < NB_ZONES; z++) {
        _tft.setTextColor(z == 0 ? Theme::GREEN : Theme::BLUE, Theme::SURFACE);
        const char* nm = (_config && _config->zone(z).name[0])
                         ? _config->zone(z).name : (z==0?"Zone 1":"Zone 2");
        char buf[32];
        snprintf(buf, sizeof(buf), "Zone %d : %s", z + 1, nm);
        _tft.drawString(buf, 10, y); y += 18;
    }

    y += 6;
    auto row = [&](const char* label, const String& val) {
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString(label, 10, y);
        _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
        _tft.drawString(val.c_str(), 140, y);
        y += 18;
    };

    if (_config) {
        row("Duree max :",    String(_config->system().maxWateringMin) + " min");
        row("Duree manuel :", String(_config->manual().durationMin)    + " min");
    }

    y += 6;
    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("Config via navigateur :", 10, y); y += 14;
    // Afficher l'IP plutôt que l'URL de l'API
    String ip = WiFi.localIP().toString();
    _tft.setTextColor(Theme::CYAN, Theme::SURFACE);
    _tft.drawString(("http://" + (ip.length() > 3 ? ip : "...")).c_str(), 10, y);
}

void DisplayManager::drawAdminPageSystem() {
    int y = ADM_CONTENT_Y + 8;
    _tft.setTextSize(1);

    auto row = [&](const char* label, const String& val) {
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString(label, 10, y);
        _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
        _tft.drawString(val.c_str(), 130, y);
        y += 18;
    };

    row("IP :", WiFi.localIP().toString());
    row("Heap libre :", String(ESP.getFreeHeap()) + " o");
    row("Heap min :",  String(ESP.getMinFreeHeap()) + " o");
    uint32_t up = millis() / 1000UL;
    char ubuf[20];
    snprintf(ubuf, sizeof(ubuf), "%luh%02lum%02lus",
             up/3600UL, (up%3600UL)/60UL, up%60UL);
    row("Uptime :", String(ubuf));
    row("Chip rev :", String(ESP.getChipRevision()));

    y += 8;
    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.drawString("Reset config : voir navigateur", 10, y);
}

// ─────────────────────────────────────────────
//  Page ADMIN : LOGS
//  Liste les entrées EventLog (plus récentes en haut).
//  Affiche ERROR en rouge, WARN en orange, INFO en gris.
//  Hauteur utile ADM_CONTENT_H=172px, ~10 lignes de 17px.
// ─────────────────────────────────────────────
void DisplayManager::drawAdminPageLogs() {
    _tft.fillRect(0, ADM_CONTENT_Y, SCREEN_W, ADM_CONTENT_H, Theme::SURFACE);

    uint8_t n = EventLog::count();
    if (n == 0) {
        _tft.setTextSize(1);
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString("Aucun evenement", 10, ADM_CONTENT_Y + 80);
        return;
    }

    // Géométrie :
    //   Préfixe  x=2      : "MM:SS" (5 chars × 6px = 30px)
    //   Bloc niveau x=34  : 4×8px
    //   Message  x=42     : 274px disponibles = 45 chars/ligne
    //   Wrap automatique si message > 45 chars → 2 lignes de 10px + gap 2px
    static constexpr uint8_t  CHAR_W    = 6;    // largeur police bitmap taille 1
    static constexpr uint8_t  CHAR_H    = 8;    // hauteur police bitmap taille 1
    static constexpr uint8_t  MSG_X     = 42;
    static constexpr uint8_t  MSG_W     = (SCREEN_W - MSG_X - 4) / CHAR_W;  // 45 chars
    static constexpr uint8_t  LINE1_H   = CHAR_H + 2;   // 10px — 1ère ligne message
    static constexpr uint8_t  LINE2_H   = CHAR_H + 1;   // 9px  — 2ème ligne (wrap)
    static constexpr uint8_t  ROW_PAD   = 2;             // padding vertical entre entrées
    static constexpr uint8_t  ROW_1LINE = LINE1_H + ROW_PAD;        // 12px si 1 ligne
    static constexpr uint8_t  ROW_2LINE = LINE1_H + LINE2_H + ROW_PAD; // 21px si wrap

    _tft.setTextSize(1);
    _tft.setFreeFont(nullptr);

    int curY = ADM_CONTENT_Y + 2;
    uint8_t shown = 0;

    for (uint8_t i = 0; i < n; i++) {
        const LogEntry& e = EventLog::get(i);
        uint16_t col  = EventLog::levelColor(e.level);

        // Calcul hauteur de cette entrée (1 ou 2 lignes)
        uint8_t msgLen  = strlen(e.msg);
        bool    doWrap  = (msgLen > MSG_W);
        uint8_t rowH    = doWrap ? ROW_2LINE : ROW_1LINE;

        // Vérifier qu'on a encore de la place
        if (curY + rowH > ADM_CONTENT_Y + ADM_CONTENT_H - 10) break;

        // ── Fond coloré sur toute la largeur pour WARN/ERROR ──────
        if (e.level >= LOG_WARN) {
            uint16_t bgCol = (e.level == LOG_ERROR) ? 0x2000 : 0x2200; // rouge foncé / orange foncé
            _tft.fillRect(0, curY - 1, SCREEN_W, rowH - 1, bgCol);
        }

        // ── Préfixe temps MM:SS ────────────────────────────────────
        uint32_t s = e.ms / 1000;
        char tBuf[7];
        snprintf(tBuf, sizeof(tBuf), "%02lu:%02lu", (s / 60) % 100, s % 60);
        _tft.setTextColor(Theme::MUTED,
                          e.level >= LOG_WARN ? (e.level == LOG_ERROR ? 0x2000 : 0x2200)
                                              : Theme::SURFACE);
        _tft.drawString(tBuf, 2, curY + 1);

        // ── Bloc niveau ────────────────────────────────────────────
        _tft.fillRect(34, curY + 1, 4, CHAR_H, col);

        // ── Ligne 1 du message ─────────────────────────────────────
        char line1[MSG_W + 1];
        strlcpy(line1, e.msg, MSG_W + 1);  // tronqué à MSG_W chars
        uint16_t msgBg = (e.level == LOG_ERROR) ? 0x2000 :
                         (e.level == LOG_WARN)  ? 0x2200 : Theme::SURFACE;
        _tft.setTextColor(col, msgBg);
        _tft.drawString(line1, MSG_X, curY + 1);

        // ── Ligne 2 si wrap ────────────────────────────────────────
        if (doWrap) {
            const char* line2Start = e.msg + MSG_W;
            char line2[MSG_W + 1];
            strlcpy(line2, line2Start, MSG_W + 1);
            _tft.drawString(line2, MSG_X, curY + LINE1_H + 1);
        }

        curY += rowH;
        shown++;
    }

    // ── Indicateur si entrées non affichées ────────────────────────
    if (shown < n) {
        char more[24];
        snprintf(more, sizeof(more), "+%d", n - shown);
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString(more, SCREEN_W - 20, ADM_CONTENT_Y + ADM_CONTENT_H - 12);
    }
}

// ═══════════════════════════════════════════════════════════════
//  Helpers NTP
// ═══════════════════════════════════════════════════════════════
int DisplayManager::jsToEsp(int tmWday) {
    // tm_wday : 0=dim..6=sam → ESP index : 0=lun..6=dim
    return (tmWday == 0) ? 6 : tmWday - 1;
}

int DisplayManager::todayEspIdx() {
    if (!_ntp || !_ntp->isSynced()) return -1;  // -1 = pas de colonne surlignée
    return jsToEsp(_ntp->getWeekday());
}