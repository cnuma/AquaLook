#include "DisplayManager.h"
#include "EventBus.h"
#include "BootLoopGuard.h"
#include "NotificationManager.h"   // marqueur "MAJ DISPO" du bandeau
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
    // strtoul plutot que sscanf : voir CloudSync::isPrivateAddress, la famille
    // scanf de la libc coute ~17 Ko et n'etait utilisee que sur des entiers.
    char comp[3] = {0, 0, 0};
    unsigned rgb[3] = {0U, 0U, 0U};
    for (uint8_t i = 0U; i < 3U; ++i) {
        comp[0] = hex[1 + i * 2];
        comp[1] = hex[2 + i * 2];
        rgb[i] = (unsigned)strtoul(comp, nullptr, 16);
    }
    const unsigned r = rgb[0], g = rgb[1], b = rgb[2];
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}


// Barre horaire d'un creneau d'arrosage, sur le planning.
//
// Passage OBLIGE des trois modes d'affichage (LIST, GRID2, GRID4) : une
// barre etroite ne doit PAS etre dessinee avec fillRoundRect. Celui-ci
// degenere des que la largeur descend a 2 px avec un rayon de 1 - son
// remplissage central vaut fillRect(x+1, y, w-2r=0, h), soit rien, et seuls
// les quatre arcs de coin subsistent : la barre s'affiche comme un crochet
// "[". Or un arrosage de 10 min sur une colonne de 65 px fait 0,4 px, donc
// ramene au minimum de 2 px : le cas degenere est la regle, pas l'exception.
//
// Corrige d'abord dans le seul mode LIST le 29 aout 2026 ; les deux autres
// modes ont ressorti le meme defaut aussitot. D'ou ce point de passage
// unique plutot qu'une troisieme correction locale.
template <typename Gfx>
static void drawSlotBar(Gfx& gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (w <= 4) gfx.fillRect(x, y, w, h, color);
    else        gfx.fillRoundRect(x, y, w, h, 1, color);
}

// Petite goutte, posee devant la valeur de pluie pour dire ce qu'elle
// represente sans depenser la largeur d'un mot ("Pluie" mangerait la moitie
// d'une colonne de 65 px). Le vent, lui, est deja annonce par sa fleche.
template <typename Gfx>
static void drawDropIcon(Gfx& gfx, int16_t x, int16_t y, uint16_t color) {
    gfx.fillTriangle(x + 3, y, x, y + 4, x + 6, y + 4, color);
    gfx.fillCircle(x + 3, y + 5, 3, color);
}

static uint16_t weatherTempBg565(float tempC) {
    if (tempC < 5.0f)  return 0x11A9; // bleu froid sombre
    if (tempC < 12.0f) return 0x1A4B; // bleu clair sombre
    if (tempC < 20.0f) return 0x2246; // vert sombre
    if (tempC < 27.0f) return 0x4A24; // ambre sombre
    return 0x49A4;                    // rouge sombre
}

static uint8_t weatherRainBarHeight(float rainMm, uint8_t maxHeight) {
    if (rainMm <= 0.0f || maxHeight == 0) return 0;
    float ratio = rainMm / 20.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    uint8_t h = (uint8_t)(ratio * maxHeight + 0.5f);
    return h == 0 ? 1 : h;
}

#if AQUALOOK_BOARD_S3
// Point cardinal d'ou vient le vent, meme convention que la page Web
// (weatherWindCardinal dans data/app.js) : 8 secteurs de 45°, centres sur
// le nord. Rendu en 2 caracteres au plus pour tenir dans une colonne.
static const char* weatherWindCardinal(int16_t deg) {
    if (deg < 0) return "";
    static const char* kPoints[8] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    return kPoints[(uint8_t)(((deg + 22) % 360) / 45)];
}

// Petite fleche indiquant la direction du vent. Huit orientations
// seulement : a cette taille (7x7 px) une rotation continue serait
// illisible, et le cardinal affiche a cote leve toute ambiguite.
// Gabarit : appelee tantot sur un sprite (bandeau planning 1-4 zones),
// tantot directement sur l'ecran (colonne compacte 5-8 zones).
template <typename Gfx>
static void drawWindArrow(Gfx& spr, int16_t cx, int16_t cy,
                          int16_t deg, uint16_t color) {
    if (deg < 0) return;
    // La fleche montre OU VA le vent, pas d'ou il vient : un vent de nord
    // (deg=0, convention meteo "provenance") pointe donc vers le BAS.
    // Choix explicite de l'utilisateur le 29 aout 2026 - le libelle cardinal
    // affiche a cote garde, lui, la convention meteo de provenance.
    const uint8_t sector = (uint8_t)(((deg + 180 + 22) % 360) / 45);
    // Vecteur unitaire approche par secteur (x vers la droite, y vers le bas).
    static const int8_t dx[8] = {  0,  2,  3,  2,  0, -2, -3, -2 };
    static const int8_t dy[8] = { -3, -2,  0,  2,  3,  2,  0, -2 };
    const int16_t tipX = cx + dx[sector];
    const int16_t tipY = cy + dy[sector];
    spr.drawLine(cx - dx[sector], cy - dy[sector], tipX, tipY, color);
    // Deux barbes formant la pointe, perpendiculaires au vecteur.
    spr.drawLine(tipX, tipY, tipX - dx[sector] / 2 + dy[sector] / 2,
                 tipY - dy[sector] / 2 - dx[sector] / 2, color);
    spr.drawLine(tipX, tipY, tipX - dx[sector] / 2 - dy[sector] / 2,
                 tipY - dy[sector] / 2 + dx[sector] / 2, color);
}
// ── Vent et rafales dans une cellule meteo ──────────────────────────
//
// Seuils choisis avec l'utilisateur le 29 aout 2026, inspires de la
// presentation de Meteo-France :
//   >= 30 km/h de rafale : la vitesse de vent est REMPLACEE par la rafale,
//                          sur pastille rouge - on parle alors de rafale,
//                          pas de vent moyen ;
//   >= 50 km/h de vent   : toute la cellule du jour passe sur fond rouge.
//
// Substitution et non alternance : le bandeau planning n'est redessine que
// sur changement de donnees, une animation imposerait de forcer un redessin
// permanent pour un gain de lisibilite discutable.
static constexpr float WIND_GUST_ALERT_KMH   = 30.0f;
static constexpr float WIND_SEVERE_KMH       = 50.0f;
static constexpr uint16_t WIND_ALERT_BG      = 0xC000;  // rouge sombre
static constexpr uint16_t WIND_SEVERE_CELL_BG = 0x5000; // rouge tres sombre

// Seuils lus dans la configuration : l'utilisateur choisit a partir de quelle
// valeur il veut voir l'alerte, plutot que de subir un chiffre code en dur.
// Les constantes ci-dessus ne servent plus que de repli si la configuration
// n'est pas encore chargee.
static bool weatherWindIsGusty(const ForecastDay& fd, uint8_t thresholdKmh) {
    return fd.valid && fd.gustMaxKmh >= (float)thresholdKmh;
}

static bool weatherWindIsSevere(const ForecastDay& fd, uint8_t thresholdKmh) {
    return fd.valid && fd.windMaxKmh >= (float)thresholdKmh;
}

// Dessine la ligne vent (ou rafale) centree sur cx. Retourne la largeur
// occupee, pour que l'appelant puisse centrer d'autres elements dessus.
template <typename Gfx>
static void drawWindLine(Gfx& gfx, int16_t cx, int16_t y,
                         const ForecastDay& fd, uint16_t cellBg,
                         uint8_t gustThresholdKmh) {
    const bool gusty = weatherWindIsGusty(fd, gustThresholdKmh);
    char buf[16];
    // Pas de point cardinal dans le texte : la fleche porte deja la
    // direction, et "SO 12 km/h" (10 caracteres, 60 px a cette taille)
    // debordait sur les colonnes voisines d'une grille de 65 px. Meteo-France
    // procede de meme - fleche puis vitesse, sans cardinal.
    snprintf(buf, sizeof(buf), "%.0f km/h",
             gusty ? fd.gustMaxKmh : fd.windMaxKmh);

    const int16_t textW = (int16_t)strlen(buf) * 6;
    const int16_t groupW = 10 + textW;
    const int16_t groupX = cx - groupW / 2;

    if (gusty) {
        // Pastille rouge : la valeur affichee n'est plus le vent moyen.
        gfx.fillRoundRect(groupX - 2, y - 5, groupW + 4, 11, 4, WIND_ALERT_BG);
        gfx.setTextColor(Theme::TEXT, WIND_ALERT_BG);
    } else {
        // Fond REEL de la cellule, pas Theme::BG : le texte opaque decoupait
        // sinon un rectangle noir dans le fond colore de la journee.
        gfx.setTextColor(Theme::MUTED, cellBg);
    }
    drawWindArrow(gfx, groupX + 4, y, fd.windDeg,
                  gusty ? Theme::TEXT : Theme::MUTED);
    gfx.setTextDatum(ML_DATUM);
    gfx.drawString(buf, groupX + 10, y);
    gfx.setTextDatum(TL_DATUM);
}

#endif  // AQUALOOK_BOARD_S3

// ─────────────────────────────────────────────────────────────
//  Remplit un rectangle avec un motif de hachures diagonales plutot
//  qu'un aplat uni — plus explicite pour signaler un etat particulier
//  (ex. "arrosage suspendu (pluie)") au premier coup d'oeil. Meme esprit
//  que le hachurage cote Web pour le meme cas (.pg-day.rain,
//  style-base.css). TFT_eSprite herite de TFT_eSPI (voir drawCardBg
//  plus bas) : cette fonction s'utilise aussi bien avec _tft qu'avec
//  n'importe quel sprite (_sprPlan...).
// ─────────────────────────────────────────────────────────────
static void fillHatchRect(
    TFT_eSPI& gfx,
    int x, int y, int w, int h,
    uint16_t bgColor, uint16_t stripeColor,
    int spacing = 5
) {
    gfx.fillRect(x, y, w, h, bgColor);
    for (int d = -h; d < w; d += spacing) {
        int x0 = x + d,     y0 = y;
        int x1 = x + d + h, y1 = y + h;
        if (x0 < x) { y0 += (x - x0); x0 = x; }
        if (x1 > x + w) { y1 -= (x1 - (x + w)); x1 = x + w; }
        if (x0 <= x1) gfx.drawLine(x0, y0, x1, y1, stripeColor);
    }
}


// Indique si une colonne du planning correspond à un jour réellement prévu
// pour une zone en mode intervalle. Le calcul reprend la logique d'exécution :
// premier arrosage aujourd'hui si aucun historique, sinon dernier jour + intervalle.
static bool intervalDayIsPlanned(const ZoneSchedule& zs,
                                 uint32_t todayEpochDay,
                                 uint8_t daysAhead) {
    const uint32_t targetDay = todayEpochDay + daysAhead;
    const uint32_t interval  = zs.intervalDays > 0 ? zs.intervalDays : 1;
    const uint32_t anchor    = zs.intervalAnchorDay;

    return anchor > 0 &&
           targetDay >= anchor &&
           ((targetDay - anchor) % interval) == 0;
}

// Arrosage prévu ce jour-là, mais suspendu parce que la pluie annoncée
// atteint le seuil de la zone.
//
// Point de passage unique : la condition était écrite à l'identique dans
// les trois rendus de planning (LIST, GRID2, GRID4), et le voyant WS2812
// en aurait ajouté une quatrième copie. Or c'est exactement le travers
// qui a imposé de corriger trois fois le même défaut avant que
// HeapMetrics.h n'existe — une règle métier recopiée finit toujours par
// diverger d'un site à l'autre.
//
// Les trois sites étaient déjà subtilement différents : deux gardaient
// l'appel météo derrière un `col < 5`, le troisième non. Sans effet ici,
// getForecastDay() bornant lui-même son argument, mais l'écart montre
// bien la dérive commencée.
static bool rainBlocksDay(const ZoneSchedule& zs,
                          const DaySchedule& ds,
                          uint8_t col,
                          const WeatherManager* weather) {
    bool hasAny = false;
    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
        if (ds.slots[s].enabled) { hasAny = true; break; }
    }
    if (!hasAny) return false;

    const ForecastDay fd = weather ? weather->getForecastDay(col) : ForecastDay{};
    return fd.valid && fd.rainMm >= zs.rain.thresholdMm;
}

