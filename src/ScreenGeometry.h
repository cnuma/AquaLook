#pragma once

#include <Arduino.h>

#include "config.h"

// ═══════════════════════════════════════════════════════════════
//  Dimensions de la dalle - SOURCE DE VERITE UNIQUE du projet.
//
//  Extraites de DisplayManager.h le 29 aout 2026 pour pouvoir etre
//  partagees avec TJpg_Decoder.h, que DisplayManager.h inclut : la
//  dependance ne peut pas etre inversee, et TJpg_Decoder codait donc
//  320/160 en dur - il ne peignait que la moitie gauche d'un ecran de
//  480 px et centrait le texte du splash de travers.
//
//  DisplayManager::SCREEN_W / SCREEN_H restent definis a partir d'ici :
//  tout le code applicatif continue de les utiliser sans changement.
//  Toute mesure liee a la taille de l'ecran doit partir de ces
//  constantes, jamais d'un litteral.
// ═══════════════════════════════════════════════════════════════

namespace AquaLook {
namespace Panel {

#if AQUALOOK_BOARD_S3
constexpr uint16_t WIDTH  = AQ_S3_SCREEN_WIDTH;   // Guition JC4827W543C_I
constexpr uint16_t HEIGHT = AQ_S3_SCREEN_HEIGHT;
#else
constexpr uint16_t WIDTH  = 320;                  // CYD ESP32-2432S028R
constexpr uint16_t HEIGHT = 240;
#endif

constexpr uint16_t CENTER_X = WIDTH / 2;

}  // namespace Panel
}  // namespace AquaLook
