#pragma once

#include <stdint.h>

#include "config.h"

// Magasin des scripts de l'utilisateur.
//
// OU VIT QUOI, ET POURQUOI
//
// Le BYTECODE vit en NVS, avec le reste de la configuration. C'est lui qui
// fait tourner le module : il doit etre la meme quand la carte SD manque,
// est en train d'etre mise a jour, ou a ete retiree. Un module qui cesserait
// d'appliquer les regles de l'utilisateur parce qu'une carte SD est absente
// serait une mauvaise surprise.
//
// Le TEXTE SOURCE vit sur la carte SD. Il ne sert qu'a l'editeur -- le module
// ne le lit jamais pour agir. Le garder en NVS aurait coute plusieurs kilo-
// octets d'une partition de 84 Ko pour un confort d'edition.
//
// Consequence assumee : sans carte SD, les scripts TOURNENT mais ne se
// relisent pas dans l'editeur. C'est le bon sens de la panne -- on prefere
// perdre la possibilite d'editer que celle d'arroser.
//
// LE MODULE NE FAIT PAS CONFIANCE AU BYTECODE RECU
//
// Il est compile par le navigateur. Tout enregistrement passe donc par
// validateScriptProgram() : opcodes connus, operandes completes, sauts
// tombant sur une frontiere d'instruction. Un refus est signale a l'auteur,
// qui est devant son ecran -- bien mieux que de le decouvrir une nuit
// d'arrosage.

namespace ScriptStore {

static constexpr uint8_t MAX_SCRIPTS = 6;
static constexpr uint16_t MAX_BYTECODE = 400;
static constexpr uint8_t MAX_NAME = 24;

// Ce qui declenche un script.
//
// Tous surveillent une TRANSITION, jamais un etat : un script ne part pas
// parce qu'une zone arrose, mais parce qu'elle VIENT de commencer. Sans
// cela, un script se relancerait a chaque tour de boucle tant que la
// condition dure.
static constexpr uint8_t TRIGGER_NONE = 0;         // lancement manuel seulement
static constexpr uint8_t TRIGGER_INPUT_CHANGE = 1; // cible = identifiant d'entree
static constexpr uint8_t TRIGGER_ZONE_START = 2;   // cible = identifiant de zone
static constexpr uint8_t TRIGGER_ZONE_STOP = 3;    // cible = identifiant de zone

inline bool triggerIsZone(uint8_t trigger) {
    return trigger == TRIGGER_ZONE_START || trigger == TRIGGER_ZONE_STOP;
}

struct Meta {
    bool     used;
    bool     enabled;
    uint8_t  trigger;
    uint8_t  reserved;
    // Cible du declencheur : identifiant STABLE d'une entree ou d'une zone
    // selon le mode. Un seul champ, parce qu'un script n'a qu'un declencheur
    // -- deux champs dont un seul sert finiraient par se contredire.
    uint16_t triggerTarget;
    uint16_t codeSize;
    char     name[MAX_NAME];
};

// Ecrit un script. Refuse un bytecode que validateScriptProgram() rejette.
// reason recoit alors le nom de la raison, utilisable tel quel dans l'API.
bool save(uint8_t index, const Meta& meta, const uint8_t* code,
          const char*& reason);

bool load(uint8_t index, Meta& meta, uint8_t* code, uint16_t capacity);
bool erase(uint8_t index);

// Relit toutes les entetes d'un coup : c'est ce dont l'executeur a besoin a
// chaque changement d'entree, et lire le bytecode pour rien couterait cher.
void loadAllMeta(Meta* metas, uint8_t capacity);

} // namespace ScriptStore
