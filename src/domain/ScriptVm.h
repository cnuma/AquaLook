#pragma once

#include <stddef.h>
#include <stdint.h>

// Machine a pile dediee au scripting d'arrosage.
//
// POURQUOI UNE MACHINE DEDIEE PLUTOT QU'UN LANGAGE EMBARQUE
//
// Un script peut rester EN ATTENTE des minutes -- le temps qu'une cuve se
// remplisse -- et le module redemarre chaque nuit. Son etat d'execution doit
// donc etre enregistrable et relisible tel quel. Les interpreteurs
// generalistes (Lua, Berry) ne savent pas serialiser leur pile ; c'est cette
// exigence-la, pas la taille, qui a tranche.
//
// POURQUOI ELLE NE COURT JAMAIS JUSQU'AU BOUT
//
// La boucle principale du module sert le WiFi, le serveur Web, l'ecran et le
// planificateur. Un script qui s'executerait d'un trait les gelerait tous --
// avec une vanne ouverte. Ici l'execution consomme un BUDGET d'instructions
// par tour de boucle puis rend la main. `while` et `for` deviennent alors
// sans danger : ils peuvent durer, jamais monopoliser.
//
// L'attente est une instruction qui rend la main, pas une boucle d'attente
// active : `tant que cuve_vide { attendre 1s }` ne coute presque rien.
//
// LE CHIEN DE GARDE
//
// Quatre plafonds, tous menant a un ARRET SIGNALE plutot qu'a un blocage :
// nombre total d'instructions, nombre de sauts arriere (iterations de
// boucle), duree d'execution, profondeur de pile. Un script qui les depasse
// est arrete, sa raison est nommee, et l'hote la remonte a l'utilisateur.
//
// LA SECURITE RESTE HORS DE PORTEE DU SCRIPT
//
// Aucune instruction ne peut lever la coupure de duree maximale ni toucher
// au cablage. Le script DEMANDE des actions ; c'est l'hote qui decide de les
// appliquer, avec ses propres garde-fous.

namespace AquaLook { namespace Domain {

enum class ScriptOp : uint8_t {
    HALT = 0,        // fin normale
    PUSH = 1,        // + int32 : empile une constante
    LOAD = 2,        // + u8    : empile la variable n
    STORE = 3,       // + u8    : depile vers la variable n
    DROP = 4,        //           depile et jette
    // Variables GLOBALES (8 oct. 2026) : partagees par tous les scripts et
    // conservees d'un redemarrage a l'autre, par l'hote (ScriptGlobals).
    GLOAD = 5,       // + u8    : empile la variable globale n (0..15)
    GSTORE = 6,      // + u8    : depile vers la variable globale n

    ADD = 16, SUB = 17, MUL = 18, DIV = 19, MOD = 20, NEG = 21,

    EQ = 32, NE = 33, LT = 34, LE = 35, GT = 36, GE = 37,
    AND = 38, OR = 39, NOT = 40,

    JMP = 48,        // + u16 : saut inconditionnel
    JZ = 49,         // + u16 : saut si le sommet vaut 0 (depile)
    JNZ = 50,        // + u16 : saut si le sommet est non nul (depile)

    READ_INPUT = 64, // + u16 id equipement : empile 0/1
    ZONE_ACTIVE = 65,// + u16 id zone       : empile 0/1
    ZONE_REMAIN = 66,// + u16 id zone       : empile les secondes restantes

    ACTION = 80,     // + u8 action, + u16 cible : depile l'argument eventuel
    NOTIFY = 81,     // + u16 code message : trace dans le journal
    WAIT = 82,       // depile des secondes, rend la main jusqu'a echeance
    // + u16 code : NOTIFIE l'utilisateur pour de bon (serveur configure).
    // Distincte de NOTIFY parce qu'elles ne font pas la meme chose : l'une
    // ecrit une ligne que personne ne lira peut-etre jamais, l'autre reveille
    // quelqu'un. Les confondre ferait envoyer une alerte a chaque trace de
    // mise au point.
    ALERT = 83,

