// ═══════════════════════════════════════════════════════════════
//  Theme.cpp — définitions des variables de palette LCD
//
//  Ce fichier contient les définitions (une seule fois dans le
//  programme) des variables déclarées extern dans Theme.h.
//  Les valeurs initiales correspondent à la palette par défaut.
//  Elles sont surchargées au boot par DisplayManager::applyDisplayConfig()
//  qui lit ConfigManager::display() (config.json section "display").
//
//  IMPORTANT : ce fichier ne doit pas être inclus manuellement.
//  PlatformIO le compile automatiquement car il est dans src/.
// ═══════════════════════════════════════════════════════════════
#include "Theme.h"

namespace Theme {

    // ── Surfaces ──────────────────────────────────────────────
    uint16_t BG       = 0x10C3;  // #101818 — ardoise très sombre
    uint16_t SURFACE  = 0x1924;  // #182420 — surface légèrement élevée
    uint16_t SURFACE2 = 0x2985;  // #283028 — bandeaux secondaires
    uint16_t BORDER   = 0x3A68;  // #384c40 — séparateurs

    // ── Texte ─────────────────────────────────────────────────
    uint16_t TEXT     = 0xFFFF;  // #f8fcf8 — texte principal
    uint16_t TEXT2    = 0xC699;  // #c0d0c8 — texte secondaire
    uint16_t MUTED    = 0x7CF0;  // #789c80 — labels

    // ── Etat actif ────────────────────────────────────────────
    uint16_t ACTIVE_BG = 0x3904; // #382020 — fond carte zone active

    // ── Accents zone ──────────────────────────────────────────
    // Ordre : vert / bleu / amber / violet
    // Modifié par applyDisplayConfig() depuis CfgDisplay::cZone0..3
    uint16_t ZONE_COLORS[4] = { 0x07E0, 0x049F, 0xFD20, 0x780F };

    // ── Formes ────────────────────────────────────────────────
    uint8_t  R_SM        = 4;    // rayon boutons standard (px)
    uint8_t  R_MD        = 6;    // rayon cartes grille (px)
    uint8_t  R_LG        = 10;   // rayon grands boutons (px)
    uint8_t  ACCENT_BAR_W = 3;   // largeur barre couleur de zone (px)

} // namespace Theme
