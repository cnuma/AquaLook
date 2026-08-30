#pragma once

#include <Arduino.h>
#include "ConfigManager.h"   // MAX_ACTIVE_ZONES

// ═══════════════════════════════════════════════════════════════
//  Voyant d'etat - point de passage OBLIGATOIRE vers le materiel
//
//  Toute la logique de voyant (priorites arrosage / WiFi / mise a jour,
//  modes utilisateur, surcharge rouge de FaultManager) vit dans
//  ScreenManager::updateLed() et n'a AUCUNE raison de connaitre le
//  materiel. Ce fichier est la seule frontiere entre cette logique et
//  les deux realisations physiques tres differentes :
//
//    - carte historique ESP32-2432S028R : voyant RGB embarque, trois
//      canaux LEDC en logique INVERSEE (anode commune : 0 = pleine
//      lumiere). Un seul point lumineux, pas de notion de zone.
//
//    - carte Guition JC4827W543C_I (ESP32-S3) : aucun voyant embarque
//      (constate le 25 aout 2026, HW_JC4827W543_PORT_IMPACT.md §8
//      test 7). Un ruban WS2812 externe sur GPIO46 le remplace, avec
//      en prime une LED par zone.
//
//  Sans cette frontiere, chaque etat a afficher devrait etre porte
//  separement dans ScreenManager - exactement le travers qui a impose
//  de corriger trois fois le meme defaut avant HeapMetrics.h.
//
// ── Affectation des LED (ruban S3) ──────────────────────────────
//  Regle posee par l'utilisateur le 30 aout 2026 :
//      nombre de LED = nombre de zones + 1
//
//    LED 0        etat general du module (ce que calcule updateLed())
//    LED 1 + z    zone z, dans sa couleur de theme
//
//  Le ruban physique peut etre plus long : les LED au-dela de
//  nbZones + 1 sont maintenues eteintes.
//
// ── Pourquoi commit() est separe des setters ────────────────────
//  renderLed() est appelee a CHAQUE tour de boucle. Pousser le ruban
//  aussi souvent serait un gaspillage franc : chaque envoi WS2812 dure
//  ~30 us par LED et doit etre suivi d'un silence de 50 us. commit()
//  ne transmet donc que si une couleur a reellement change.
// ═══════════════════════════════════════════════════════════════

namespace AquaLook {
namespace StatusLed {

// Une LED d'etat general, plus une par zone active.
constexpr uint8_t MAX_LEDS = MAX_ACTIVE_ZONES + 1U;

// A appeler une fois au demarrage, avant toute autre fonction.
void begin();

// Nombre de zones actives (config.system().nbZones). Determine la
// longueur reellement pilotee : nbZones + 1. Les LED au-dela sont
// eteintes des le prochain commit().
void setZoneCount(uint8_t nbZones);

// Etat general du module -- LED 0 sur le ruban, voyant RGB embarque
// sur la carte historique. Couleur deja resolue par l'appelant
// (FaultManager a impose sa priorite en amont).
void setStatus(uint8_t r, uint8_t g, uint8_t b);

// Etat d'une zone -- LED 1 + zone. Sans effet sur la carte historique,
// qui n'a qu'un seul point lumineux : la logique appelante n'a donc
// pas a se demander sur quelle carte elle tourne.
void setZone(uint8_t zone, uint8_t r, uint8_t g, uint8_t b);

// Eteint toutes les LED de zone (pas l'etat general).
void clearZones();

// Transmet au materiel, uniquement si quelque chose a change.
void commit();

}  // namespace StatusLed
}  // namespace AquaLook
