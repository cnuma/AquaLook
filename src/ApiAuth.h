#pragma once

#include <Arduino.h>
#include <stdint.h>

// Authentification des ecritures sensibles de l'API.
//
// LE PROBLEME
//
// L'interface du module n'a jamais eu d'authentification. Tant qu'elle ne
// servait qu'a regler des creneaux, le risque etait celui d'un reseau local :
// reel, mais borne. Accepter du CODE EXECUTABLE change l'echelle -- un
// script depose par n'importe qui commande des vannes, tout seul, la nuit.
//
// CE QUE CE MODULE FAIT, ET CE QU'IL NE FAIT PAS
//
// Il verifie que l'envoi vient de quelqu'un qui connait le secret partage,
// et qu'il n'est pas rejoue. Il ne CHIFFRE rien : la pile Web du module ne
// sait pas servir de TLS, donc le contenu circule en clair sur le reseau
// local. C'est une protection contre l'INJECTION, pas contre l'ecoute.
//
// Le dire est important : quelqu'un qui capture le reseau verra passer les
// scripts. Il ne pourra pas en deposer.
//
// LA SIGNATURE PORTE SUR LE SENS, PAS SUR LES OCTETS
//
// Signer le corps HTTP brut obligerait le module a le conserver tel quel
// pour le rejouer -- or il n'en garde qu'une version deja analysee, dont la
// re-serialisation ne redonne pas les memes octets (espaces, ordre des
// champs). On signe donc une forme CANONIQUE construite des deux cotes a
// partir des valeurs qui comptent.
//
// LE PREMIER SECRET
//
// Il ne peut etre pose que si aucun n'existe, ou en presentant l'actuel.
// C'est la confiance au premier usage : la fenetre d'exposition se limite a
// l'instant de la mise en service, sur son propre reseau. L'effacer demande
// une remise a zero de la configuration -- volontairement peu commode.

namespace ApiAuth {

// true si un secret est enregistre. Tant que non, les routes protegees
// REFUSENT : mieux vaut une fonction indisponible qu'une porte ouverte.
bool hasSecret();

// Pose ou remplace le secret. Refuse si un secret existe deja et que
// currentSecret ne correspond pas.
bool setSecret(const char* currentSecret, const char* newSecret);

// Verifie une signature hexadecimale sur un message canonique, et que le
// nonce progresse. Le nonce accepte est persiste : sans cela, un
// redemarrage permettrait de rejouer une requete capturee.
bool verify(const String& canonicalMessage, uint32_t nonce, const String& hexSignature);

// Dernier nonce accepte, pour que le client sache ou reprendre.
uint32_t lastNonce();

} // namespace ApiAuth
