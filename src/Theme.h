#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════
//  Theme.h — palette et typographie LCD
//
//  But : harmoniser l'identité visuelle de l'afficheur avec le
//  dashboard web (style.css), sans en répliquer la luminosité.
//
//  RÈGLE : toute couleur ou police utilisée dans DisplayManager
//  doit provenir de ce fichier.
//
//  Variables configurables (extern) vs constantes fixes (constexpr) :
//
//  extern :   BG, SURFACE, SURFACE2, BORDER, TEXT, TEXT2, MUTED,
//             ACTIVE_BG, ZONE_COLORS[], R_SM, R_MD, R_LG, ACCENT_BAR_W
//             — définies dans Theme.cpp, modifiées via
//             DisplayManager::applyDisplayConfig() au boot ou sur
//             EventBus::displayDirty.
//
//  constexpr : accents nommés (GREEN, BLUE, AMBER, PURPLE, RED, CYAN),
//              couleurs d'état, splash — valeurs fixes, non exposées
//              à l'éditeur web.
//
//  Approche extern + Theme.cpp (vs inline C++17) : compatible C++11,
//  pas de modification de platformio.ini requise. PlatformIO compile
//  automatiquement Theme.cpp puisqu'il est dans src/.
//
//  Polices GFXFF : déjà incluses par TFT_eSPI.h → gfxfont.h.
//  NE PAS ajouter de #include <Fonts/GFXFF/...> séparé :
//  erreur de redéfinition à la compilation (cas vécu 17/06/2026).
// ═══════════════════════════════════════════════════════════════

namespace Theme {

    // ── Surfaces (extern — configurables depuis la page web) ──
    extern uint16_t BG;
    extern uint16_t SURFACE;
    extern uint16_t SURFACE2;
    extern uint16_t BORDER;

    // ── Texte (extern — configurables) ────────────────────────
    extern uint16_t TEXT;
    extern uint16_t TEXT2;
    extern uint16_t MUTED;

    // ── Etat actif (extern — configurable) ────────────────────
    extern uint16_t ACTIVE_BG;

    // ── Accents zone (extern — configurables) ─────────────────
    // ZONE_COLORS[z % 4] — identite couleur de zone
    extern uint16_t ZONE_COLORS[4];

    // ── Formes (extern — configurables) ───────────────────────
    extern uint8_t  R_SM;
    extern uint8_t  R_MD;
    extern uint8_t  R_LG;
    extern uint8_t  ACCENT_BAR_W;

    // ── Accents nommés (constexpr — conservés pour les usages
    //    directs dans le code : icônes météo, boutons, états) ─
    // Valeurs initiales = même table que ZONE_COLORS[] par défaut.
    // Ne pas les supprimer : Theme::GREEN / BLUE / AMBER / PURPLE
    // sont utilisés directement dans DisplayManager.cpp en dehors
    // des tableaux ZONE_COLORS[].
    constexpr uint16_t GREEN   = 0x07E0;
    constexpr uint16_t BLUE    = 0x049F;
    constexpr uint16_t AMBER   = 0xFD20;
    constexpr uint16_t PURPLE  = 0x780F;
    constexpr uint16_t RED     = 0xF800;
    constexpr uint16_t CYAN    = 0x07FF;

    // ── Accents fixes (constexpr — non exposés) ────────────────
    constexpr uint16_t BORDER2        = 0x534B;
    constexpr uint16_t ACTIVE_BG_SOFT = 0x3904;
    constexpr uint16_t ACTIVE_BORDER  = 0x7965;
    constexpr uint16_t ON_ACTIVE_TEXT  = 0xFFE0;
    constexpr uint16_t ON_ACTIVE_MUTED = 0xBDF7;

    // ── Profondeur (constexpr — toujours très sombre) ─────────
    constexpr uint16_t SHADOW  = 0x0882;

    // ── Splash (constexpr — fond blanc distinct) ───────────────
    constexpr uint16_t SPLASH_ACCENT = 0x049F;
    constexpr uint16_t SPLASH_MUTED  = 0x7BEF;
    constexpr uint16_t SPLASH_MUTED2 = 0xAD55;
    constexpr uint16_t SPLASH_TRACK  = 0xD69A;

    // ── Typographie (macros — polices GFXFF) ──────────────────
    #define THEME_FONT_TITLE     &FreeSansBold9pt7b
    #define THEME_FONT_BODY      &FreeSans9pt7b
    #define THEME_FONT_HEADLINE  &FreeSansBold12pt7b
    // Le splash réutilise la 12pt déjà liée au firmware afin de ne pas
    // embarquer uniquement pour le fallback la table Bold18 (~5,6 Kio).
    #define THEME_FONT_SPLASH    &FreeSansBold12pt7b

} // namespace Theme
