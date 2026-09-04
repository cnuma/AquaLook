#pragma once

#include <stdint.h>

#include "RelayTopology.h"

// Persistance optionnelle de la topologie relais.
//
// Aujourd'hui la topologie est DERIVEE a chaque demarrage depuis la config
// simple (nb de zones, controleur, logique) par buildLegacyCompatibleTopology.
// Cette derivation reste la reference eprouvee et le comportement par defaut.
//
// Ce magasin ajoute la possibilite d'ENREGISTRER une topologie arbitraire
// (plusieurs cartes, roles pompe/auxiliaire, voies non sequentielles) qui prend
// alors le pas au demarrage. En l'absence d'enregistrement -- cas nominal --
// ou si le bloc est invalide, on retombe silencieusement sur la derivation
// legacy : aucun changement de comportement.
//
// Meme discipline que les autres espaces NVS du projet : magic, version de
// schema, garde de longueur, CRC, et refus au moindre doute.
namespace RelayTopologyStore {

// Charge une topologie persistee et coherente. false = rien d'enregistre ou
// bloc refuse ; l'appelant derive alors la topologie legacy.
bool load(RelayTopology::RelayTopologyConfig& topology, uint8_t nbZones);

// Enregistre une topologie. Refuse d'ecrire un contenu incoherent.
bool save(const RelayTopology::RelayTopologyConfig& topology, uint8_t nbZones);

// Efface la topologie persistee : retour a la derivation legacy.
bool clear();

// Une topologie persistee est-elle presente (taille attendue) ?
bool exists();

}  // namespace RelayTopologyStore
