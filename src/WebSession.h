#pragma once
#include <Arduino.h>
#include <stdint.h>

// Session Web locale (D016, lot F).
//
// LE PROBLEME
//
// Le verrou administrateur de l'interface n'etait qu'un masque d'affichage
// dans le navigateur : toutes les routes /api/* repondaient a n'importe quel
// poste du reseau local. ApiAuth protegeait deja les scripts, mais chaque
// requete devait y etre signee une a une.
//
// CE QUE CE MODULE FAIT
//
// Le secret ApiAuth devient le mot de passe d'acces Web. Le navigateur
// demande un defi (nombre aleatoire de 128 bits, valable une minute, a usage
// unique), repond HMAC-SHA256(secret, "session|" + defi), et recoit un jeton
// de session de 128 bits dans un cookie HttpOnly. Le mot de passe ne circule
// jamais. Le filtre de WebManager exige ensuite ce cookie pour toutes les
// ecritures (classement W1, docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md
// §8.1).
//
// CE QU'IL NE FAIT PAS
//
// Il ne chiffre rien : HTTP reste en clair sur le reseau local, un poste qui
// ecoute peut capter le cookie (risque assume, D016). Les sessions vivent en
// RAM et disparaissent au redemarrage (voulu : rien de plus a proteger en
// NVS). Les essais rates sont limites en RAM, comme une porte qu'on ne peut
// rouvrir a distance : redemarrer le module exige deja une session.
//
// Constantes de securite, pas de service : comme celles de PinLock, elles ne
// se reglent pas depuis la console.
namespace WebSession {

constexpr uint8_t MAX_SESSIONS = 4;
constexpr uint32_t IDLE_TIMEOUT_MS = 30UL * 60UL * 1000UL;
constexpr uint8_t MAX_CHALLENGES = 4;
constexpr uint32_t CHALLENGE_TTL_MS = 60UL * 1000UL;
// Cinq essais libres, puis attente de 30 s doublee a chaque nouvel echec,
// plafonnee a 15 min -- meme progression que le PIN du LCD.
constexpr uint8_t FREE_FAILURES = 5;
constexpr uint32_t FIRST_LOCK_SEC = 30UL;
constexpr uint32_t MAX_LOCK_SEC = 15UL * 60UL;
constexpr uint8_t TOKEN_HEX_LEN = 32;   // 128 bits
constexpr const char* COOKIE_NAME = "aqls";

enum class LoginResult : uint8_t {
    OK,
    REFUSED,        // signature fausse : compte comme un echec
    LOCKED,         // trop d'echecs, attendre retryInSec
    NO_CHALLENGE,   // defi inconnu, expire ou deja servi
    NO_SECRET       // aucun mot de passe pose : rien a ouvrir
};

// Nouveau defi, 32 caracteres hexadecimaux + zero final.
bool newChallenge(char out[TOKEN_HEX_LEN + 1]);

// Verifie la reponse au defi et ouvre une session. Le defi est consomme dans
// tous les cas. outToken recoit le jeton a poser en cookie.
LoginResult login(const char* challengeHex, const char* signatureHex,
                  char outToken[TOKEN_HEX_LEN + 1], uint32_t& retryInSec);

// Ouvre une session sans defi. Reserve au cas ou la requete vient de prouver
// la connaissance du secret par un autre chemin (pose du premier secret,
// changement du secret sous session).
bool openTrusted(char outToken[TOKEN_HEX_LEN + 1]);

// En-tete Cookie complet de la requete (peut etre nul). Une session valide
// voit son delai d'inactivite repartir de zero.
bool isValid(const char* cookieHeader);

// Secondes avant expiration par inactivite, 0 si pas de session valide.
// Ne prolonge pas la session.
uint32_t remainingSec(const char* cookieHeader);

void close(const char* cookieHeader);

// Ferme toutes les sessions et oublie les defis en cours : secret oublie
// depuis l'ecran, ou remplace.
void closeAll();

uint8_t activeCount();

}  // namespace WebSession