    // Bloc « parallele ... avec ... finparallele » (6 oct. 2026). La forme
    // compilee est : FORK Lb ; [branche 1] ; JOIN ; JMP fin ; Lb: [branche 2] ;
    // ENDBRANCH ; fin:
    //
    // FORK demande a l'hote de faire tourner la branche 2 (a partir de Lb)
    // dans une seconde machine. S'il refuse -- aucune place libre --, cette
    // machine execute la branche 2 elle-meme, puis revient juste apres FORK :
    // les deux branches s'enchainent au lieu de partir ensemble, mais aucune
    // n'est perdue. Un seul retour en attente : pas de bloc parallele imbrique
    // (refuse par le compilateur).
    FORK = 84,       // + u16 : debut de la branche 2
    JOIN = 85,       // rend la main tant que la branche 2 tourne ailleurs
    ENDBRANCH = 86,  // fin de branche 2 : termine la machine-branche, ou
                     // revient apres FORK quand la branche a ete faite ici
    // Bloc « parallele ou » (8 oct. 2026) : la suite reprend des que l'UNE
    // des deux branches est finie, l'autre continue. Forme compilee :
    // FORK L1 ; FORK L2 ; JOINANY ; JMP fin ; L1: [1] ENDBRANCH ;
    // L2: [2] ENDBRANCH ; fin: -- et un JOIN avant le HALT final, pour que
    // la branche restante ne soit pas arretee avec le script. Chaque FORK
    // accepte rend la main, pour que l'hote demarre la branche avant le
    // FORK suivant. Une branche faite sur place (pas de place libre) compte
    // comme finie.
    JOINANY = 87,    // rend la main tant qu'aucune branche du bloc n'est finie
};

// Actions demandees a l'hote. Le script DEMANDE, l'hote dispose.
enum class ScriptAction : uint8_t {
    ZONE_START = 1,   // argument : duree en secondes
    ZONE_STOP = 2,    // arret franc, le reliquat est perdu
    ZONE_PAUSE = 3,   // met en pause, le reliquat est conserve
    ZONE_RESUME = 4,  // reprend le reliquat conserve
    SET_OUTPUT = 5,   // argument : 0/1 -- pompe, eclairage, auxiliaire
    SCRIPT_RUN = 6,   // cible : numero d'emplacement (1..6), argument ignore
};

enum class ScriptStatus : uint8_t {
    READY = 0,     // pret a demarrer
    RUNNING = 1,   // budget du tour epuise, reprendra au tour suivant
    WAITING = 2,   // en attente d'echeance, ne consomme rien
    FINISHED = 3,  // HALT atteint
    ABORTED = 4,   // arrete par le chien de garde ou par un programme invalide
};

enum class ScriptAbort : uint8_t {
    NONE = 0,
    STEP_LIMIT,        // trop d'instructions au total
    LOOP_LIMIT,        // trop d'iterations : boucle vraisemblablement infinie
    TIME_LIMIT,        // trop longtemps en execution
    STACK_OVERFLOW,
    STACK_UNDERFLOW,
    BAD_OPCODE,
    BAD_JUMP,
    BAD_VARIABLE,
    DIVIDE_BY_ZERO,
    HOST_REFUSED,      // l'hote a refuse l'action demandee
};

const char* scriptAbortName(ScriptAbort reason);

// Plafonds du chien de garde. Valeurs par defaut volontairement basses : un
// script d'arrosage qui les approche fait probablement autre chose que ce
// que son auteur croit.
struct ScriptLimits {
    uint16_t stepsPerTick;      // instructions par tour de boucle
    uint32_t maxTotalSteps;     // instructions pour toute l'execution
    uint32_t maxLoopIterations; // sauts arriere cumules
    uint32_t maxRunMs;          // duree totale, attentes comprises

    constexpr ScriptLimits()
        : stepsPerTick(200U), maxTotalSteps(200000UL),
          maxLoopIterations(100000UL), maxRunMs(6UL * 3600UL * 1000UL) {}
};

// Interface vers le monde reel. Meme forme que les autres pilotes du projet
// (BinaryActuatorDriverOps) : des pointeurs de fonction et un contexte, pour
// que la machine reste testable sans materiel.
struct ScriptHostOps {
    bool (*readInput)(void* ctx, uint16_t equipmentId, int32_t& value);
    bool (*zoneActive)(void* ctx, uint16_t zoneId, int32_t& value);
    bool (*zoneRemainingSec)(void* ctx, uint16_t zoneId, int32_t& value);
    bool (*action)(void* ctx, ScriptAction action, uint16_t target, int32_t arg);
    bool (*notify)(void* ctx, uint16_t messageCode);
    // Retourne false si les notifications ne sont pas configurees : le script
    // s'arrete alors sur « action refusee » plutot que de croire avoir
    // prevenu quelqu'un.
    bool (*alert)(void* ctx, uint16_t messageCode);
    uint32_t (*nowMs)(void* ctx);
    // Bloc parallele. fork() : true si l'hote fera tourner la branche qui
    // commence a `pc` dans une autre machine ; false pour que celle-ci la
    // fasse elle-meme. branchesRunning() : branches encore en cours ailleurs.
    // Absents (nullptr) : FORK execute toujours la branche sur place.
    bool (*fork)(void* ctx, uint16_t pc);
    uint8_t (*branchesRunning)(void* ctx);
    // Rendez-vous « ou » et cloture de bloc. Retourne true -- et clot le bloc
    // en cours, dont les branches restantes ne compteront plus -- si une
    // branche du bloc est finie ou si inlineDone ; false sinon. Appele avec
    // inlineDone=true au passage d'un JOIN, pour clore le bloc « et » : ses
    // branches ne doivent pas debloquer un bloc « ou » suivant. Absent
    // (nullptr) : JOINANY attend toutes les branches, comme JOIN.
    bool (*joinAny)(void* ctx, bool inlineDone);
    // Variables globales. Absents (nullptr) ou refus : arret BAD_VARIABLE.
    bool (*globalGet)(void* ctx, uint8_t index, int32_t& value);
    bool (*globalSet)(void* ctx, uint8_t index, int32_t value);
};

struct ScriptProgram {
    const uint8_t* code;
    uint16_t size;