// SCREEN_W / SCREEN_H sont desormais des constantes publiques de la
// classe (DisplayManager.h) : elles etaient definies ici en #define, donc
// invisibles des autres fichiers qui dessinent sur le meme ecran.

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
    if (y >= SCREEN_H) return true;  // hors écran
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
                            ConfigManager* config, WiFiManager* wifi) {
    _ntp      = ntp;
    _weather  = weather;
    _relais   = relais;
    _schedule = schedule;
    _config   = config;
    _wifi     = wifi;

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

#if AQUALOOK_TOUCH_GT911
    // GT911 capacitif, I2C dedie - pas de calibration (voir getTouchPoint()),
    // rotation confirmee sur materiel reel le 27 aout 2026.
    _touch.begin();
    _touch.setRotation(AQ_S3_TOUCH_ROTATION);

    // 400 kHz (mode rapide), au lieu des 100 kHz par defaut d'Arduino que
    // TAMC_GT911::begin() laisse en place via son Wire.begin().
    Wire.setClock(400000UL);

    // Delai d'expiration ramene de 50 ms (defaut Arduino) a 10 ms. La
    // scrutation tactile coutait 100 ms par passage, toutes les 80 ms,
    // mesure le 29 aout 2026 en instrumentant DisplayManager::update() :
    // c'est exactement DEUX expirations de 50 ms, et TAMC_GT911::read()
    // fait exactement deux transactions quand rien n'est touche (lecture
    // du registre d'etat, puis remise a zero). Autrement dit le controleur
    // ne repondait pas du tout - la vitesse du bus n'y changeait rien.
    // Borner le delai empeche qu'un tactile muet bloque la boucle
    // principale, quel que soit le resultat du diagnostic ci-dessous.
    Wire.setTimeOut(10);

    // Balayage du bus tactile, comme celui deja fait sur le bus relais
    // dans main.cpp : dit sans ambiguite si le GT911 repond, et a quelle
    // adresse (la sienne depend de la sequence de reset INT/RST : 0x5D ou
    // 0x14). test_touch_s3.cpp fonctionnait sur ce meme brochage, donc un
    // silence ici signalerait une difference d'integration, pas un defaut
    // materiel.
    uint8_t touchFound = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[Touch] I2C: peripherique a 0x%02X\n", addr);
            touchFound++;
        }
    }
    Serial.printf("[Touch] I2C: %u peripherique(s) sur SDA=%d SCL=%d\n",
                  touchFound, (int)AQ_S3_TOUCH_SDA, (int)AQ_S3_TOUCH_SCL);
#else
    // Invariant I5 : XPT2046 direct, bus VSPI séparé
    // Note : le warning addApbChangeCallback vient de TFT_eSPI qui ré-enregistre
    // son callback APB lors du second passage dans begin(). Suppression du log parasite
    // en désactivant temporairement les logs ESP pendant l'init du touch.
    esp_log_level_set("*", ESP_LOG_ERROR);  // masquer uniquement pendant init SPI/touch
    _touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    _touch.begin(_touchSPI);
    esp_log_level_set("*", ESP_LOG_WARN);   // restaurer apres initialisation complete
    _touch.setRotation(1);
#endif

    // Déterminer le mode HOME selon le nb de zones actives
    _nbZones = _config ? _config->nbZones() : NB_ZONES;
    if (_nbZones > 8) _nbZones = 8;  // mode 16 zones retiré de l'interface
    if (_nbZones <= 4) _homeMode = HomeMode::LIST;
    else               _homeMode = HomeMode::GRID2;

    updateGrid2Geometry();
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
    _sprTime.createSprite(HDR_TIME_W, 20);  // heure size2 + température size1 côte à côte
    _sprSignal.createSprite(HDR_SIGNAL_W, 16);
    _sprPlan.createSprite(PL_PLAN_W, PL_PLAN_H);
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
// Ecran de mise a jour : violet plein, message centre, retroeclairage
// rallume.
//
// Appelee juste avant le redemarrage en mode maintenance. C'est le seul
// moment ou l'on peut encore dessiner : pendant la maintenance, setup()
// est intercepte et aucun code d'affichage ne tourne.
//
// L'image survit pourtant a tout le redemarrage, parce que la dalle
// NV3041A n'a pas de broche de reset cablee sur cette carte (voir la
// section [jc4827w543c_i] de platformio.ini) : son controleur garde sa
// memoire d'image tant qu'il est alimente. MaintenanceBoot se contente
// donc de rallumer le retroeclairage pour la rendre visible.
//
// L'ecran est reveille de force : une mise a jour peut etre declenchee
// depuis la page Web alors que l'ecran dort, et c'est precisement le cas
// ou l'utilisateur, passant devant le module, doit comprendre qu'il ne
// faut pas y toucher.
void DisplayManager::showUpdateScreen(const char* title, const char* message) {
    _screenMgr.wakeUp();

    _tft.fillScreen(Theme::PURPLE);
    _tft.setFreeFont(nullptr);
    _tft.setTextSize(1);
    _tft.setTextDatum(MC_DATUM);

    // Texte clair sur fond violet, et fond de texte EGAL au fond de
    // l'ecran : passer Theme::BG ici peindrait un rectangle sombre autour
    // de chaque ligne - le defaut deja rencontre sur les cellules meteo et
    // sur le bandeau.
    _tft.setTextColor(Theme::TEXT, Theme::PURPLE);
    _tft.setFreeFont(THEME_FONT_TITLE);
    _tft.drawString(title, SCREEN_W / 2, SCREEN_H / 2 - 16);

    _tft.setFreeFont(nullptr);
    _tft.drawString(message, SCREEN_W / 2, SCREEN_H / 2 + 12);
    _tft.drawString("Ne pas eteindre le module",
                    SCREEN_W / 2, SCREEN_H / 2 + 30);

    _tft.setTextDatum(TL_DATUM);
}

uint16_t DisplayManager::rainBlockedMaskToday() {
    if (!_schedule) return 0U;

    const int      todayIdx      = todayEspIdx();
    const uint8_t  baseIdx       = (todayIdx >= 0) ? (uint8_t)todayIdx : 0U;
    const uint32_t todayEpochDay = (_ntp && _ntp->isSynced()) ? _ntp->getEpochDay() : 0U;

    uint16_t mask = 0U;
    for (uint8_t z = 0; z < _nbZones && z < MAX_ZONES; z++) {
        const ZoneSchedule zs = _schedule->getZoneSchedule(z);

        // Mode intervalle : le jour doit d'abord être un jour d'arrosage.
        // Même garde que les rendus de planning, colonne 0 = aujourd'hui.
        if (zs.mode != 0) {
            if (todayEpochDay == 0U ||
                !intervalDayIsPlanned(zs, todayEpochDay, 0U)) {
                continue;
            }
        }
        const DaySchedule& ds = (zs.mode == 0) ? zs.daySlots[baseIdx]
                                               : zs.intervalSlots;
        if (rainBlocksDay(zs, ds, 0U, _weather)) {
            mask |= (uint16_t)(1U << z);
        }
    }
    return mask;
}

