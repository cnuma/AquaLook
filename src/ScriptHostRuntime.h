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
};

const AquaLook::Domain::ScriptHostOps& scriptHostOps();
