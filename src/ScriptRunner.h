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
// Quatre sur ESP32-S3, deux sur l'ancienne carte CYD. Chaque script en cours
// immobilise un tampon de bytecode et une machine (~600 octets). Deux
// suffisaient tant qu'un script ne pouvait pas en lancer un autre ; depuis
// « lancer script N » (6 oct. 2026), un script qui en lance un second en
// occupe deja deux. Le S3 a la RAM pour quatre ; la CYD, tres juste en
// memoire, garde deux. Un declenchement de plus alors que toutes les places
// sont prises est SIGNALE, jamais avale en silence.
//
// UN SCRIPT PEUT EN LANCER UN AUTRE
//
// « lancer script N » ne fait que DEMANDER : le lancement a lieu apres le
// tour du script appelant, avec les memes regles qu'un declenchement (place
// libre, script actif, pas deja en cours, pas relance en moins de 5 s). Un
// refus est journalise et le script appelant continue.
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
#if AQUALOOK_BOARD_S3
    static constexpr uint8_t MAX_CONCURRENT = 4;
#else
    static constexpr uint8_t MAX_CONCURRENT = 2;
#endif

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
    // Detail du refus (ex. "zone 5 : duree nulle ou negative"), vide si
    // l'arret n'est pas un HOST_REFUSED ou si le dernier passage s'est bien
    // termine.
    const char* lastAbortDetail(uint8_t index) const;
    // Position dans le bytecode au moment de l'arret -- l'editeur, qui
    // recompile le MEME source, peut la retraduire en ligne pour surligner
    // l'endroit en cause. 0 si le dernier passage s'est bien termine.
    uint16_t lastAbortPc(uint8_t index) const;

private:
    struct Job {
        bool active = false;
        uint8_t index = 0xFF;
        // Branche 2 d'un bloc parallele : meme script (index) que son parent,
        // qui tourne dans la place parentSlot.
        bool isBranch = false;
        uint8_t parentSlot = 0xFF;
        // Generation du bloc parallele qui l'a ouverte (ctx.branchGen du
        // parent au depart) : seule une branche du bloc en cours debloque
        // un rendez-vous « ou ».
        uint8_t gen = 0U;
        uint16_t codeSize = 0U;
        char name[ScriptStore::MAX_NAME] = {};
        uint8_t code[ScriptStore::MAX_BYTECODE] = {};
        AquaLook::Domain::ScriptVm vm;
        ScriptRuntimeContext ctx;
    };

    bool start(uint8_t index, const char*& reason);
    // Demarre les scripts demandes par « lancer script N » (masque d'un bit
    // par emplacement) au nom du script `caller`.
    void launchRequested(uint8_t caller, uint8_t mask);
    // Bloc parallele : demarre la branche 2 du travail parentSlot a `pc`.
    bool startBranch(uint8_t parentSlot, uint16_t pc);
    // Arrete les branches 2 encore en cours du travail parentSlot.
    void stopBranches(uint8_t parentSlot, const char* why);
    int8_t freeSlot() const;
    uint8_t freeCount() const;

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
    char _lastAbortDetail[ScriptStore::MAX_SCRIPTS][48] = {};
    uint16_t _lastAbortPc[ScriptStore::MAX_SCRIPTS] = {};
    bool _primed = false;
};
