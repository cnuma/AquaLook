#pragma once

#include <stdint.h>

// Reglage de la dalle propre a CHAQUE module (P2.18, 17 oct. 2026).
//
// Deux series de la meme carte JC4827W543C_I ne se comportent pas pareil :
// sur le module de banc .141 l'image et le tactile concordent, sur le second
// module l'image est retournee de 180 degres par rapport au tactile et les
// couleurs sont inversees. Les macros de compilation (AQ_S3_LCD_IPS,
// AQ_S3_TOUCH_ROTATION) restent la reference ; ce module ne stocke que
// l'ecart du module par rapport a elles.
//
// Stockage : espace NVS dedie "aqlhw", hors du bloc de configuration ALOK
// (pas de changement de schema, pas de hausse de configRevision). La
// reinitialisation de la configuration ne l'efface pas : c'est une propriete
// du materiel, pas un reglage de l'utilisateur. Bloc absent ou illisible =
// valeurs de compilation, jamais un echec de demarrage.
namespace PanelSetup {

// Charge le reglage. configWasFresh : la configuration NVS etait absente a ce
// demarrage (module neuf ou efface). Sans reglage enregistre :
//  - module neuf      -> l'assistant d'ecran est demande (needsWizard) ;
//  - module en service -> valeurs de compilation enregistrees en silence,
//    pour qu'une mise a jour n'affiche jamais l'assistant.
void begin(bool configWasFresh);

bool needsWizard();

// Image tournee de 180 degres (rotation 2 du NV3041A au lieu de 0).
bool screenFlipped();
// Couleurs inversees par rapport a AQ_S3_LCD_IPS.
bool colorsInverted();
// Tactile a retourner (deux axes) pour concorder avec l'image affichee.
bool touchMirrored();

// Reglage en cours d'essai par l'assistant : RAM seulement.
void preview(bool screenFlip, bool touchFlip, bool invertColors);
bool touchFlipRaw();

// Enregistre le reglage courant ; l'assistant n'est plus demande.
bool save();

}  // namespace PanelSetup
