#pragma once

#include <stdint.h>

#include "RelayTopology.h"

// Persistance du cablage relais -- desormais la SEULE source de verite.
//
// Ce magasin decrit le cablage reel : quelles cartes, a quelles adresses,
// avec quelle logique, et quelle voie physique porte quel role (vanne de
// zone, pompe, auxiliaire). Il accepte des voies non sequentielles et
// plusieurs cartes.
//
// Jusqu'au 7 septembre 2026 un module sans enregistrement DEDUISAIT un
// cablage depuis la config simple (nb de zones, controleur, logique). Cette
// deduction a ete retiree : deviner un cablage revenait a risquer d'ouvrir
// la mauvaise vanne. Sans enregistrement, le module ne pilote plus rien et
// le dit -- c'est un etat de premiere mise en service, pas une panne.
//
// Meme discipline que les autres espaces NVS du projet : magic, version de
// schema, garde de longueur, CRC, et refus au moindre doute.
namespace RelayTopologyStore {

// Charge un cablage persiste et coherent. false = rien d'enregistre ou bloc
// refuse ; l'appelant laisse alors la topologie VIDE (aucune sortie).
bool load(RelayTopology::RelayTopologyConfig& topology, uint8_t nbZones);

// Enregistre une topologie. Refuse d'ecrire un contenu incoherent.
bool save(const RelayTopology::RelayTopologyConfig& topology, uint8_t nbZones);

// Efface le cablage persiste : le module ne pilotera plus aucune sortie.
bool clear();

// Une topologie persistee est-elle presente (taille attendue) ?
bool exists();

}  // namespace RelayTopologyStore
