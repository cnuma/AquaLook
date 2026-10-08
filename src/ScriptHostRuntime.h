#pragma once

#include "ConfigManager.h"
#include "InputSampler.h"
#include "ScheduleManager.h"
#include "domain/ScriptVm.h"

// Pont entre la machine a scripts et le module reel.
//
// La machine ne connait ni ScheduleManager ni InputSampler : elle DEMANDE,
// et c'est ici qu'on decide si la demande est recevable. C'est ce qui permet
// de la mettre a l'epreuve sans materiel, et surtout de garder les garde-fous
// hors de portee du script.
//
// TOUT EST DESIGNE PAR IDENTIFIANT STABLE
//
// Un script cite un identifiant de zone ou d'entree, jamais un index. Un
// index se decale quand on supprime une zone, et le script se mettrait alors
// a arroser autre chose sans que rien ne le signale.
//
// CE QUE L'HOTE REFUSE
//
// Un refus arrete le script sur une raison nommee, plutot que de le laisser
// insister. Sont refuses : une zone ou une entree inconnue, une entree dont
// la valeur n'est pas encore stabilisee, et toute action pas encore
// implementee. Mieux vaut un script arrete qu'un script qui croit avoir agi.

struct ScriptRuntimeContext {
    const InputSampler* inputs = nullptr;
    ScheduleManager* schedule = nullptr;
    const ConfigManager* config = nullptr;
    // Compteurs de diagnostic : ce que le script a reellement obtenu.
    uint16_t reads = 0U;
    uint16_t actions = 0U;
    uint16_t refusals = 0U;
    uint16_t lastNotify = 0U;
    uint16_t lastAlert = 0U;
    // Nom du script, repris dans la notification : un code seul ne dirait pas
    // d'ou il vient.
    const char* name = "";
    // Detail du DERNIER refus (ex. "zone 5 : duree nulle ou negative") --
    // "action refusee" seul ne dit pas QUOI a echoue. Tampon fixe : pas
    // d'allocation dans le chemin d'execution des scripts.
    char refusalReason[48] = "";
    // Emplacement de CE script (0..5), pour refuser qu'il se lance lui-meme.
    uint8_t selfIndex = 0xFF;
    // Scripts que ce script demande de lancer, un bit par emplacement. Le
    // runner les demarre APRES le tick : demarrer un travail depuis l'hote,
    // en plein parcours de la table des travaux, la modifierait sous ses pieds.
    uint8_t launchMask = 0U;

    // Bloc parallele. Le runner pose freeSlots avant chaque tick ; fork()
    // n'accepte une branche que s'il reste une place, et note son debut dans
    // forkPc pour que le runner la demarre apres le tick. branchesRunning
    // compte les branches 2 en cours (decompte par le runner a leur fin).
    // Une machine-branche (isBranch) ne peut pas en ouvrir une autre.
    static constexpr uint16_t NO_FORK = 0xFFFFU;
    uint8_t freeSlots = 0U;
    uint16_t forkPc = NO_FORK;
    uint8_t branchesRunning = 0U;
    bool isBranch = false;
    // Rendez-vous « ou » : generation du bloc parallele en cours (les
    // branches en heritent a leur depart) et fin d'une de ses branches. Une
    // branche d'un bloc deja clos ne debloque pas le suivant.
    uint8_t branchGen = 0U;
    bool genDone = false;
};

const AquaLook::Domain::ScriptHostOps& scriptHostOps();
