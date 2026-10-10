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
// l'instant de la mise en service, sur son propre reseau.
//
// L'OUBLI DU SECRET
//
// Aucune route reseau ne sait l'effacer : ce serait rendre la signature
// contournable par quiconque est sur le LAN, exactement ce qu'elle protege.
// Seul un appui sur l'ecran du module (page ADMIN > Systeme, deux appuis de
// confirmation, voir DisplayManager::handleTouchAdmin) peut le faire --
// forgetSecret() n'est appele que depuis la, jamais depuis une requete HTTP.

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

// Verifie une signature sur un message SANS toucher au compteur de nonces :
// reserve au defi-reponse de la session Web (WebSession), dont le defi
// aleatoire a usage unique joue deja le role anti-rejeu.
bool verifyMessage(const char* message, const char* hexSignature);

// Premier secret (D016 lot F) : tant qu'aucun n'existe, setSecret ne
// l'accepte que pendant une fenetre ouverte par un geste sur l'ecran du
// module (ADMIN > Systeme). Sans cela, n'importe quel poste du reseau -- ou
// un programme malveillant qui y tourne -- pourrait poser le sien avant le
// proprietaire et l'enfermer dehors.
void allowFirstSecret(uint32_t windowMs);
uint32_t firstSecretWindowLeftMs();

// Remplace le secret sans qu'il circule en clair : encHex = nouveau secret
// XOR un flux HMAC(actuel, "aql-chg-ks|" + defi + "|0"/"|1"), macHex =
// HMAC(actuel, "aql-chg-mac|" + defi + "|" + encHex). Le defi doit avoir ete
// consomme par l'appelant (WebSession::consumeChallenge).
bool setSecretEncrypted(const char* challengeHex, const char* encHex, const char* macHex);

// Dernier nonce accepte, pour que le client sache ou reprendre.
uint32_t lastNonce();

// Efface le secret (et lui seul -- jamais les autres cles du namespace
// partage). A n'appeler QUE depuis un geste physique sur l'ecran du module :
// voir le commentaire plus haut. Le nonce n'est pas touche, une fois un
// nouveau secret pose il continue de progresser sans revenir a 0.
void forgetSecret();

} // namespace ApiAuth
