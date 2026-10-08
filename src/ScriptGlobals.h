#pragma once

#include <Arduino.h>

// Variables globales des scripts (8 oct. 2026) : 16 entiers signes 32 bits,
// lus et ecrits par TOUS les scripts (g1..g16), qui survivent aux
// redemarrages.
//
// PERSISTANCE -- NVS, espace "aqlvars", a part de la configuration : un
// compteur qui bouge ne doit ni faire monter la revision de configuration
// (CloudSync la renverrait a chaque fois), ni risquer le bloc ALOK.
//   cle "v" : valeurs, bloc versionne {magic, version, nombre, v[16], crc}
//   cle "n" : noms, bloc versionne {magic, version, nombre, n[16][24], crc}
// Un bloc absent, d'une autre version ou au CRC faux est ignore : valeurs a
// 0, noms vides, et le journal le dit. Jamais d'echec de demarrage.
//
// USURE DE LA FLASH -- un script qui incremente une variable chaque seconde
// ne doit pas ecrire la flash chaque seconde. set() ne touche que la RAM ;
// update() ecrit au plus une fois par SAVE_INTERVAL_MS, et flush() ecrit tout
// de suite (appele avant chaque redemarrage voulu, voir BootLoopGuard).
// Une coupure de courant perd donc au plus la derniere minute.
//
// CONCURRENCE -- les scripts tournent dans la boucle principale, les routes
// Web dans la tache AsyncTCP : chaque acces aux tableaux passe par un verrou
// court. L'ecriture NVS, elle, n'a lieu que dans la boucle principale.
//
// Les noms ne servent qu'a l'affichage : le bytecode ne transporte qu'un
// numero. Un nom vide ou manquant n'empeche jamais un script de tourner.
namespace ScriptGlobals {

constexpr uint8_t  COUNT = 16U;
constexpr uint8_t  NAME_LEN_MAX = 23U;              // octets UTF-8, hors zero final
constexpr uint32_t SAVE_INTERVAL_MS = 60000UL;

// A appeler une fois au demarrage : relit valeurs et noms en NVS.
void begin();
// Vrai apres begin(). Le mode maintenance ne l'appelle pas : noms et valeurs
// y sont vides, et ne doivent pas etre presentes comme ceux du module.
bool started();

// i de 0 a COUNT-1. get() rend 0 hors bornes ; set() rend false hors bornes.
int32_t get(uint8_t i);
bool set(uint8_t i, int32_t value);

// Copie les COUNT valeurs d'un seul tenant (meme verrou) et rend leur CRC32 :
// sert a ne remonter les valeurs vers l'espace en ligne que lorsqu'elles ont
// change (CloudSync, rapport de telemetrie).
uint32_t snapshot(int32_t out[COUNT]);

// Copie le nom de la variable i (vide si aucun) dans out, borne a n.
void name(uint8_t i, char* out, size_t n);
// Remplace les 16 noms d'un coup (chaque entree deja validee en amont,
// <= NAME_LEN_MAX octets) et les ecrit en NVS immediatement : ils changent
// rarement. Rend false si l'ecriture echoue (les noms en RAM restent ceux
// d'avant).
bool setNames(const char* const names[COUNT]);

// Ecriture regroupee des valeurs, a appeler a chaque tour de boucle.
void update(uint32_t nowMs);
// Ecrit les valeurs tout de suite si elles ont change.
void flush();

}  // namespace ScriptGlobals
