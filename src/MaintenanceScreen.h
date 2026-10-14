#pragma once

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════
//  MaintenanceScreen - progression lisible pendant une mise a jour (P2.14)
//
//  En mode maintenance, setup() est intercepte : ni DisplayManager ni
//  ScreenManager ne tournent, et l'ecran gardait jusqu'ici l'image violette
//  figee posee avant le redemarrage. On ne savait donc pas ou en etait le
//  module pendant 30 a 100 s.
//
//  Ce module initialise la dalle en dessin direct (aucun sprite, quelques
//  Ko de tas seulement : le TLS garde sa marge), peint l'ecran violet UNE
//  fois (begin, equivalent du redraw complet de ce mode), puis ne repeint
//  que la ligne d'etape et la barre de progression. Les LED de toutes les
//  zones passent au violet #6633cc : le module est occupe a autre chose que
//  piloter les vannes.
//
//  Chaque etape produit aussi une ligne serie [MAJ-ETAPE].
//
//  Tant que begin() n'a pas ete appele (mode normal, verification nocturne,
//  synchro), toutes les fonctions sont sans effet : les appelants n'ont pas
//  a savoir dans quel mode ils tournent. Sans ecran sur la carte historique.
// ═══════════════════════════════════════════════════════════════

namespace MaintenanceScreen {

// Dessine l'ecran de maintenance et met les LED de zone au violet.
// subtitle : ce qui est mis a jour ("Pages Web", "Programme du module").
void begin(const char* subtitle, uint8_t nbZones, uint8_t totalSteps);

bool active();

// Etape courante (1..totalSteps) ; efface la progression precedente.
void step(uint8_t index, const char* label);

// Progression de l'etape courante. kilobytes : affiche "820 / 1830 Ko (45 %)",
// sinon "12 / 27". detail : nom du fichier en cours, peut etre nul.
// Repeint au plus toutes les 250 ms (et toujours a 0 et a la fin) pour ne
// pas ralentir le telechargement.
void progress(uint32_t done, uint32_t total, bool kilobytes, const char* detail);

// Bilan avant le redemarrage. En echec, le message reste lisible
// FAIL_HOLD_MS avant le retour au mode normal.
void finish(bool ok, const char* message);

}  // namespace MaintenanceScreen
