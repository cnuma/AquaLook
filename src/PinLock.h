#pragma once

#include <Arduino.h>
#include <stdint.h>

// Code PIN du LCD (decision D016, lot E).
//
// CE QU'IL PROTEGE
//
// L'ecran ADMIN (configuration) et le DEMARRAGE manuel d'une zone. Jamais
// l'arret d'une zone en cours : couper une vanne doit rester possible sans
// rien savoir (securite relais, D012). La consultation reste libre.
//
// STOCKAGE
//
// Espace NVS "aqlsec", a part du bloc de configuration ALOK : poser un PIN
// ne fait pas monter configRevision, et /api/resetConfig ne l'efface pas.
// Bloc versionne {magic, version, ..., crc32} ; seule une empreinte
// PBKDF2-HMAC-SHA256 salee est rangee, jamais le PIN. Bloc illisible = pas
// de PIN (message [SEC]), jamais un echec de demarrage ni un ecran bloque.
//
// ESSAIS
//
// Cinq essais libres, puis attente de 30 s doublee a chaque echec, plafond
// 15 min. Le compteur est persiste : un redemarrage ne le remet pas a zero
// (l'attente repart alors du demarrage).
//
// PAS DE PIN POSE = comportement d'avant le lot E : tout est libre.
//
// OUBLI
//
// clear() n'est appele que depuis un geste physique sur l'ecran (appui
// maintenu pendant le demarrage) : aucune route reseau ne sait l'effacer.
namespace PinLock {

constexpr uint8_t MIN_DIGITS = 4;
constexpr uint8_t MAX_DIGITS = 6;
constexpr uint32_t UNLOCK_WINDOW_MS = 5UL * 60UL * 1000UL;

enum class Result : uint8_t { OK, WRONG, LOCKED, NO_PIN };

// Lit le bloc et le compteur d'echecs. A appeler une fois au demarrage,
// NVS prete. Journalise l'etat ([SEC]).
void begin();

bool hasPin();

// true si aucun PIN n'est pose, ou s'il a ete saisi il y a moins de
// UNLOCK_WINDOW_MS et que lock() n'a pas ete appele depuis.
bool isUnlocked();

// Verifie un PIN saisi (chiffres seulement). OK ouvre la fenetre de
// deverrouillage et remet le compteur a zero.
Result verify(const char* digits);

// Pose ou remplace le PIN. Refuse si un PIN existe et que la session n'est
// pas deverrouillee, ou si le format est invalide. Ouvre la fenetre.
bool set(const char* digits);

// Retire le PIN depuis une session deverrouillee (choix de l'utilisateur
// dans ADMIN). Refuse sinon.
bool remove();

// Efface le PIN SANS verification. Reserve au geste physique de
// recuperation : voir l'en-tete.
void clear();

// Referme la fenetre (mise en veille de l'ecran, retour a l'accueil).
void lock();

// Secondes d'attente restantes avant un nouvel essai (0 = essai permis).
uint32_t lockoutRemainingSec();

uint8_t failures();

}  // namespace PinLock