void DisplayManager::update() {
    const uint32_t now = millis();

    // ScreenManager — veille/réveil/LED
    // Vérification sur toutes les zones actives (pas uniquement Z0/Z1)
    // Le masque sert au ruban WS2812 de la carte S3, qui a une LED par
    // zone : on ne peut donc plus sortir de la boucle des la premiere
    // zone active, il faut les relever toutes.
    bool anyActive = false;
    uint16_t activeZoneMask = 0U;
    if (_relais) {
        for (uint8_t z = 0; z < _nbZones && z < MAX_ZONES; z++) {
            if (_relais->getState(z)) {
                anyActive = true;
                activeZoneMask |= (uint16_t)(1U << z);
            }
        }
    }
    // Masque de blocage pluie, uniquement là où il sert : la carte S3 et son
    // ruban WS2812. Sur la carte historique, dont le voyant unique ne peut
    // rien en faire, le calcul est neutralisé à la compilation — le laisser
    // tourner serait payer pour un résultat jeté, comme renderZones().
    //
    // Rafraîchi périodiquement et non à chaque tour de boucle :
    // rainBlockedMaskToday() recopie le planning complet de chaque zone, et
    // l'état qu'il décrit ne bouge qu'au rythme des prévisions météo (une
    // fois par heure) ou d'une modification de configuration. Un retard de
    // quelques secondes est sans conséquence — contrairement au masque
    // d'arrosage en cours juste au-dessus, lui recalculé à chaque passage
    // pour que le déclenchement soit visible immédiatement.
#if AQUALOOK_BOARD_S3
    constexpr uint32_t RAIN_MASK_REFRESH_MS = 10000UL;
    if (_rainMaskAtMs == 0U || (now - _rainMaskAtMs) >= RAIN_MASK_REFRESH_MS) {
        _rainMaskAtMs   = now;
        _rainMaskCache  = rainBlockedMaskToday();
    }
    const uint16_t rainMask = _rainMaskCache;
#else
    const uint16_t rainMask = 0U;
#endif
    _screenMgr.update(anyActive, isWifiSearching(), activeZoneMask, _nbZones, rainMask);

    // Si en veille : ne pas redessiner, juste gérer le touch pour réveil
    if (_screenMgr.isAsleep()) {
        // Ecran eteint : plus rien ne dessine (on sort juste en dessous sans
        // redraw), donc les deux gros sprites ne servent a rien. Les liberer
        // rend ~95 Ko au tas, ce qui conditionne directement le nombre de
        // connexions HTTP simultanees que le module peut honorer.
        // Mesure du 16 aout 2026 : sprites alloues, 3 requetes simultanees
        // suffisaient a rendre le serveur muet (page blanche, /app.js en
        // echec, /index.html a 25 s) ; sprites liberes, 6 requetes passent
        // toutes en moins de 1,7 s. Un navigateur ouvre couramment 6
        // connexions paralleles pour charger une page.
        suspendForMemoryRelief();

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

    // Ecran allume : les sprites doivent exister avant le moindre rendu.
    // Applique ici plutot qu'au seul instant du reveil parce que le reveil
    // peut venir de plusieurs chemins (touch, demarrage d'arrosage via
    // ScreenManager::update, appel direct a wakeUp()) et que la verification
    // HTTPS d'une ressource Web libere aussi ces sprites de son cote : cet
    // invariant, reevalue a chaque passage, rattrape tous ces cas.
    if (_spritesFreed) {
        // Tentative espacee : en cas d'echec, reessayer a chaque iteration
        // enchainerait des allocations vouees a echouer et ajouterait de la
        // charge au moment precis ou la memoire manque.
        if (!AquaLook::Time::elapsedAtLeast(now, _lastSpriteRetryMs,
                                            SPRITE_RETRY_INTERVAL_MS)) {
            return;
        }
        _lastSpriteRetryMs = now;

        if (!resumeAfterMemoryRelief()) {
            return;   // defaut deja signale, on ne dessine pas dans le vide
        }
        _needsFullRedraw = true;
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

#if AQUALOOK_BOARD_S3
    // Encart meteo ouvert : ne rien redessiner dessous, le rendu periodique
    // repasserait par-dessus et le hacherait.
    if (_wxPopupDay >= 0) return;
#endif

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
    // Couleur du bandeau : elle porte l'etat global du module.
    //
    //   ambre   mode degrade - le module a redemarre plusieurs fois de suite
    //           et s'est mis en securite. Meteo, verification des mises a
    //           jour et notifications sont suspendues, et il faut une action
    //           de l'utilisateur. Etat DURABLE, donc prioritaire sur le
    //           violet : une mise a jour ne peut de toute facon pas etre en
    //           cours puisqu'elles sont justement suspendues.
    //   violet  mise a jour engagee - operation en cours, ne pas solliciter
    //           le module. Meme teinte que le voyant et que la pastille Web.
    //   gris    fonctionnement nominal.
    //
    // Le rouge n'est pas utilise ici : il reste reserve a une panne active
    // signalee par FaultManager, pour qu'il garde son sens au premier coup
    // d'oeil.
    //
    //   Mise a jour DISPONIBLE : le bandeau reste gris, seul le marqueur de
    //   droite passe en violet. Deliberement pas de bandeau plein : une mise
    //   a jour disponible peut le rester des jours, et un bandeau violet
    //   permanent dirait "ne touche a rien" en permanence — exactement le
    //   contraire du message. Le violet plein reste reserve a l'operation en
    //   cours, qui, elle, dure quelques minutes.
    uint16_t headerBg = Theme::SURFACE;
    if (BootLoopGuard::isDegraded())      headerBg = Theme::AMBER;
    else if (EventBus::updateInProgress)  headerBg = Theme::PURPLE;
    const bool updateReady =
        (headerBg == Theme::SURFACE) && NotificationManager::updateAvailable();
    _tft.fillRect(0, 0, SCREEN_W, 28, headerBg);
    _tft.drawFastHLine(0, 27, SCREEN_W, Theme::BORDER);
    _tft.setFreeFont(nullptr);
    _tft.setTextSize(1);

    // Le fond passe a setTextColor doit etre la couleur REELLE du bandeau,
    // et non Theme::SURFACE en dur : sinon chaque texte peint son propre
    // rectangle gris et decoupe le bandeau colore. Meme defaut que celui
    // corrige le 29 aout 2026 sur les cellules meteo.
    if (backBtn) {
        _tft.setTextColor(Theme::MUTED, headerBg);
        _tft.drawString("<", 8, 10);
    }

    _tft.setTextColor(Theme::TEXT, headerBg);
    _tft.setTextDatum(MC_DATUM);
    _tft.setFreeFont(THEME_FONT_TITLE);
    _tft.drawString(title, SCREEN_W / 2, 14);
    _tft.setFreeFont(nullptr);

    // Marqueur a droite : la couleur seule ne dit pas POURQUOI. Deux mots
    // suffisent a orienter vers l'interface Web, qui porte l'explication
    // complete et le bouton de reactivation.
    if (headerBg != Theme::SURFACE) {
        _tft.setTextColor(Theme::BG, headerBg);   // sombre sur fond vif
        _tft.setTextDatum(MR_DATUM);
        _tft.drawString(BootLoopGuard::isDegraded() ? "DEGRADE" : "MAJ...",
                        SCREEN_W - 8, 14);
    } else if (updateReady) {
        // Violet sur le gris du bandeau : meme teinte que le clignotement du
        // voyant et que la pastille de l'interface Web, pour que les trois
        // surfaces disent la meme chose avec la meme couleur.
        _tft.setTextColor(Theme::PURPLE, headerBg);
        _tft.setTextDatum(MR_DATUM);
        _tft.drawString("MAJ DISPO", SCREEN_W - 8, 14);
    }
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
#if AQUALOOK_TOUCH_GT911
    // Capacitif : coordonnees natives en pixels ecran, pas d'etalonnage
    // (§5 du document d'impact - TOUCH_X_MIN/MAX etc. sans objet ici).
    // ts.touches a ete observe jusqu'a 14 sur cette carte reelle alors
    // que TAMC_GT911.h ne declare que points[5] - se limiter au premier
    // contact et ne jamais boucler sur ts.touches sans le borner (bug
    // trouve par test_touch_s3.cpp le 27 aout 2026).
    _touch.read();
    if (!_touch.isTouched || _touch.touches < 1) return false;
    tx = (uint16_t)constrain((int)_touch.points[0].x, 0, SCREEN_W - 1);
    ty = (uint16_t)constrain((int)_touch.points[0].y, 0, SCREEN_H - 1);
    return true;
#else
    if (!_touch.tirqTouched() || !_touch.touched()) return false;
    TS_Point p = _touch.getPoint();

    int16_t xMin = _config ? _config->touch().xMin : TOUCH_X_MIN;
    int16_t xMax = _config ? _config->touch().xMax : TOUCH_X_MAX;
    int16_t yMin = _config ? _config->touch().yMin : TOUCH_Y_MIN;
    int16_t yMax = _config ? _config->touch().yMax : TOUCH_Y_MAX;

    tx = (uint16_t)constrain(map(p.x, xMin, xMax, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
    ty = (uint16_t)constrain(map(p.y, yMin, yMax, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
    return true;
#endif
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
    // Sur l'ecran "non cable" aucune tuile n'existe : seul le menu reste
    // actif, sans quoi l'utilisateur serait enferme sur cet ecran.
    if (homeUnwired()) {
        if (hitTest(0, 0, 40, G2_HDR_H, tx, ty)) goTo(Screen::ADMIN);
        return;
    }
    switch (_homeMode) {
        case HomeMode::LIST:  handleTouchHome_list(tx, ty);  break;
        case HomeMode::GRID2: handleTouchHome_grid2(tx, ty); break;
        case HomeMode::GRID4: handleTouchHome_grid4(tx, ty); break;
    }
}

#if AQUALOOK_BOARD_S3
// ── Encart meteo detaille ───────────────────────────────────────────
//
// Reprend l'infobulle de la page Web : toutes ces donnees sont deja dans
// ForecastDay mais aucune ne tenait dans une colonne de 65 px. Le bandeau
// planning ne reagissait a rien au-dessus des lignes de zone, cette bande
// etait donc libre pour ouvrir l'encart.
//
// Dessine directement sur l'ecran, sans sprite : il est pose une seule fois
// a l'ouverture, pas rafraichi, et un tampon de 320x190 couterait 118 Ko de
// PSRAM pour rien.
void DisplayManager::drawWeatherPopup(uint8_t dayCol) {
    const ForecastDay fd = _weather ? _weather->getForecastDay(dayCol)
                                    : ForecastDay{};

    constexpr int16_t W = 330;
    constexpr int16_t H = 196;
    const int16_t x = (SCREEN_W - W) / 2;
    const int16_t y = (SCREEN_H - H) / 2;

    drawCardBg(_tft, x, y, W, H, Theme::R_LG, Theme::SURFACE, Theme::BORDER, false);

    static const char* kJours[] = {"Lundi", "Mardi", "Mercredi", "Jeudi",
                                   "Vendredi", "Samedi", "Dimanche"};
    const int today = todayEspIdx();
    const int espIdx = ((today >= 0 ? today : 0) + dayCol) % 7;

    _tft.setFreeFont(nullptr);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextSize(2);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    _tft.drawString(kJours[espIdx], x + 14, y + 12);

    _tft.setTextSize(1);
    if (!fd.valid) {
        _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
        _tft.drawString("Aucune prevision disponible pour ce jour.", x + 14, y + 44);
        _tft.setTextDatum(BC_DATUM);
        _tft.drawString("Toucher pour fermer", x + W / 2, y + H - 10);
        _tft.setTextDatum(TL_DATUM);
        return;
    }

    if (fd.description[0]) {
        _tft.setTextColor(Theme::TEXT2, Theme::SURFACE);
        _tft.drawString(fd.description, x + 14, y + 34);
    }
    _tft.drawFastHLine(x + 14, y + 48, W - 28, Theme::BORDER);

    // Deux colonnes de couples libelle/valeur : la largeur de l'encart le
    // permet et cela evite une liste de huit lignes difficile a parcourir.
    struct Row { const char* label; char value[16]; };
    Row left[4];
    Row right[4];

    snprintf(left[0].value, sizeof(left[0].value), "%.0f / %.0f C",
             fd.tempMin, fd.tempMax);
    left[0].label = "Temperature";
    snprintf(left[1].value, sizeof(left[1].value), "%.0f C", fd.feelsLikeMax);
    left[1].label = "Ressenti max";
    snprintf(left[2].value, sizeof(left[2].value), "%.1f mm", fd.rainMm);
    left[2].label = "Pluie";
    snprintf(left[3].value, sizeof(left[3].value), "%u %%",
             (unsigned)fd.rainProbability);
    left[3].label = "Prob. pluie";

    snprintf(right[0].value, sizeof(right[0].value), "%s %.0f km/h",
             weatherWindCardinal(fd.windDeg), fd.windMaxKmh);
    right[0].label = "Vent";
    snprintf(right[1].value, sizeof(right[1].value), "%.0f km/h", fd.gustMaxKmh);
    right[1].label = "Rafales";
    snprintf(right[2].value, sizeof(right[2].value), "%u %%",
             (unsigned)fd.humidityMax);
    right[2].label = "Humidite";
    snprintf(right[3].value, sizeof(right[3].value), "%u hPa",
             (unsigned)fd.pressureAvg);
    right[3].label = "Pression";

    const int16_t colX[2] = { (int16_t)(x + 14), (int16_t)(x + W / 2 + 4) };
    for (uint8_t i = 0; i < 4; i++) {
        const int16_t rowY = y + 58 + i * 26;
        for (uint8_t c = 0; c < 2; c++) {
            const Row& r = (c == 0) ? left[i] : right[i];
            _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
            _tft.drawString(r.label, colX[c], rowY);
            _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
            _tft.drawString(r.value, colX[c], rowY + 11);
        }
    }

    // Rafales fortes : meme code couleur que la cellule du bandeau, pour que
    // les deux se lisent de la meme facon.
    if (weatherWindIsGusty(fd, _config ? _config->windAlert().gustKmh : 30)) {
        _tft.fillRoundRect(colX[1] - 4, y + 58 + 26 - 3, 96, 25, 5, WIND_ALERT_BG);
        _tft.setTextColor(Theme::TEXT, WIND_ALERT_BG);
        _tft.drawString(right[1].label, colX[1], y + 58 + 26);
        _tft.drawString(right[1].value, colX[1], y + 58 + 26 + 11);
    }

    _tft.setTextColor(Theme::MUTED, Theme::SURFACE);
    _tft.setTextDatum(BC_DATUM);
    _tft.drawString("Toucher pour fermer", x + W / 2, y + H - 10);
    _tft.setTextDatum(TL_DATUM);
}

// Retourne true si le toucher a ete consomme par l'encart (ouverture ou
// fermeture), auquel cas l'appelant ne doit rien faire d'autre.
bool DisplayManager::handleWeatherPopupTouch(uint16_t tx, uint16_t ty) {
    if (_wxPopupDay >= 0) {          // ouvert : n'importe ou pour fermer
        _wxPopupDay = -1;
        _needsFullRedraw = true;
        return true;
    }

    // Bande meteo du bandeau planning, au-dessus des lignes de zone.
    if (ty < PL_PLAN_Y || ty >= PL_PLAN_Y + _planHdrH) return false;
    if (tx < PL_LABEL_W) return false;

    const uint8_t col = (uint8_t)((tx - PL_LABEL_W) / PL_DAY_W);
    if (col >= 5) return false;      // au-dela de J+4, pas de prevision

    _wxPopupDay = (int8_t)col;
    drawWeatherPopup(col);
    return true;
}
#endif  // AQUALOOK_BOARD_S3

void DisplayManager::handleTouchHome_list(uint16_t tx, uint16_t ty) {
#if AQUALOOK_BOARD_S3
    if (handleWeatherPopupTouch(tx, ty)) return;
#endif
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
    // Heure seule dans le bandeau : la température est déjà visible dans le planning.
    _sprTime.fillSprite(Theme::SURFACE);
    _sprTime.setFreeFont(nullptr);
    _sprTime.setTextSize(2);
    _sprTime.setTextColor(Theme::TEXT, Theme::SURFACE);
    _sprTime.setTextDatum(MC_DATUM);
    String t = (_ntp && _ntp->isSynced()) ? _ntp->getHHMM() : "--:--";
    _sprTime.drawString(t.c_str(), 55, 10);
    _sprTime.setTextDatum(TL_DATUM);
    _sprTime.pushSprite(HDR_TIME_X, 6);
}

// Recherche WiFi (ni connecte, ni portail captif — connexion en cours ou
// reconnexion apres detection zombie) : les 4 barres clignotent en ambre
// au lieu de rester eteintes, pour que l'etat "recherche en cours" soit
// visible d'un coup d'oeil plutot que de se confondre avec "aucun signal".
bool DisplayManager::isWifiSearching() const {
    return _wifi && !_wifi->isConnected() && !_wifi->isCaptivePortal();
}

void DisplayManager::renderSignalSprite() {
    _sprSignal.fillSprite(Theme::SURFACE);

    if (isWifiSearching()) {
        const bool blinkOn = (millis() / 300) % 2 == 0;
        if (blinkOn) {
            for (uint8_t i = 0; i < 4; i++) {
                uint8_t h = 4 + i * 3;
                _sprSignal.fillRect(i * 5, 16 - h, 4, h, Theme::AMBER);
            }
        }
        _sprSignal.pushSprite(HDR_SIGNAL_X, 6);
        return;
    }

    int8_t rssi = (int8_t)WiFi.RSSI();
    uint8_t bars = (rssi > -55) ? 4 : (rssi > -70) ? 3 : (rssi > -80) ? 2 : 1;
    if (WiFi.status() != WL_CONNECTED) bars = 0;
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t col = (i < bars) ? Theme::GREEN : Theme::BORDER;
        uint8_t  h   = 4 + i * 3;
        _sprSignal.fillRect(i * 5, 16 - h, 4, h, col);
    }
    _sprSignal.pushSprite(HDR_SIGNAL_X, 6);
}

void DisplayManager::renderPlanSprite() {
    uint8_t  nbPlan    = min(_nbZones, (uint8_t)4);
    uint16_t spriteH   = _planHdrH + nbPlan * _planZoneH;  // hauteur utile réelle

    _sprPlan.fillSprite(Theme::BG);

    // ── Séparateurs horizontaux — uniquement sur les lignes utilisées ──
    _sprPlan.drawFastHLine(0, _planHdrH - 1, PL_PLAN_W, Theme::BORDER);
    for (uint8_t z = 0; z < nbPlan; z++) {
        uint16_t rowY = _planHdrH + z * _planZoneH;
        _sprPlan.drawFastHLine(0, rowY + _planZoneH - 1, PL_PLAN_W, Theme::BORDER);
    }

    // ── Noms de jours : première colonne = jour courant ──
    // Même convention que l'interface Web, GRID2 et GRID4.
    // Sans synchronisation NTP, repli provisoire sur lundi.
    const char* jours[] = {"Lu","Ma","Me","Je","Ve","Sa","Di"};
    int todayIdx = todayEspIdx();
    int baseIdx  = (todayIdx >= 0) ? todayIdx : 0;
    const bool weatherVisuals = _config && _config->weatherVisualsEnabled();
    if (weatherVisuals) {
        for (uint8_t col = 0; col < 5; ++col) {
            ForecastDay fd = _weather ? _weather->getForecastDay(col) : ForecastDay{};
            if (!fd.valid) continue;
            const int x0 = PL_LABEL_W + col * PL_DAY_W + 1;
            const int y0 = 11;
            const int h = _planHdrH > y0 ? _planHdrH - y0 - 1 : 0;
            if (h <= 0) continue;
            // Fond neutre : seules les pastilles de température portent la couleur.
            _sprPlan.fillRect(x0, y0, PL_DAY_W - 2, h, Theme::SURFACE2);
        }
    }
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
#if AQUALOOK_BOARD_S3
    // Cellule meteo enrichie : reprend ce que montre deja la page Web pour
    // chaque jour - icone, pastilles min/max, vent (fleche + cardinal +
    // km/h) et pluie en mm. Impossible en 42 px de large ; les 65 px du
    // 480x272 et l'en-tete de 50 px le permettent.
    if (disp.showWeatherIcon || disp.showWeatherTemp) {
        // Seuils d'alerte vent lus une fois pour les cinq colonnes.
        const CfgWindAlert wa = _config ? _config->windAlert() : CfgWindAlert{};
        for (uint8_t col = 0; col < 5; col++) {
            const ForecastDay fd = _weather ? _weather->getForecastDay(col)
                                            : ForecastDay{};
            const int16_t x0 = PL_LABEL_W + col * PL_DAY_W + 1;
            const int16_t cx = PL_LABEL_W + col * PL_DAY_W + PL_DAY_W / 2;

            if (!fd.valid) {
                _sprPlan.setTextSize(1);
                _sprPlan.setTextDatum(MC_DATUM);
                _sprPlan.setTextColor(Theme::BORDER, Theme::BG);
                _sprPlan.drawString("--", cx, 26);
                _sprPlan.setTextDatum(TL_DATUM);
                continue;
            }

            // Vent annonce violent : toute la cellule du jour passe sur fond
            // rouge, avant que quoi que ce soit d'autre n'y soit dessine.
            if (weatherWindIsSevere(fd, wa.severeKmh)) {
                _sprPlan.fillRect(x0, 10, PL_DAY_W - 2, _planHdrH - 11,
                                  WIND_SEVERE_CELL_BG);
            }

            // Jauge de pluie verticale supprimee le 29 aout 2026 : peu utile a
            // l'usage, et la bande qu'elle reservait a droite retrecissait
            // toutes les autres lignes. La goutte posee devant la valeur en
            // millimetres dit deja clairement de quoi il s'agit.
            const int16_t contentCx = x0 + (PL_DAY_W - 2) / 2;
            // Couleur de fond effective de la cellule du jour, propagee a
            // tous les textes opaques qui y sont poses.
            const uint16_t cellBg = weatherWindIsSevere(fd, wa.severeKmh)
                                  ? WIND_SEVERE_CELL_BG
                                  : (weatherVisuals ? Theme::SURFACE2 : Theme::BG);

            if (disp.showWeatherIcon) {
                drawWeatherIcon(_sprPlan, contentCx - 7, 12,
                                fd.rainMm, fd.tempMax, true, /*showTemp=*/false);
            }

            if (disp.showWeatherTemp && fd.tempMax > -50.0f) {
                const int16_t pillW = 24;
                const int16_t pillH = 11;
                const int16_t pillY = 26;
                const int16_t minX  = contentCx - pillW - 1;
                const int16_t maxX  = contentCx + 1;
                char tmin[6], tmax[6];
                snprintf(tmin, sizeof(tmin), "%.0f", fd.tempMin);
                snprintf(tmax, sizeof(tmax), "%.0f", fd.tempMax);
                _sprPlan.fillRoundRect(minX, pillY, pillW, pillH, 4,
                                       weatherTempBg565(fd.tempMin));
                _sprPlan.fillRoundRect(maxX, pillY, pillW, pillH, 4,
                                       weatherTempBg565(fd.tempMax));
                _sprPlan.setTextSize(1);
                _sprPlan.setTextDatum(MC_DATUM);
                _sprPlan.setTextColor(Theme::TEXT, weatherTempBg565(fd.tempMin));
                _sprPlan.drawString(tmin, minX + pillW / 2, pillY + pillH / 2);
                _sprPlan.setTextColor(Theme::TEXT, weatherTempBg565(fd.tempMax));
                _sprPlan.drawString(tmax, maxX + pillW / 2, pillY + pillH / 2);
                _sprPlan.setTextDatum(TL_DATUM);
            }

            // Vent puis pluie, sur deux lignes distinctes. Une premiere
            // version n'affichait la pluie QUE s'il n'y en avait pas, et le
            // vent seulement sinon : des qu'une averse etait annoncee - donc
            // presque tous les jours - le vent disparaissait de la colonne.
            // La page Web montre les deux, c'est la reference demandee.
            _sprPlan.setTextSize(1);
            // Chaque ligne est centree sur la zone de contenu : le
            // pictogramme et son texte forment un groupe dont la largeur
            // totale est mesuree avant d'etre posee.
            if (fd.windMaxKmh > 0.0f) {
                _sprPlan.setTextSize(1);
                drawWindLine(_sprPlan, contentCx, 45, fd, cellBg, wa.gustKmh);
            }
            if (fd.rainMm > 0.0f) {
                char rain[10];
                snprintf(rain, sizeof(rain), "%.1fmm", fd.rainMm);
                const int16_t groupW = 9 + (int16_t)strlen(rain) * 6;
                const int16_t groupX = contentCx - groupW / 2;
                drawDropIcon(_sprPlan, groupX, 52, Theme::BLUE);
                _sprPlan.setTextColor(Theme::BLUE, cellBg);
                _sprPlan.setTextDatum(ML_DATUM);
                _sprPlan.drawString(rain, groupX + 9, 55);
                _sprPlan.setTextDatum(TL_DATUM);
            }
        }
    }
#else
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
                const uint16_t tempY = disp.showWeatherIcon ? 28 : 15;
                // Réserver explicitement la bande de droite à la jauge de pluie.
                // Deux pastilles plus compactes évitent que le maxi la recouvre.
                const int pillW = 14;
                const int pillH = 10;
                const int minX  = cx - 17;
                const int maxX  = cx - 2;
                char tmin[5], tmax[5];
                snprintf(tmin, sizeof(tmin), "%.0f", fd.tempMin);
                snprintf(tmax, sizeof(tmax), "%.0f", fd.tempMax);
                _sprPlan.fillRoundRect(minX, tempY, pillW, pillH, 4,
                                       weatherTempBg565(fd.tempMin));
                _sprPlan.fillRoundRect(maxX, tempY, pillW, pillH, 4,
                                       weatherTempBg565(fd.tempMax));
                _sprPlan.setTextSize(1);
                _sprPlan.setTextDatum(MC_DATUM);
                _sprPlan.setTextColor(Theme::TEXT, weatherTempBg565(fd.tempMin));
                _sprPlan.drawString(tmin, minX + pillW / 2, tempY + 5);
                _sprPlan.setTextColor(Theme::TEXT, weatherTempBg565(fd.tempMax));
                _sprPlan.drawString(tmax, maxX + pillW / 2, tempY + 5);
                _sprPlan.setTextDatum(TL_DATUM);
            }
        }
    }

#endif  // AQUALOOK_BOARD_S3

    // ── Repères couleur des zones + barres de slots ──
    for (uint8_t z = 0; z < nbPlan; z++) {
        uint16_t rowY  = _planHdrH + z * _planZoneH;
        uint16_t col_z = zoneColor(z);

        // Bulle de couleur dans la colonne planning.
        // L'identité de la zone est portée ici, pas dans le bouton d'action.
        const int16_t bulletX = PL_LABEL_W / 2;
        const int16_t bulletY = rowY + _planZoneH / 2;
        const int16_t bulletR = max(3, min(5, (int)(_planZoneH / 2 - 2)));

        // Une zone sans sortie physique n'arrosera aucun de ces jours :
        // hachurer la ligne entiere et eteindre sa bulle, plutot que d'afficher
        // un planning qui ne se produira jamais. Meme langage visuel que le
        // blocage par la pluie, deja lisible sur cet ecran.
        const bool zMapped = _relais.relay &&
            RelayTopology::resolveZoneValve(
                _relais.relay->topology(), z, _nbZones).valid;
        if (!zMapped) {
            fillHatchRect(_sprPlan, PL_LABEL_W, rowY + 1,
                          PL_PLAN_W - PL_LABEL_W, _planZoneH - 2,
                          Theme::SURFACE, Theme::MUTED);
        }
        _sprPlan.fillCircle(bulletX, bulletY, bulletR,
                            zMapped ? col_z : Theme::SURFACE2);

        // Barres de slots
        if (!_schedule || !zMapped) continue;
        ZoneSchedule zs = _schedule->getZoneSchedule(z);

        for (int col = 0; col < 7; col++) {
            int espIdx = (baseIdx + col) % 7;
            int x0 = PL_LABEL_W + col * PL_DAY_W + 1;
            if (zs.mode != 0) {
                const uint32_t todayEpochDay =
                    (_ntp && _ntp->isSynced()) ? _ntp->getEpochDay() : 0;
                if (todayEpochDay == 0 ||
                    !intervalDayIsPlanned(zs, todayEpochDay, (uint8_t)col)) {
                    continue;
                }
            }
            DaySchedule& ds = (zs.mode == 0) ? zs.daySlots[espIdx] : zs.intervalSlots;
            const bool rainBlk = rainBlocksDay(zs, ds, (uint8_t)col, _weather);
            if (rainBlk) {
                fillHatchRect(_sprPlan, x0, rowY + 1, PL_DAY_W - 2, _planZoneH - 2,
                              Theme::RAIN_BG_SOFT, Theme::RAIN_STRIPE);
            }
            const uint16_t slotColor = rainBlk ? Theme::AMBER : col_z;
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const TimeSlot& sl = ds.slots[s];
                if (!sl.enabled) continue;
                float frac = (float)(sl.hour * 60 + sl.minute) / 1440.0f;
                int sx = x0 + (int)(frac * (PL_DAY_W - 2));
                int sw = max(2, (int)((float)sl.duration / 1440.0f * (PL_DAY_W - 2)));
                // Rectangle FRANC et non arrondi tant que la barre est etroite.
                // fillRoundRect(w=2, r=1) degenere : son remplissage central
                // vaut fillRect(x+1, y, w-2r=0, h), soit rien du tout, et seuls
                // les quatre arcs de coin subsistent - la barre s'affichait donc
                // comme un crochet "[". Un arrosage de 10 min sur une colonne de
                // 65 px fait 0,4 px, donc ramene au minimum de 2 px : le cas
                // degenere est la regle, pas l'exception (constate sur materiel
                // reel le 29 aout 2026).
                drawSlotBar(_sprPlan, sx, rowY + 2, sw, _planZoneH - 4, slotColor);
            }
        }
    }

    // Pousser uniquement la hauteur réellement utilisée.
    // Un pushSprite() complet ferait 90 px de haut et recouvrirait le haut
    // des boutons lorsque leur position dynamique commence avant y=118.
    _tft.pushImage(0, PL_PLAN_Y, PL_PLAN_W, spriteH,
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

    // Mode 5-8 zones : densifier les huit lignes de planning afin de
    // réserver une vraie zone météo en haut (jour, icône, mini, maxi, pluie).
    // Geometrie partagee avec DisplayPlanningDecor (voir DisplayManager.h) :
    // les hachures des jours non arroses doivent se superposer exactement
    // aux cellules tracees ici.
    const uint16_t HDR_H      = _g2PlanHdrH;
    const uint16_t LABEL_W_G2 = G2_PLAN_LABEL_W;
    const uint16_t zoneH      = _g2PlanZoneH;
    const uint8_t  nbPlan     = min(_nbZones, (uint8_t)8);
    const uint16_t COL_W      = _g2PlanColW;
    (void)planW;
    int todayIdx = todayEspIdx();
    int baseIdx = (todayIdx >= 0) ? todayIdx : 0;
    const char* jours[] = {"Lu","Ma","Me","Je","Ve","Sa","Di"};

    for (uint8_t c = 0; c < 2; c++) {
        int espIdx = (baseIdx + c) % 7;
        uint16_t cx = LABEL_W_G2 + c * COL_W;
        uint16_t col_c = (c == 0 && todayIdx >= 0) ? Theme::CYAN : Theme::MUTED;
        _tft.setFreeFont(nullptr);
        _tft.setTextSize(1);
        _tft.setTextColor(col_c, Theme::BG);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(jours[espIdx], cx + COL_W / 2, destY + 1);

        ForecastDay fd = _weather ? _weather->getForecastDay(c) : ForecastDay{};
        if (fd.valid && fd.tempMax > -50.0f) {
            const bool visuals = _config && _config->weatherVisualsEnabled();
            const uint16_t boxY = destY + 10;
            const uint16_t boxH = HDR_H - 11;

            if (visuals) {
                _tft.fillRect(cx + 1, boxY, COL_W - 2, boxH, Theme::SURFACE2);

                const uint16_t iconX = cx + 3;
                const uint16_t iconY = destY + 12;
                if (fd.rainMm > 1.0f) {
                    _tft.fillRoundRect(iconX, iconY + 1, 9, 4, 2, Theme::MUTED);
                    _tft.drawFastVLine(iconX + 2, iconY + 6, 2, Theme::BLUE);
                    _tft.drawFastVLine(iconX + 6, iconY + 6, 2, Theme::BLUE);
                } else {
                    _tft.fillCircle(iconX + 4, iconY + 4, 3, Theme::AMBER);
                }


                char tmin[5], tmax[5];
                snprintf(tmin, sizeof(tmin), "%.0f", fd.tempMin);
                snprintf(tmax, sizeof(tmax), "%.0f", fd.tempMax);
#if AQUALOOK_BOARD_S3
                // Meme ordre vertical que la page Web : icone, puis les deux
                // temperatures cote a cote (mini a gauche), puis le vent,
                // puis la pluie. Le vent etait auparavant colle a l'icone,
                // ce qui donnait une lecture differente de celle du Web pour
                // la meme donnee.
                const uint16_t pillH = 11;
                const uint16_t pillW = (COL_W - 12) / 2;
                const uint16_t pillY = destY + 24;
                const uint16_t minX  = cx + 2;
                const uint16_t maxX  = minX + pillW + 2;
                _tft.fillRoundRect(minX, pillY, pillW, pillH, 4, weatherTempBg565(fd.tempMin));
                _tft.fillRoundRect(maxX, pillY, pillW, pillH, 4, weatherTempBg565(fd.tempMax));
                _tft.setTextSize(1);
                _tft.setTextDatum(MC_DATUM);
                _tft.setTextColor(Theme::TEXT, weatherTempBg565(fd.tempMin));
                _tft.drawString(tmin, minX + pillW / 2, pillY + pillH / 2);
                _tft.setTextColor(Theme::TEXT, weatherTempBg565(fd.tempMax));
                _tft.drawString(tmax, maxX + pillW / 2, pillY + pillH / 2);
                _tft.setTextDatum(TL_DATUM);

                const int16_t cellCx = cx + COL_W / 2;
                const CfgWindAlert wa = _config ? _config->windAlert() : CfgWindAlert{};
                const uint16_t cellBg = weatherWindIsSevere(fd, wa.severeKmh)
                                      ? WIND_SEVERE_CELL_BG : Theme::SURFACE2;
                if (fd.windMaxKmh > 0.0f) {
                    _tft.setTextSize(1);
                    drawWindLine(_tft, cellCx, destY + 42, fd, cellBg, wa.gustKmh);
                }
                if (fd.rainMm > 0.0f) {
                    char rbuf[10];
                    snprintf(rbuf, sizeof(rbuf), "%.1fmm", fd.rainMm);
                    const int16_t groupW = 9 + (int16_t)strlen(rbuf) * 6;
                    const int16_t groupX = cellCx - groupW / 2;
                    drawDropIcon(_tft, groupX, destY + 49, Theme::BLUE);
                    _tft.setTextColor(Theme::BLUE, cellBg);
                    _tft.setTextDatum(ML_DATUM);
                    _tft.drawString(rbuf, groupX + 9, destY + 52);
                    _tft.setTextDatum(TL_DATUM);
                }
#else
                const uint16_t pillX = cx + 2;
                const uint16_t pillW = COL_W - 9;
                const uint16_t pillH = 8;
                const uint16_t maxY = destY + 24;
                const uint16_t minY = destY + 33;
                _tft.fillRoundRect(pillX, maxY, pillW, pillH, 3, weatherTempBg565(fd.tempMax));
                _tft.fillRoundRect(pillX, minY, pillW, pillH, 3, weatherTempBg565(fd.tempMin));
                _tft.setTextDatum(MC_DATUM);
                _tft.setTextColor(Theme::TEXT, weatherTempBg565(fd.tempMax));
                _tft.drawString(tmax, pillX + pillW / 2, maxY + pillH / 2);
                _tft.setTextColor(Theme::TEXT, weatherTempBg565(fd.tempMin));
                _tft.drawString(tmin, pillX + pillW / 2, minY + pillH / 2);
#endif
            } else {
                const uint16_t wx = cx + 3;
                const uint16_t wy = destY + 13;
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
                _tft.drawString(wbuf, cx + COL_W - 2, destY + 13);
            }
        }
        _tft.setTextDatum(TL_DATUM);
        _tft.drawFastVLine(cx, destY, sprH, Theme::BORDER);
    }
    _tft.drawFastHLine(0, destY + HDR_H - 1, planW, Theme::BORDER);

    for (uint8_t z = 0; z < nbPlan; z++) {
        uint16_t rowY = destY + HDR_H + z * zoneH;
        uint16_t col_z = zoneColor(z);
        _tft.drawFastHLine(0, rowY + zoneH - 1, planW, Theme::BORDER);

        const int16_t bulletX = LABEL_W_G2 / 2;
        const int16_t bulletY = rowY + zoneH / 2;
        const int16_t bulletR = max(2, min(4, (int)(zoneH / 2 - 1)));
        _tft.fillCircle(bulletX, bulletY, bulletR, col_z);

        if (!_schedule) continue;
        ZoneSchedule zs = _schedule->getZoneSchedule(z);
        for (uint8_t c = 0; c < 2; c++) {
            int espIdx = (baseIdx + c) % 7;
            uint16_t cx = LABEL_W_G2 + c * COL_W;
            if (zs.mode != 0) {
                const uint32_t todayEpochDay =
                    (_ntp && _ntp->isSynced()) ? _ntp->getEpochDay() : 0;
                if (todayEpochDay == 0 ||
                    !intervalDayIsPlanned(zs, todayEpochDay, c)) {
                    continue;
                }
            }
            DaySchedule& ds = (zs.mode == 0) ? zs.daySlots[espIdx] : zs.intervalSlots;
            const bool rainBlk = rainBlocksDay(zs, ds, (uint8_t)c, _weather);
            if (rainBlk) {
                fillHatchRect(_tft, cx + 1, rowY + 1, COL_W - 2, max(2, (int)zoneH - 2),
                              Theme::RAIN_BG_SOFT, Theme::RAIN_STRIPE);
            }
            const uint16_t slotColor = rainBlk ? Theme::AMBER : col_z;
            for (uint8_t sl = 0; sl < MAX_SLOTS; sl++) {
                const TimeSlot& slot = ds.slots[sl];
                if (!slot.enabled) continue;
                float frac = (float)(slot.hour * 60 + slot.minute) / 1440.0f;
                int sx = cx + 1 + (int)(frac * (COL_W - 2));
                int sw = max(2, (int)((float)slot.duration / 1440.0f * (COL_W - 2)));
                drawSlotBar(_tft, sx, rowY + 2, sw, max(2, (int)zoneH - 4), slotColor);
            }
        }
    }
}

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
        uint16_t col_z = zoneColor(z);
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
            if (zs.mode != 0) {
                const uint32_t todayEpochDay =
                    (_ntp && _ntp->isSynced()) ? _ntp->getEpochDay() : 0;
                if (todayEpochDay == 0 ||
                    !intervalDayIsPlanned(zs, todayEpochDay, col)) {
                    continue;
                }
            }
            DaySchedule& ds = (zs.mode == 0) ? zs.daySlots[espIdx] : zs.intervalSlots;
            const bool rainBlk = rainBlocksDay(zs, ds, (uint8_t)col, _weather);
            if (rainBlk) {
                uint16_t cellH = max((uint16_t)2, (uint16_t)(zoneH - 2));
                fillHatchRect(_tft, x0, rowY + 1, DAY_W - 2, cellH,
                              Theme::RAIN_BG_SOFT, Theme::RAIN_STRIPE);
            }
            const uint16_t slotColor = rainBlk ? Theme::AMBER : col_z;
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const TimeSlot& sl = ds.slots[s];
                if (!sl.enabled) continue;
                float frac = (float)(sl.hour * 60 + sl.minute) / 1440.0f;
                int sx = x0 + (int)(frac * (DAY_W - 2));
                int sw = max(2, (int)((float)sl.duration / 1440.0f * (DAY_W - 2)));
                uint16_t barH = max((uint16_t)2, (uint16_t)(zoneH - 4));
                drawSlotBar(_tft, sx, rowY + 2, sw, barH, slotColor);
            }
        }
    }
}

