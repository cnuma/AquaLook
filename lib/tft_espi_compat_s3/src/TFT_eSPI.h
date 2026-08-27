#pragma once
// Adaptateur TFT_eSPI/TFT_eSprite pour la carte Guition JC4827W543C_I
// (ESP32-S3, controleur d'ecran NV3041A en QSPI), implemente au-dessus
// d'Arduino_GFX.
//
// Strategie documentee dans docs/architecture/HW_JC4827W543_PORT_IMPACT.md
// (§3, §11) : exposer l'API TFT_eSPI reellement utilisee par le projet
// (verifiee par grep sur le code reel, pas seulement par lecture de la
// doc) plutot que de reecrire les ~430 sites d'appel de DisplayManager.cpp
// et consorts. Ces fichiers ne sont PAS modifies pour la partie dessin :
// seule cette bibliotheque change d'un environnement PlatformIO a
// l'autre (voir platformio.ini, lib_ignore de la vraie bodmer/TFT_eSPI
// pour l'env ProgrammeArrosage_s3, et de celle-ci pour les env legacy).
//
// _tft (classe TFT_eSPI) dessine DIRECTEMENT sur le panneau, sans tampon
// intermediaire : comportement identique a TFT_eSPI historique, qui
// ecrit sur l'ecran a chaque appel. Seul TFT_eSprite bufferise (son
// propre Arduino_Canvas), exactement comme TFT_eSprite historique -
// pushSprite() recopie explicitement vers l'ecran. Aucune notion de
// flush() globale n'est necessaire ici, contrairement a la maquette de
// test test_screen_s3.cpp qui enveloppait tout dans un Arduino_Canvas
// plein ecran pour mesurer un cout de transfert (test 4) - ce n'est pas
// l'architecture retenue pour le rendu applicatif.
//
// Toutes les primitives de dessin sont des no-op silencieux si la cible
// (_target) n'est pas encore prete : c'est le comportement attendu par
// DisplayManager.h (suspendForMemoryRelief()/resumeAfterMemoryRelief()),
// qui compte sur le fait qu'un TFT_eSprite non alloue ne plante pas.

#include <Arduino.h>
#include <Print.h>
#include <Arduino_GFX_Library.h>

// Polices GFXFF utilisees par Theme.h (THEME_FONT_*) : la vraie
// TFT_eSPI.h les rend disponibles globalement quand LOAD_GFXFF=1, sans
// que Theme.h ait besoin de les inclure lui-meme ("NE PAS ajouter de
// #include <Fonts/GFXFF/...> separe" - Theme.h:30). On reproduit ce
// comportement ici, depuis Adafruit GFX Library (source originale de
// ces polices, meme format GFXfont qu'Arduino_GFX attend).
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// Arduino_GFX.h definit RED/GREEN/BLUE/CYAN/PURPLE/... comme macros
// GLOBALES sans prefixe (#define RED RGB565_RED, etc). Le preprocesseur
// ne connait pas les espaces de noms C++ : Theme::RED/GREEN/BLUE/CYAN/
// PURPLE (Theme.h) seraient donc corrompus en Theme::RGB565_RED etc a
// la compilation. On les annule immediatement apres l'inclusion -
// Theme.h definit ses propres constantes typees juste apres, aucune
// des deux bibliotheques n'a besoin de ces macros sous ce nom precis.
#undef BLACK
#undef NAVY
#undef DARKGREEN
#undef DARKCYAN
#undef MAROON
#undef PURPLE
#undef OLIVE
#undef LIGHTGREY
#undef DARKGREY
#undef BLUE
#undef GREEN
#undef CYAN
#undef RED
#undef MAGENTA
#undef YELLOW
#undef WHITE
#undef ORANGE

// ── Couleurs RGB565, jeu standard TFT_eSPI ────────────────────────────
#define TFT_BLACK       0x0000
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
#define TFT_LIGHTGREY   0xC618
#define TFT_DARKGREY    0x7BEF
#define TFT_BLUE        0x001F
#define TFT_GREEN       0x07E0
#define TFT_CYAN        0x07FF
#define TFT_RED         0xF800
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_WHITE       0xFFFF
#define TFT_ORANGE      0xFDA0
#define TFT_GREENYELLOW 0xB7E0
#define TFT_PINK        0xFE19
#define TFT_SKYBLUE     0x867D
#define TFT_VIOLET      0x915C

// ── Datums texte, jeu standard TFT_eSPI ───────────────────────────────
#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define MC_DATUM 4
#define MR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8

class TFT_eSPI : public Print {
public:
    TFT_eSPI();
    virtual ~TFT_eSPI() {}

    // Initialise le bus QSPI + le panneau NV3041A + le retroeclairage.
    // Broches/parametres : voir la section [jc4827w543c_i] de
    // platformio.ini (AQ_S3_LCD_*), confirmes sur materiel reel.
    void init();

    // No-op documente : le panneau est deja monte dans l'orientation
    // attendue (rotation=0 au constructeur, confirme sur materiel reel
    // par test_screen_s3.cpp) - DisplayManager.cpp appelle
    // setRotation(1) par habitude de l'ancien panneau ILI9341 (portrait
    // natif, tourne en paysage), ce qui n'a pas de sens ici (NV3041A
    // deja nativement 480x272 paysage).
    void setRotation(uint8_t r);

    void fillScreen(uint16_t color);
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint16_t color);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint16_t color);
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint16_t color);
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t color);
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color);
    void fillCircle(int32_t x, int32_t y, int32_t r, uint16_t color);
    void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t color);

    void setTextColor(uint16_t fg);
    void setTextColor(uint16_t fg, uint16_t bg);
    void setTextSize(uint8_t s);
    void setFreeFont(const GFXfont *f);
    void setTextDatum(uint8_t datum);
    void drawString(const char *text, int32_t x, int32_t y);
    int32_t textWidth(const char *text);
    int32_t fontHeight();

    // JPEG (TJpg_Decoder.h) : setSwapBytes(true) avant un pushImage
    // signale des octets RVB565 en ordre big-endian (sortie standard
    // d'un decodeur JPEG) -> route vers draw16bitBeRGBBitmap plutot que
    // draw16bitRGBBitmap.
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data);
    void setSwapBytes(bool swap);

    // Print - non utilise en pratique par le projet (aucun tft.print()/
    // println() trouve par grep), fourni par prudence/completude.
    size_t write(uint8_t c) override;

    Arduino_GFX *target() const { return _target; }

protected:
    Arduino_GFX *_target = nullptr;  // panneau (this) ou canvas (sprite)
    const GFXfont *_font = nullptr;
    uint8_t _datum = TL_DATUM;
    uint16_t _fg = TFT_WHITE;
    uint16_t _bg = TFT_BLACK;
    bool _bgOpaque = false;
    bool _swapBytes = false;

    void applyTextStyle();
};

class TFT_eSprite : public TFT_eSPI {
public:
    explicit TFT_eSprite(TFT_eSPI *parent);
    ~TFT_eSprite() override;

    // Retourne le pointeur du tampon (non nul = succes), jamais nullptr
    // sans avoir nettoye l'etat interne - voir DisplayManager.h qui
    // teste explicitement la valeur de retour.
    void *createSprite(int16_t w, int16_t h);
    void fillSprite(uint16_t color);
    void *getPointer();
    void pushSprite(int32_t x, int32_t y);
    void deleteSprite();

private:
    TFT_eSPI *_parent;
    Arduino_Canvas *_canvas = nullptr;
    int16_t _w = 0;
    int16_t _h = 0;
};
