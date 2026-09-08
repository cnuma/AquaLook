#pragma once

#include <stdint.h>

#include "ScriptHostRuntime.h"
#include "ScriptStore.h"
#include "domain/ScriptVm.h"

// Execution des scripts de l'utilisateur.
//
// QUAND UN SCRIPT PART
//
// Au CHANGEMENT de la valeur stabilisee de son entree -- pas a chaque
// lecture. L'echantillonneur compte les transitions ; le runner compare ce
// compteur a celui qu'il a vu la derniere fois. Un flotteur qui claquette ne
// produit aucune transition (l'anti-rebond l'absorbe), donc ne declenche
// rien.
//
// COMBIEN A LA FOIS
//
// Deux, volontairement. Chaque script en cours immobilise un tampon de
// bytecode et une machine ; six a la fois couteraient de la RAM pour un
// besoin qui n'existe pas. Un declenchement de plus alors que les deux
// places sont prises est SIGNALE, jamais avale en silence.
//
// UN SCRIPT DEJA EN COURS NE REDEMARRE PAS
//
// Si son entree rebascule pendant qu'il tourne, on l'ignore et on le dit.
// Le relancer perdrait l'etat du precedent -- typiquement une zone qu'il
// venait de suspendre et qu'il etait seul a savoir reprendre.
//
// LE BUDGET EST CELUI DE LA MACHINE
//
// update() ne fait qu'un tick par script et par tour de boucle : c'est la
// machine qui borne le travail, pas le runner. Un script ne peut donc jamais
// retarder l'affichage, le Web ou le planificateur.

class ScriptRunner {
public:
    static constexpr uint8_t MAX_CONCURRENT = 2;

    void begin(const InputSampler* inputs, ScheduleManager* schedule,
               const ConfigManager* config);

    // A appeler a chaque tour de boucle : detecte les declenchements et fait
    // avancer les scripts en cours.
    void update();

    // Lance un script a la demande, pour l'essayer. Retourne false si aucune
    // place n'est libre ou si l'emplacement est vide.
    bool runNow(uint8_t index, const char*& reason);

    bool isRunning(uint8_t index) const;
    uint8_t runningCount() const;
    // Derniere raison d'arret d'un script, pour l'interface. Vide si le
    // dernier passage s'est termine normalement.
    const char* lastAbort(uint8_t index) const;

private:
    struct Job {
        bool active = false;
        uint8_t index = 0xFF;
        char name[ScriptStore::MAX_NAME] = {};
        uint8_t code[ScriptStore::MAX_BYTECODE] = {};
        AquaLook::Domain::ScriptVm vm;
        ScriptRuntimeContext ctx;
    };

    bool start(uint8_t index, const char*& reason);
    int8_t freeSlot() const;

    Job _jobs[MAX_CONCURRENT];
    const InputSampler* _inputs = nullptr;
    ScheduleManager* _schedule = nullptr;
    const ConfigManager* _config = nullptr;

    // Compteur de transitions vu au dernier passage, par script.
    uint32_t _seenTransitions[ScriptStore::MAX_SCRIPTS] = {};
    // Etat d'arrosage de la zone surveillee, vu au dernier passage.
    bool _seenZoneActive[ScriptStore::MAX_SCRIPTS] = {};
    // Instant du dernier depart. Un script declenche par le demarrage d'une
    // zone et qui demarre cette meme zone se rappellerait sans fin : le
    // module refuse deja de relancer un script EN COURS, mais un script
    // court se terminerait avant de se voir relancer. Ce delai casse la
    // boucle dans tous les cas.
    uint32_t _lastStartMs[ScriptStore::MAX_SCRIPTS] = {};
    static constexpr uint32_t MIN_RESTART_MS = 5000U;
    const char* _lastAbort[ScriptStore::MAX_SCRIPTS] = {};
    bool _primed = false;
};