// Libelle du prochain arrosage d'une zone : "auj. 06:30", "demain 07:15",
// "jeudi 06:00", ou "--:--" si rien n'est planifie ou si l'heure est
// inconnue.
//
// Extrait de renderBtnSprite() le 29 aout 2026 : la mise en page 480x272
// en a besoin avec une disposition differente, et recopier ce calcul aurait
// laisse les deux versions diverger en silence.
String DisplayManager::nextSlotLabel(uint8_t zone) {
    // Prochain slot — toujours chercher le créneau futur le plus proche.
    // Les slots peuvent être enregistrés dans un ordre quelconque : ne jamais
    // considérer que slots[0] est chronologiquement le premier.
    String next = "--:--";
    if (_schedule && _ntp && _ntp->isSynced()) {
        ZoneSchedule zs   = _schedule->getZoneSchedule(zone);
        const int todayEsp = todayEspIdx();
        const uint32_t epochNow = _ntp->getEpochDay();
        const int nowMin = _ntp->getHour() * 60 + _ntp->getMinute();
        const char* JOURS[] = {"lundi","mardi","mercredi","jeudi","vendredi","samedi","dimanche"};

        auto findEarliestSlot = [](const DaySchedule& ds, int minExclusive,
                                   uint8_t& outHour, uint8_t& outMinute) -> bool {
            int bestMin = 24 * 60;
            bool found = false;
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const TimeSlot& slot = ds.slots[s];
                if (!slot.enabled) continue;
                const int slotMin = slot.hour * 60 + slot.minute;
                if (slotMin <= minExclusive || slotMin >= bestMin) continue;
                bestMin = slotMin;
                outHour = slot.hour;
                outMinute = slot.minute;
                found = true;
            }
            return found;
        };

        if (zs.mode == 0) {
            // Jours fixes : aujourd'hui, ignorer les créneaux passés ; pour les
            // jours suivants, choisir le premier créneau chronologique du jour.
            for (int d = 0; d < NB_DAYS; d++) {
                const int dayIdx = (todayEsp + d) % NB_DAYS;
                uint8_t hour = 0, minute = 0;
                const int minExclusive = (d == 0) ? nowMin : -1;
                if (!findEarliestSlot(zs.daySlots[dayIdx], minExclusive, hour, minute)) continue;

                char buf2[20];
                if (d == 0)      snprintf(buf2, sizeof(buf2), "auj. %02d:%02d", hour, minute);
                else if (d == 1) snprintf(buf2, sizeof(buf2), "demain %02d:%02d", hour, minute);
                else             snprintf(buf2, sizeof(buf2), "%s %02d:%02d", JOURS[dayIdx], hour, minute);
                next = buf2;
                break;
            }
        } else {
            // Intervalle : déterminer le prochain jour autorisé, puis chercher le
            // premier créneau encore futur. Si tous les créneaux du jour sont
            // passés, avancer d'un intervalle complet.
            const uint32_t interval = zs.intervalDays > 0 ? zs.intervalDays : 1;
            uint32_t nextDay = zs.intervalAnchorDay;
            if (nextDay == 0) {
                nextDay = epochNow;
            } else {
                while (nextDay < epochNow) nextDay += interval;
            }

            uint8_t hour = 0, minute = 0;
            int minExclusive = (nextDay == epochNow) ? nowMin : -1;
            if (!findEarliestSlot(zs.intervalSlots, minExclusive, hour, minute) &&
                nextDay == epochNow) {
                nextDay += interval;
                minExclusive = -1;
            }

            if (findEarliestSlot(zs.intervalSlots, minExclusive, hour, minute)) {
                const int32_t daysAhead = (int32_t)(nextDay - epochNow);
                const uint8_t nextEspIdx = (uint8_t)((nextDay + 3) % 7);
                char buf2[20];
                if (daysAhead == 0)      snprintf(buf2, sizeof(buf2), "auj. %02d:%02d", hour, minute);
                else if (daysAhead == 1) snprintf(buf2, sizeof(buf2), "demain %02d:%02d", hour, minute);
                else                     snprintf(buf2, sizeof(buf2), "%s %02d:%02d", JOURS[nextEspIdx], hour, minute);
                next = buf2;
            }
        }
    }
    return next;
}

