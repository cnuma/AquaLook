#pragma once

#include <stddef.h>

// Nom du module sur le reseau local : on le joint par http://<nom>.local
// (mDNS) au lieu de son adresse IP, et la box peut le publier dans son DNS
// local (nom DHCP). Derive du nom du module en ligne (CloudSync moduleId,
// ex. "Jardin-01" -> "jardin-01"), sinon "aqualook-" + fin de l'adresse MAC.
// Un changement de nom prend effet au redemarrage suivant.
//
// Le filtre d'hote de WebManager accepte deja les noms en .local.
namespace NetName {

// A appeler AVANT WiFiManager::begin() : pose le nom DHCP.
void prepare(const char* moduleId);

// A appeler a chaque tour de boucle : demarre mDNS a la premiere connexion.
void update(bool wifiConnected);

// Nom sans suffixe (ex. "jardin-01"), vide avant prepare().
const char* name();

// true quand mDNS repond (http://<nom>.local utilisable).
bool announced();

}  // namespace NetName