    constexpr ScriptProgram() : code(nullptr), size(0U) {}
    constexpr ScriptProgram(const uint8_t* bytes, uint16_t length)
        : code(bytes), size(length) {}
};

// Verification d un programme AVANT de l accepter.
//
// Le bytecode est compile par le navigateur : il arrive donc du dehors, et
// le module ne doit pas lui faire confiance. Un octet errant suffirait a
// faire sauter l execution au milieu d une instruction, ou hors du
// programme.
//
// La machine se defend deja a l execution -- opcode inconnu, saut hors
// bornes -- mais decouvrir cela pendant un arrosage vaut moins bien que
// le refuser a l enregistrement, ou l utilisateur est devant son ecran.
//
// Retourne ScriptAbort::NONE si le programme tient debout.
ScriptAbort validateScriptProgram(const ScriptProgram& program);

class ScriptVm {
public:
    static constexpr uint8_t STACK_CAPACITY = 24U;
    static constexpr uint8_t VAR_COUNT = 8U;
    static constexpr uint8_t GLOBAL_COUNT = 16U;   // = ScriptGlobals::COUNT

    ScriptVm() = default;

    void load(const ScriptProgram& program,
              const ScriptHostOps* host,
              void* hostContext,
              const ScriptLimits& limits = ScriptLimits());

    // Execute au plus limits.stepsPerTick instructions puis rend la main.
    // A appeler a chaque tour de boucle tant que le statut est RUNNING ou
    // WAITING : c'est cet appel repete, et non une boucle interne, qui fait
    // avancer le script.
    ScriptStatus tick();

    // Fait de cette machine celle d'une branche 2 de bloc parallele : elle
    // part de `pc` et s'arrete (FINISHED) sur ENDBRANCH. A appeler juste
    // apres load().
    void startBranch(uint16_t pc) { _pc = pc; _isBranch = true; }

    ScriptStatus status() const { return _status; }
    ScriptAbort abortReason() const { return _abort; }
    uint16_t programCounter() const { return _pc; }
    uint32_t stepsUsed() const { return _steps; }
    uint32_t loopsUsed() const { return _loops; }
    // Lecture des variables : sert aux autotests et au diagnostic d un
    // script arrete, ou l etat final est la seule trace de ce qui s est passe.
    int32_t variable(uint8_t i) const { return i < VAR_COUNT ? _vars[i] : 0; }

private:
    bool push(int32_t v);
    bool pop(int32_t& v);
    bool fetch8(uint8_t& v);
    bool fetch16(uint16_t& v);
    bool fetch32(int32_t& v);
    void fail(ScriptAbort reason);

    ScriptProgram _program;
    const ScriptHostOps* _host = nullptr;
    void* _hostCtx = nullptr;
    ScriptLimits _limits;

    int32_t _stack[STACK_CAPACITY] = {};
    int32_t _vars[VAR_COUNT] = {};
    uint8_t _sp = 0U;
    uint16_t _pc = 0U;

    uint32_t _steps = 0U;
    uint32_t _loops = 0U;
    uint32_t _startedMs = 0U;
    uint32_t _wakeAtMs = 0U;

    // Retour apres une branche 2 executee sur place (FORK refuse), sinon
    // NO_RETURN. Et vrai pour une machine-branche (startBranch()).
    static constexpr uint16_t NO_RETURN = 0xFFFFU;
    // Frequence a laquelle JOIN revient voir si la branche 2 est finie.
    static constexpr uint32_t JOIN_POLL_MS = 250U;
    uint16_t _returnPc = NO_RETURN;
    bool _isBranch = false;
    // Une branche a ete faite sur place depuis le dernier rendez-vous : pour
    // JOINANY, elle est finie.
    bool _inlineDone = false;

    ScriptStatus _status = ScriptStatus::READY;
    ScriptAbort _abort = ScriptAbort::NONE;
};

}} // namespace AquaLook::Domain