void DisplayManager::renderBtnSprite(uint8_t zone, uint16_t pushY) {
    bool    active   = _relais && _relais->getState(zone);
    // Une zone sans voie physique ne pourra jamais arroser, et le module ne
    // peut pas le deviner : les cartes relais ne presentent pas leur
    // configuration -- le circuit est identique qu il y ait 1, 2, 4 ou 8
    // relais soudes. L information vient donc du cablage declare, et elle
    // doit se voir sur l ecran plutot que de se deduire d une zone qui refuse
    // de demarrer sans explication.
    const bool mapped = _relais.relay &&
        RelayTopology::resolveZoneValve(
            _relais.relay->topology(), zone, _nbZones).valid;
    uint16_t bg      = active ? Theme::ACTIVE_BG : Theme::SURFACE;
    uint16_t border  = active ? Theme::ACTIVE_BORDER : Theme::BORDER;
    uint16_t zColor  = zoneColor(zone);
    // Conserver une marge basse réelle : une carte dont le bord inférieur
    // coïncide avec SCREEN_H est physiquement tronquée et paraît carrée.
    static constexpr uint16_t BTN_BOTTOM_MARGIN = 6;
    const uint16_t availableH = (pushY + BTN_BOTTOM_MARGIN < SCREEN_H)
        ? (uint16_t)(SCREEN_H - pushY - BTN_BOTTOM_MARGIN)
        : 0;
    const uint16_t visibleH = min((uint16_t)PL_BTN_H, availableH);
    if (visibleH == 0) return;

    // Le sprite reste alloué à PL_BTN_H, mais la carte est dessinée selon la
    // hauteur réellement visible afin de conserver les coins arrondis en bas.
    // Le sprite alloué est plus haut que la carte réellement visible.
    // Le remplir avec le fond d'écran permet de pousser uniquement les lignes
    // utiles sans dépendre d'une couleur transparente, dont la comparaison peut
    // varier selon l'ordre des octets du sprite TFT_eSPI.
    _sprBtn0.fillSprite(Theme::BG);
    drawCardBg(_sprBtn0, 0, 0, PL_BTN_W, visibleH, Theme::R_LG, bg, border, false);
    // Barre d accent eteinte quand la zone n a pas de sortie : la carte
    // recule visuellement au lieu de se confondre avec une zone au repos.
    drawAccentBar(_sprBtn0, 0, 0, visibleH, Theme::R_LG,
                  mapped ? zColor : Theme::SURFACE2);

#if AQUALOOK_BOARD_S3
    // ── Carte 228x156 (480x272) ────────────────────────────────
    // Barre d'accent a gauche + relief leger, mise en page choisie avec
    // l'utilisateur le 29 aout 2026. La version 154x120 de la carte
    // historique tenait sur trois lignes serrees ; ici la hauteur permet
    // une vraie hierarchie : identite en haut, information utile au
    // centre, action rappelee en bas.
    constexpr int16_t PAD    = 14;   // marge interne, apres la barre d'accent
    const int16_t     innerW = PL_BTN_W - 2 * PAD;

    _sprBtn0.setFreeFont(nullptr);

    // Entete : nom de zone en gros, pastille d'etat calee a droite.
    char fallbackName[16];
    const char* zoneName =
        zoneButtonName(_config, zone, fallbackName, sizeof(fallbackName));
    _sprBtn0.setTextSize(2);
    _sprBtn0.setTextColor(Theme::TEXT, bg);
    _sprBtn0.setTextDatum(TL_DATUM);
    _sprBtn0.drawString(zoneName, PAD, 10);

    // La pastille porte la couleur de la zone quand elle arrose : c'est le
    // seul element vif de la carte au repos, donc l'etat se lit d'un coup
    // d'oeil sans avoir a dechiffrer le texte.
    {
        constexpr int16_t pillW = 46;
        constexpr int16_t pillH = 20;
        const int16_t pillX = PL_BTN_W - PAD - pillW;
        _sprBtn0.fillRoundRect(pillX, 9, pillW, pillH, pillH / 2,
                               active ? zColor : Theme::SURFACE2);
        _sprBtn0.setTextSize(1);
        _sprBtn0.setTextColor(active ? Theme::BG : Theme::MUTED,
                              active ? zColor : Theme::SURFACE2);
        _sprBtn0.setTextDatum(MC_DATUM);
        _sprBtn0.drawString(!mapped ? "N/C" : (active ? "ON" : "OFF"),
                            pillX + pillW / 2, 9 + pillH / 2);
        _sprBtn0.setTextDatum(TL_DATUM);
    }

    _sprBtn0.drawFastHLine(PAD, 38, innerW, Theme::BORDER);

    _sprBtn0.setTextSize(1);

    if (active) {
        const uint32_t elapsed = _schedule ? _schedule->getElapsedMs(zone) : 0;
        const uint32_t remain  = _schedule ? _schedule->getRemainingMs(zone) : 0;
        const uint32_t total   = elapsed + remain;

        _sprBtn0.setTextColor(Theme::MUTED, bg);
        _sprBtn0.drawString("Temps restant", PAD, 48);

        char rbuf[10];
        snprintf(rbuf, sizeof(rbuf), "%02lu:%02lu",
                 remain / 60000UL, (remain % 60000UL) / 1000UL);
        _sprBtn0.setTextSize(3);
        _sprBtn0.setTextColor(Theme::TEXT, bg);
        _sprBtn0.drawString(rbuf, PAD, 62);
        _sprBtn0.setTextSize(1);

        const uint8_t pct = (total > 0) ? (uint8_t)((elapsed * 100UL) / total) : 0;
        const int16_t barW = (int16_t)((int32_t)innerW * pct / 100);
        _sprBtn0.fillRoundRect(PAD, 98, innerW, 8, 4, Theme::SURFACE2);
        if (barW > 0) _sprBtn0.fillRoundRect(PAD, 98, barW, 8, 4, zColor);

        char ebuf[20];
        snprintf(ebuf, sizeof(ebuf), "ecoule %02lu:%02lu",
                 elapsed / 60000UL, (elapsed % 60000UL) / 1000UL);
        _sprBtn0.setTextColor(Theme::MUTED, bg);
        _sprBtn0.drawString(ebuf, PAD, 112);

    } else if (!mapped) {
        // Afficher le prochain creneau serait mensonger : il ne tombera
        // jamais, faute de sortie a piloter.
        _sprBtn0.setTextColor(Theme::AMBER, bg);
        _sprBtn0.drawString("Aucune sortie affectee", PAD, 48);
        _sprBtn0.setTextColor(Theme::MUTED, bg);
        _sprBtn0.drawString("a definir dans le cablage", PAD, 66);

    } else {
        const String next = nextSlotLabel(zone);

        _sprBtn0.setTextColor(Theme::MUTED, bg);
        _sprBtn0.drawString("Prochain arrosage", PAD, 48);

        // Size 2 tant que le libelle tient dans la largeur utile (12 px par
        // caractere a cette taille) ; les noms de jour longs repassent en
        // size 1 plutot que d'etre tronques.
        const bool wide = (next.length() * 12) <= (unsigned)innerW;
        _sprBtn0.setTextSize(wide ? 2 : 1);
        _sprBtn0.setTextColor(Theme::TEXT, bg);
        _sprBtn0.drawString(next.c_str(), PAD, wide ? 64 : 68);
        _sprBtn0.setTextSize(1);

        if (_schedule) {
            const ZoneSchedule zs = _schedule->getZoneSchedule(zone);
            char modeBuf[28];
            if (zs.mode == 0) snprintf(modeBuf, sizeof(modeBuf), "Jours fixes");
            else              snprintf(modeBuf, sizeof(modeBuf),
                                       "Intervalle / %uj", (unsigned)zs.intervalDays);
            _sprBtn0.setTextColor(Theme::MUTED, bg);
            _sprBtn0.drawString(modeBuf, PAD, 96);
        }
    }

    // Rappel de l'action, cale sur le bas reel de la carte.
    _sprBtn0.setTextColor(Theme::MUTED, bg);
    _sprBtn0.setTextDatum(BC_DATUM);
    _sprBtn0.drawString(active ? "Appuyer pour arreter" : "Appuyer pour arroser",
                        PL_BTN_W / 2,
                        (visibleH > 24) ? (int16_t)(visibleH - 12) : (int16_t)visibleH);
    _sprBtn0.setTextDatum(TL_DATUM);
#else
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
        _sprBtn0.drawString(ebuf, 6, 64);

        // Hint arrêt — position relative à la hauteur réellement visible.
        _sprBtn0.setTextColor(Theme::ON_ACTIVE_MUTED, bg);  // gris clair lisible sur rouge
        _sprBtn0.setTextDatum(TC_DATUM);
        const uint16_t hintY = (visibleH > 16) ? min((uint16_t)84, (uint16_t)(visibleH - 12)) : 4;
        _sprBtn0.drawString("Appuyer pour arreter", PL_BTN_W / 2, hintY);
        _sprBtn0.setTextDatum(TL_DATUM);

    } else {
        const String next = nextSlotLabel(zone);
        _sprBtn0.setTextColor(Theme::MUTED, bg);
        _sprBtn0.drawString("Prochain :", 6, 20);
        // Size 2 si <= 11 chars (ex "dem 06:30"), sinon size 1 pour les noms longs
        _sprBtn0.setTextSize(next.length() <= 11 ? 2 : 1);
        _sprBtn0.setTextColor(Theme::TEXT, bg);
        _sprBtn0.drawString(next.c_str(), 6, next.length() <= 11 ? 32 : 38);
        _sprBtn0.setTextSize(1);

        // Information propre à la zone : mode de planification uniquement.
        // La météo reste dans le bandeau planning et n'est plus dupliquée ici.
        if (_schedule) {
            ZoneSchedule zs = _schedule->getZoneSchedule(zone);
            char modeBuf[28];
            if (zs.mode == 0)
                snprintf(modeBuf, sizeof(modeBuf), "Jours fixes");
            else
                snprintf(modeBuf, sizeof(modeBuf), "Intervalle / %uj", (unsigned)zs.intervalDays);

            _sprBtn0.setTextColor(Theme::MUTED, bg);
            _sprBtn0.drawString(modeBuf, 6, 58);
        }

        _sprBtn0.setTextColor(Theme::MUTED, bg);
        _sprBtn0.setTextDatum(TC_DATUM);
        const uint16_t hintY = (visibleH > 16) ? min((uint16_t)76, (uint16_t)(visibleH - 12)) : 4;
        _sprBtn0.drawString("Appuyer pour arroser", PL_BTN_W / 2, hintY);
        _sprBtn0.setTextDatum(TL_DATUM);
    }

