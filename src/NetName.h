#pragma once

#include <stddef.h>

// Nom du module sur le reseau local : on le joint par http://<nom>.local
// (mDNS) au lieu de son adresse IP, et la box peut le publier dans son DNS
// local (nom DHCP).
//
// Le nom est CELUI QUE L'UTILISATEUR A DONNE au module dans son espace en
// ligne ("Potager du fond" -> potager-du-fond), transmis a chaque synchro ;
// sans nom donne, le nom par defaut aqualook-<fin de l'adresse MAC>.
// Memorise en NVS (espace aq_net) : connu des le demarrage suivant, meme
// sans Internet. Le filtre d'hote de WebManager accepte deja les .local.
namespace NetName {

// A appeler AVANT WiFiManager::begin() : nom memorise ou par defaut, pose
// comme nom DHCP.
void prepare();

// A appeler a chaque tour de boucle : demarre mDNS a la premiere connexion.
void update(bool wifiConnected);

// Nom donne par l'utilisateur (libelle de l'espace en ligne, UTF-8) ; vide =
// revenir au nom par defaut. Boucle principale uniquement. Renomme mDNS a
// chaud ; le nom DHCP suit a la prochaine connexion.
void setLabel(const char* label);

// Nom sans suffixe (ex. "potager-du-fond"), vide avant prepare().
const char* name();

// true quand mDNS repond (http://<nom>.local utilisable).
bool announced();

}  // namespace NetName