#endif  // AQUALOOK_BOARD_S3

    const uint16_t pushX = (zone == 0) ? PL_BTN_Z1_X : PL_BTN_Z2_X;

#if AQUALOOK_BOARD_S3
    // Ne nettoyer QUE la bande situee sous la carte, pas la carte elle-meme :
    // le sprite pousse juste apres la recouvre integralement, donc la
    // repeindre d'abord en couleur de fond ne sert a rien et produit un
    // eclair visible a chaque rafraichissement. Le minuteur se redessinant
    // chaque seconde pendant un arrosage, cela donnait un scintillement
    // permanent de la carte (constate sur materiel reel le 29 aout 2026).
    if (SCREEN_H > pushY + visibleH) {
        _tft.fillRect(pushX, pushY + visibleH, PL_BTN_W,
                      SCREEN_H - pushY - visibleH, Theme::BG);
    }
#else
    // Nettoyer toute la colonne, puis transférer uniquement la hauteur utile.
    // Ne jamais pousser PL_BTN_H complet ici : la partie basse inutilisée du
    // sprite était la source du rectangle coloré visible sous les boutons.
    _tft.fillRect(pushX, pushY, PL_BTN_W, SCREEN_H - pushY, Theme::BG);
#endif
    _tft.pushImage(pushX, pushY, PL_BTN_W, visibleH,
                   static_cast<uint16_t*>(_sprBtn0.getPointer()));
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
    // Interception AVANT la dispatch de mode : c'est le seul point par lequel
    // passent LIST, GRID2 et GRID4. Marquer les tuiles une par une ne suffit
    // pas ici -- au-dela de quelques zones l'ecran n'en montre pas la moitie,
    // et un module non cable doit dire son etat d'un seul coup d'oeil.
    if (homeUnwired()) { drawHomeFull_unwired(); return; }
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
// ─────────────────────────────────────────────
//  HOME NON CABLE — aucune sortie declaree
//
//  Un module neuf ne devine plus son cablage (cf. RelaisManager). Il ne peut
//  donc pas arroser, et afficher huit tuiles hachurees le dirait mal : on
//  affiche UN message, celui de l'action a faire, et rien d'autre.
// ─────────────────────────────────────────────
void DisplayManager::drawHomeFull_unwired() {
    _tft.fillScreen(Theme::BG);

    // Meme bandeau que les autres modes : le [≡] est la seule porte vers le
    // menu, le retirer enfermerait l'utilisateur ici.
    _tft.fillRect(0, 0, SCREEN_W, G2_HDR_H, Theme::SURFACE);
    _tft.setFreeFont(nullptr);
    drawMenuIcon(_tft, 6, 7, Theme::TEXT);
    _tft.setTextSize(1);
    _tft.setTextColor(Theme::TEXT, Theme::SURFACE);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("AquaLook", 24, 8);
    renderTimeSprite();
    renderSignalSprite();

    const uint16_t cx = SCREEN_W / 2;
    uint16_t y = G2_CONTENT_Y + (SCREEN_H - G2_CONTENT_Y) / 2 - 52;

    _tft.setTextDatum(TC_DATUM);
    _tft.setTextSize(2);
    _tft.setTextColor(Theme::AMBER, Theme::BG);
    _tft.drawString("Module non cable", cx, y);
    y += 30;

    _tft.setTextSize(1);
    _tft.setTextColor(Theme::TEXT, Theme::BG);
    _tft.drawString("Aucune sortie n'est affectee :", cx, y);   y += 14;
    _tft.drawString("le module ne peut pas arroser.", cx, y);   y += 24;

    _tft.setTextColor(Theme::TEXT2, Theme::BG);
    _tft.drawString("Interface web  >  Zones", cx, y);          y += 14;
    _tft.drawString(">  Cablage relais", cx, y);                y += 24;

    _tft.setTextColor(Theme::BLUE, Theme::BG);
    const String ip = WiFi.localIP().toString();
    _tft.drawString(ip.length() > 6 ? ip.c_str() : "en attente du reseau",
                    cx, y);

    _tft.setTextDatum(TL_DATUM);
}

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
        // L'ecran ne montre que les 4 premieres zones : un marquage par zone
        // resterait invisible pour les suivantes. Un compteur d'ensemble, lui,
        // se voit quel que soit le defilement.
        uint8_t sansSortie = 0;
        for (uint8_t z = 0; z < _nbZones && _relais.relay; z++) {
            if (!RelayTopology::resolveZoneValve(
                    _relais.relay->topology(), z, _nbZones).valid) {
                sansSortie++;
            }
        }
        if (sansSortie > 0) {
            char warn[24];
            snprintf(warn, sizeof(warn), "%u sans sortie", (unsigned)sansSortie);
            _tft.setTextColor(Theme::AMBER, Theme::SURFACE2);
            _tft.setTextDatum(ML_DATUM);
            _tft.drawString(warn, 6, 230);
            _tft.setTextDatum(MC_DATUM);
        }
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
    // L'ecran "non cable" n'a rien de dynamique a rafraichir, et le laisser
    // passer ici repeindrait des elements de la grille par-dessus lui.
    if (homeUnwired()) { renderTimeSprite(); renderSignalSprite(); return; }
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

    // Signal — redessine aussi en continu pendant la recherche WiFi pour
    // faire vivre le clignotement (rssi reste fige a 0 tant que non connecte,
    // ce qui ne declencherait sinon jamais de redraw).
    int8_t rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : 0;
    if (rssi != _hc.rssi || isWifiSearching()) { _hc.rssi = rssi; renderSignalSprite(); }

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
#if AQUALOOK_BOARD_S3
        if (_nbZones <= 2) {
            // Redessin complet de la carte plutot qu'un rafraichissement
            // partiel place a la main. Le bloc ci-dessous (branche carte
            // historique) ecrit directement sur l'ecran a des ordonnees
            // figees - btnY+16, +17, +30, +56, +64 - qui correspondent a la
            // carte 154x120. Sur la carte 228x156 elles tombaient en plein
            // milieu du nouveau contenu et SUPERPOSAIENT l'ancien affichage
            // au nouveau (constate sur materiel reel le 29 aout 2026 :
            // "~ ON" et "+00:12" par-dessus "Temps restant" et "ecoule").
            //
            // Passer par renderBtnSprite() garde une seule source de verite
            // pour la mise en page de la carte. Le cout est acceptable : le
            // sprite est envoye en un seul transfert, donc sans
            // scintillement, contrairement a un fond redessine a l'ecran.
            renderBtnSprite(z, btnY);
        }
#else
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
            _tft.drawString(ebuf, x + 6, btnY + 64);
        }
#endif  // AQUALOOK_BOARD_S3
        if (_nbZones > 2 && _nbZones <= 4) {
            const uint16_t x = z * (PL_CBTN_W + PL_CBTN_GAP);
            const uint16_t bg = Theme::ACTIVE_BG;
            const uint32_t elapsed = _schedule ? _schedule->getElapsedMs(z) : 0;
            const uint32_t total = elapsed + remainMs;
            const uint8_t pct = total ? (uint8_t)((elapsed * 100UL) / total) : 0;
            const uint16_t barW = (uint16_t)((PL_CBTN_W - 8) * pct / 100);

            _tft.fillRect(x + 5, btnY + 18, PL_CBTN_W - 10, 36, bg);
            _tft.setFreeFont(nullptr);
            _tft.setTextDatum(TC_DATUM);
            _tft.setTextSize(2);
            _tft.setTextColor(Theme::TEXT, bg);
            char rbuf[10];
            snprintf(rbuf, sizeof(rbuf), "%02lu:%02lu", remainMs / 60000UL, (remainMs % 60000UL) / 1000UL);
            _tft.drawString(rbuf, x + PL_CBTN_W / 2, btnY + 22);
            _tft.fillRoundRect(x + 4, btnY + 47, PL_CBTN_W - 8, 5, 2, Theme::BORDER);
            if (barW) _tft.fillRoundRect(x + 4, btnY + 47, barW, 5, 2, Theme::AMBER);
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

    // Couleur propre a chaque zone (schema NVS 4). La palette ci-dessus reste
    // le recours quand une zone n'a jamais ete configuree.
    for (uint8_t z = 0; z < MAX_ZONES; ++z) {
        const char* hex = _config->zoneColor(z);
        _zoneRgb[z] = (hex && hex[0] == '#') ? hexToRgb565(hex)
                                             : Theme::ZONE_COLORS[z % 4];
    }

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
    updateGrid2Geometry();   // depend de _g2Gpad, recalculer apres chargement

#if AQUALOOK_BOARD_S3
    // 480x272 : la cellule meteo enrichie (icone, pastilles min/max, vent,
    // pluie et jauge) a besoin d'une hauteur fixe de 50 px, quel que soit le
    // detail demande - les elements sont empiles a des ordonnees figees, les
    // masquer laisse simplement du vide plutot que de comprimer la colonne.
    // L'espace ne manque pas ici, contrairement au 320x240 ou chaque option
    // devait etre compensee ligne par ligne.
    _planHdrH  = (d.showWeatherIcon || d.showWeatherTemp) ? PL_HDR_H : 16;
    _planZoneH = PL_ZONE_H;
#else
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
#endif  // AQUALOOK_BOARD_S3
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
    uint16_t zColor = zoneColor(zone);
    // C'est CETTE fonction qui dessine les boutons au-dela de 4 zones
    // (HomeMode::GRID2). Une zone sans voie physique ne pourra jamais
    // arroser : meme signal que sur le Web, une trame diagonale, qui se lit
    // comme "indisponible" la ou un simple gris passerait pour un choix de
    // theme.
    const bool mapped = _relais.relay &&
        RelayTopology::resolveZoneValve(
            _relais.relay->topology(), zone, _nbZones).valid;

    drawCardBg(_tft, x, y, w, h, Theme::R_MD, bg, border, false);
    if (!mapped) {
        fillHatchRect(_tft, x + 2, y + 2, w - 4, h - 4, bg, Theme::MUTED);
    }
    drawAccentBar(_tft, x, y, h, Theme::R_MD, mapped ? zColor : Theme::SURFACE2);
    _tft.setFreeFont(nullptr);
    _tft.setTextSize(1);

    char fallbackName[16];
    const char* zoneName = zoneButtonName(_config, zone, fallbackName, sizeof(fallbackName));

#if AQUALOOK_BOARD_S3
    // Meme vocabulaire visuel que les cartes 1-4 zones validees le 29 aout
    // 2026 : nom a gauche, pastille d'etat a droite portant la couleur de la
    // zone quand elle arrose. Les cartes de la grille font ici 161 px de
    // large (contre 126) et de 60 a 86 px de haut : le contenu d'origine,
    // reduit au seul mot "Appuyer", y paraissait vide.
    {
        constexpr int16_t pad = 10;
        _tft.setTextColor(Theme::TEXT, bg);
        _tft.setTextDatum(TL_DATUM);
        _tft.drawString(zoneName, x + pad, y + 6);

        const int16_t pillW = 34;
        const int16_t pillH = 14;
        const int16_t pillX = x + w - pad - pillW;
        _tft.fillRoundRect(pillX, y + 5, pillW, pillH, pillH / 2,
                           active ? zColor : Theme::SURFACE2);
        _tft.setTextColor(active ? Theme::BG : Theme::MUTED,
                          active ? zColor : Theme::SURFACE2);
        _tft.setTextDatum(MC_DATUM);
        _tft.drawString(!mapped ? "N/C" : (active ? "ON" : "OFF"),
                        pillX + pillW / 2, y + 5 + pillH / 2);

        // Corps 2 seulement quand la hauteur le permet : a 60 px (8 zones)
        // il deborderait sur la pastille.
        const bool roomy = (h >= 70);
        _tft.setTextDatum(TL_DATUM);
        if (active && _schedule) {
            const uint32_t rem = _schedule->getRemainingMs(zone);
            char buf[12];
            snprintf(buf, sizeof(buf), "%02lu:%02lu",
                     rem / 60000UL, (rem % 60000UL) / 1000UL);
            _tft.setTextSize(roomy ? 2 : 1);
            _tft.setTextColor(Theme::TEXT, bg);
            _tft.drawString(buf, x + pad, y + 26);
            _tft.setTextSize(1);

            const uint32_t elapsed = _schedule->getElapsedMs(zone);
            const uint32_t total   = elapsed + rem;
            const uint8_t  pct     = total ? (uint8_t)((elapsed * 100UL) / total) : 0;
            const int16_t  barW    = (int16_t)((int32_t)(w - 2 * pad) * pct / 100);
            _tft.fillRoundRect(x + pad, y + h - 14, w - 2 * pad, 6, 3, Theme::SURFACE2);
            if (barW > 0) _tft.fillRoundRect(x + pad, y + h - 14, barW, 6, 3, zColor);
        } else if (!mapped) {
            // Annoncer un prochain arrosage sur une zone sans sortie serait
            // faux : ce creneau ne se produira jamais. La tuile est deja
            // hachuree, le texte doit dire la meme chose.
            _tft.setTextColor(Theme::AMBER, bg);
            _tft.drawString("Non affectee", x + pad, y + 26);
            _tft.setTextColor(Theme::MUTED, bg);
            _tft.drawString("Cablage relais", x + pad, y + 40);

        } else {
            _tft.setTextColor(Theme::MUTED, bg);
            _tft.drawString("Prochain", x + pad, y + 26);
            _tft.setTextColor(Theme::TEXT, bg);
            _tft.setTextSize(roomy ? 2 : 1);
            _tft.drawString(nextSlotLabel(zone).c_str(), x + pad, y + 38);
            _tft.setTextSize(1);
        }
        _tft.setTextDatum(TL_DATUM);
        return;
    }
#endif

    // Nom de zone dans le bouton ; aucune bulle ici.
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

// Geometrie de la grille 5-8 zones. Appelee au demarrage et a chaque
// rechargement de configuration : la hauteur des cartes depend du nombre de
// zones, la largeur de celle de l'ecran.
//
// Sur la carte historique les valeurs restent celles d'origine (64/65/126/50,
// calees sur 320x240). Sur la JC4827W543C_I la grille etait simplement
// recopiee telle quelle et laissait 160 px de large et 32 px de haut
// inutilises - c'est ce que l'utilisateur a releve le 29 aout 2026.
void DisplayManager::updateGrid2Geometry() {
#if AQUALOOK_BOARD_S3
    _g2PlanW = 150;                 // colonne planning, elargie
    // Marges autour du bloc de boutons : sans elles les cartes collaient au
    // separateur du planning a gauche et au bord de la dalle a droite et en
    // bas. Le bloc est ensuite centre verticalement dans la place restante.
    constexpr uint16_t marginX = 10;
    constexpr uint16_t marginY = 8;

    _g2GridX = _g2PlanW + 2 + marginX;

    const uint8_t  nb   = min(_nbZones, (uint8_t)8);
    const uint8_t  rows = (uint8_t)((nb + 1) / 2);   // toujours 2 colonnes
    // Ecart minimal entre cartes : la valeur de configuration vaut 1 ou 2 px,
    // suffisant en 320x240 ou les cartes etaient serrees par necessite. Ici
    // elles paraissaient soudees les unes aux autres.
    const uint16_t gap = max<uint16_t>(_g2Gpad, 10);
    const uint16_t availW = SCREEN_W - _g2GridX - marginX;
    _g2Gw = (uint16_t)((availW - gap) / 2);

    if (rows > 0) {
        const uint16_t availH = G2_CONTENT_H - 2 * marginY;
        _g2Gh = (uint16_t)((availH - (rows - 1) * gap) / rows);
        // Plafond de lisibilite : au-dela, une carte de 2 lignes de texte
        // parait vide plutot que genereuse.
        if (_g2Gh > 86) _g2Gh = 86;

        const uint16_t blockH = rows * _g2Gh + (rows - 1) * gap;
        _g2GridY = G2_CONTENT_Y +
                   (uint16_t)((G2_CONTENT_H > blockH)
                              ? (G2_CONTENT_H - blockH) / 2 : 0);
    }
    _g2Gpad = gap;   // les sites de dessin et de test tactile l'utilisent
    _g2PlanHdrH = 58;
#else
    _g2PlanW = 64;
    _g2GridX = 65;
    _g2Gw    = 126;
    _g2Gh    = 50;
    _g2GridY = G2_CONTENT_Y;
    _g2PlanHdrH = 42;
#endif
    _g2PlanColW = (uint16_t)((_g2PlanW - G2_PLAN_LABEL_W) / 2);
    const uint8_t nbPlan = min(_nbZones, (uint8_t)8);
    _g2PlanZoneH = nbPlan ? (uint16_t)((G2_CONTENT_H - _g2PlanHdrH) / nbPlan)
                          : (uint16_t)(G2_CONTENT_H - _g2PlanHdrH);
    if (_g2PlanZoneH < 4) _g2PlanZoneH = 4;
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
    // Separateur cale juste apres la colonne planning, pas contre les cartes :
    // _g2GridX inclut desormais la marge laterale du bloc de boutons, l'y
    // poser collait le trait aux bordures des cartes.
    _tft.drawFastVLine(_g2PlanW + 2, G2_CONTENT_Y, G2_CONTENT_H, Theme::BORDER);

    // ── Colonne gauche : planning aujourd'hui (toutes les zones) ──
    renderPlanSpriteCompact(G2_CONTENT_H, G2_CONTENT_Y, _g2PlanW);

    // ── Colonne droite : grille marche forcée ──
    uint8_t maxZ = min(_nbZones, (uint8_t)8);
    for (uint8_t z = 0; z < maxZ; z++) {
        uint8_t  col = z % 2;
        uint8_t  row = z / 2;
        uint16_t bx  = _g2GridX + col * (_g2Gw + _g2Gpad);
        uint16_t by  = _g2GridY + row * (_g2Gh + _g2Gpad);
        drawZoneBtn(z, bx, by, _g2Gw, _g2Gh);
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
        renderPlanSpriteCompact(G2_CONTENT_H, G2_CONTENT_Y, _g2PlanW);
    }

    const uint8_t maxZ = min(_nbZones, (uint8_t)8);
    for (uint8_t z = 0; z < maxZ; ++z) {
        const uint8_t col = z % 2;
        const uint8_t row = z / 2;
        const uint16_t x = _g2GridX + col * (_g2Gw + _g2Gpad);
        const uint16_t y = _g2GridY + row * (_g2Gh + _g2Gpad);
        const bool active = _relais && _relais->getState(z);
        const uint32_t remainMs = (active && _schedule) ? _schedule->getRemainingMs(z) : 0;
        const uint32_t remainSec = active ? (remainMs / 1000UL) : UINT32_MAX;

        if (s_zoneActiveCache[z] != (active ? 1 : 0)) {
            drawZoneBtn(z, x, y, _g2Gw, _g2Gh);
            s_zoneActiveCache[z] = active ? 1 : 0;
            s_zoneRemainSecCache[z] = remainSec;
            continue;
        }
        if (!active || remainSec == s_zoneRemainSecCache[z]) continue;

#if AQUALOOK_BOARD_S3
        // Redessin complet de la carte, comme en mode 1-4 zones. Le
        // rafraichissement partiel ci-dessous efface une bande a y+15 et
        // reecrit l'heure centree a y+16 : ces positions visent la carte
        // 126x50 d'origine et tombent en plein sur le nom de zone et la
        // pastille d'etat de la carte 161xN, qu'elles recouvrent a moitie
        // (constate sur materiel reel le 29 aout 2026, zone 7).
        drawZoneBtn(z, x, y, _g2Gw, _g2Gh);
#else
        // Mise à jour locale du timer : pas de ré-écriture du fond rouge ni des bordures.
        _tft.fillRect(x + 10, y + 15, _g2Gw - 20, 22, Theme::ACTIVE_BG);
        _tft.setFreeFont(nullptr);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextSize(2);
        _tft.setTextColor(Theme::TEXT, Theme::ACTIVE_BG);
        char buf[12];
        snprintf(buf, sizeof(buf), "%02lu:%02lu", remainMs / 60000UL, (remainMs % 60000UL) / 1000UL);
        _tft.drawString(buf, x + _g2Gw / 2, y + 16);
        _tft.setTextSize(1);
        _tft.setTextDatum(TL_DATUM);
#endif
        s_zoneRemainSecCache[z] = remainSec;
    }
}

void DisplayManager::handleTouchHome_grid2(uint16_t tx, uint16_t ty) {
    // [≡] menu
    if (hitTest(0, 0, 40, G2_HDR_H, tx, ty)) { goTo(Screen::ADMIN); return; }

    // Colonne droite uniquement : grille → toggle arrosage direct
    if (tx >= _g2GridX) {
        uint8_t maxZ = min(_nbZones, (uint8_t)8);
        for (uint8_t z = 0; z < maxZ; z++) {
            uint8_t  col = z % 2;
            uint8_t  row = z / 2;
            uint16_t bx  = _g2GridX + col * (_g2Gw + _g2Gpad);
            uint16_t by  = _g2GridY + row * (_g2Gh + _g2Gpad);
            if (hitTest(bx, by, _g2Gw, _g2Gh, tx, ty)) {
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
    uint16_t zColor = zoneColor(zone);
    // Au-dela de 4 zones l'ecran utilise ces rangees, pas les cartes : c'est
    // ici que l'absence de sortie doit se voir. Sans quoi la rangee invite a
    // "appuyer pour arroser" et offre un bouton GO vert pour une action qui
    // ne fera rien.
    const bool mapped = _relais.relay &&
        RelayTopology::resolveZoneValve(
            _relais.relay->topology(), zone, _nbZones).valid;

    drawCardBg(_tft, x, y, w, h, Theme::R_SM, bg, border, true);
    drawAccentBar(_tft, x, y, h, Theme::R_SM, mapped ? zColor : Theme::SURFACE2);
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
    } else if (!mapped) {
        _tft.setTextColor(Theme::AMBER, bg);
        _tft.drawString("Aucune sortie affectee", x + 14, y + 14);
    } else {
        _tft.drawString("Appuyer pour arroser", x + 14, y + 14);
    }

    // Bouton GO/STOP à droite — couleur vive assumée (action, pas une carte)
    uint16_t btnX = x + w - 36;
    uint16_t btnY = y + h/2 - 10;
    // Pas de bouton vert engageant pour une zone qu on ne peut pas piloter.
    const uint16_t btnBg = !mapped ? Theme::SURFACE2
                                   : (active ? Theme::RED : Theme::GREEN);
    _tft.fillRoundRect(btnX, btnY, 32, 20, Theme::R_SM, btnBg);
    _tft.setTextColor(!mapped ? Theme::MUTED : (active ? Theme::TEXT : 0x0000), btnBg);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(!mapped ? "N/C" : (active ? "STOP" : "GO"), btnX + 16, btnY + 10);
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
    uint16_t zColor = zoneColor(zone);

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
        _tft.drawString(rbuf, x + w / 2, y + 22);
        _tft.setTextSize(1);
        // Barre de progression remontée pour réserver deux lignes au hint.
        uint8_t pct  = (tot > 0) ? (uint8_t)((el * 100UL) / tot) : 0;
        uint16_t bw  = (uint16_t)((w - 8) * pct / 100);
        _tft.fillRoundRect(x + 4, y + 47, w - 8, 5, 2, Theme::BORDER);
        if (bw > 0) _tft.fillRoundRect(x + 4, y + 47, bw, 5, 2, Theme::AMBER);
        // Hint arrêt entièrement contenu dans le bouton.
        _tft.setTextColor(Theme::MUTED, bg);
        const uint16_t hint1Y = y + ((h > 24) ? h - 22 : 2);
        const uint16_t hint2Y = y + ((h > 13) ? h - 11 : 11);
        _tft.drawString("Appuyer", x + w / 2, hint1Y);
        _tft.drawString("pour arreter", x + w / 2, hint2Y);
    } else {
        // Prochain slot simplifié
        _tft.setTextColor(Theme::MUTED, bg);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString("Appuyer", x + w / 2, y + 45);
        _tft.drawString("pour arroser", x + w / 2, y + 56);

        // La météo reste exclusivement dans le bandeau planning.
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
    // C'est ici qu'on vient chercher pourquoi une zone ne part pas : le dire
    // franchement plutot que d'afficher un "ARRET" indiscernable d'un repos.
    const bool zoneMapped = _relais.relay &&
        RelayTopology::resolveZoneValve(
            _relais.relay->topology(), zone, _nbZones).valid;
    if (!zoneMapped) {
        _tft.setTextColor(Theme::AMBER, Theme::BG);
        _tft.drawString("SANS SORTIE AFFECTEE", 10, 40);
    } else {
        _tft.drawString(active ? "ARROSAGE EN COURS" : "ARRET", 10, 40);
    }
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
    // Pas de bouton vert engageant pour une zone qu on ne peut pas piloter.
    drawButton(2, 176, 230, 40,
               zoneMapped ? btnLabel : "Aucune sortie affectee",
               zoneMapped ? (active ? Theme::RED : Theme::GREEN) : Theme::SURFACE2,
               zoneMapped ? Theme::TEXT : Theme::MUTED);
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