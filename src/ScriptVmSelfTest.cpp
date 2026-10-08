#include "ScriptVmSelfTest.h"

#include <Arduino.h>

#include "InputSampler.h"
#include "domain/ScriptVm.h"

// Autotest de la machine a scripts, execute SUR LA CIBLE.
//
// Il n'y a pas de compilateur hote dans cet environnement : la machine ne
// peut donc pas etre validee sur un poste avant d'arriver ici. Elle l'est
// donc la ou elle tournera, et le resultat est lisible par HTTP -- meme
// principe que /api/debug/sd-selftest.
//
// Le cas qui justifie a lui seul cet autotest est le n°4 : une boucle sans
// fin doit rendre la main et etre SIGNALEE, jamais figer le module. Un
// module qui gele avec une vanne ouverte n'est pas une panne, c'est un
// degat des eaux.

namespace {

using namespace AquaLook::Domain;

struct FakeHost {
    uint32_t nowMs = 0U;
    int32_t inputValue = 0;
    uint16_t lastNotify = 0U;
    uint16_t lastAlert = 0U;
    uint8_t actionCount = 0U;
    ScriptAction lastAction = ScriptAction::ZONE_STOP;
    uint16_t lastTarget = 0U;
    int32_t lastArg = 0;
    bool refuseActions = false;
    // Bloc parallele : l'hote jouet accepte la branche 2 seulement si
    // acceptFork, et dit qu'elle tourne tant que branches > 0.
    bool acceptFork = false;
    uint16_t forkPc = 0U;
    uint8_t branches = 0U;
    // Rendez-vous « ou » : forks acceptes, et une branche du bloc finie.
    uint8_t forks = 0U;
    bool anyDone = false;
    // Variables globales jouets (le vrai magasin est ScriptGlobals, en NVS).
    int32_t globals[16] = {};
    uint8_t lastNotifyVar = 0xFFU;
    uint8_t lastAlertVar = 0xFFU;
};

bool hostReadInput(void* ctx, uint16_t, int32_t& v) {
    v = static_cast<FakeHost*>(ctx)->inputValue;
    return true;
}
bool hostZoneActive(void* ctx, uint16_t, int32_t& v) {
    (void)ctx; v = 1; return true;
}
bool hostZoneRemain(void* ctx, uint16_t, int32_t& v) {
    (void)ctx; v = 120; return true;
}
bool hostAction(void* ctx, ScriptAction a, uint16_t target, int32_t arg) {
    FakeHost* h = static_cast<FakeHost*>(ctx);
    if (h->refuseActions) return false;
    h->actionCount++;
    h->lastAction = a;
    h->lastTarget = target;
    h->lastArg = arg;
    return true;
}
bool hostNotify(void* ctx, uint16_t code) {
    static_cast<FakeHost*>(ctx)->lastNotify = code;
    return true;
}
// L'hote jouet accepte l'alerte : le refus quand les notifications ne sont
// pas configurees se teste sur le vrai hote, pas ici.
bool hostAlert(void* ctx, uint16_t code) {
    static_cast<FakeHost*>(ctx)->lastAlert = code;
    return true;
}
uint32_t hostNow(void* ctx) { return static_cast<FakeHost*>(ctx)->nowMs; }
bool hostFork(void* ctx, uint16_t pc) {
    FakeHost* h = static_cast<FakeHost*>(ctx);
    if (!h->acceptFork) return false;
    h->forkPc = pc;
    h->branches = 1U;
    h->forks++;
    return true;
}
uint8_t hostBranches(void* ctx) { return static_cast<FakeHost*>(ctx)->branches; }
bool hostJoinAny(void* ctx, bool inlineDone) {
    FakeHost* h = static_cast<FakeHost*>(ctx);
    if (!inlineDone && !h->anyDone) return false;
    h->anyDone = false;
    return true;
}

bool hostGlobalGet(void* ctx, uint8_t i, int32_t& v) {
    if (i >= 16U) return false;
    v = static_cast<FakeHost*>(ctx)->globals[i];
    return true;
}
bool hostGlobalSet(void* ctx, uint8_t i, int32_t v) {
    if (i >= 16U) return false;
    static_cast<FakeHost*>(ctx)->globals[i] = v;
    return true;
}

bool hostNotifyVar(void* ctx, uint16_t code, uint8_t i) {
    FakeHost* h = static_cast<FakeHost*>(ctx);
    h->lastNotify = code;
    h->lastNotifyVar = i;
    return true;
}

bool hostAlertVar(void* ctx, uint16_t code, uint8_t i) {
    FakeHost* h = static_cast<FakeHost*>(ctx);
    h->lastAlert = code;
    h->lastAlertVar = i;
    return true;
}

const ScriptHostOps HOST_OPS = {
    hostReadInput, hostZoneActive, hostZoneRemain,
    hostAction, hostNotify, hostAlert, hostNow,
    hostFork, hostBranches, hostJoinAny, hostGlobalGet, hostGlobalSet, hostNotifyVar,
    hostAlertVar
};

// Bloc parallele assemble comme script-lang.js le compile :
//   FORK Lb ; [1] var0 = 1 ; JOIN ; JMP fin ; Lb: [2] var1 = var0*10 + 2 ;
//   ENDBRANCH ; fin: HALT
// Le calcul de la branche 2 revele l'ORDRE d'execution : var1 vaut 2 si la
// branche 2 passe avant la branche 1 (repli sur place), 12 si apres.
struct ParallelProgram {
    uint8_t code[64] = {};
    uint16_t n = 0U;
    uint16_t branch2 = 0U;
};
ParallelProgram assembleParallel() {
    ParallelProgram p;
    uint8_t* c = p.code;
    uint16_t& n = p.n;
    auto u8 = [&](uint8_t v) { c[n++] = v; };
    auto u16 = [&](uint16_t v) { c[n++] = (uint8_t)(v & 0xFF); c[n++] = (uint8_t)(v >> 8); };
    auto push = [&](int32_t v) {
        u8((uint8_t)ScriptOp::PUSH);
        const uint32_t r = (uint32_t)v;
        for (uint8_t i = 0U; i < 4U; ++i) u8((uint8_t)((r >> (8U * i)) & 0xFFU));
    };
    u8((uint8_t)ScriptOp::FORK); const uint16_t forkAt = n; u16(0);
    push(1); u8((uint8_t)ScriptOp::STORE); u8(0);
    u8((uint8_t)ScriptOp::JOIN);
    u8((uint8_t)ScriptOp::JMP); const uint16_t jmpAt = n; u16(0);
    p.branch2 = n;
    u8((uint8_t)ScriptOp::LOAD); u8(0); push(10); u8((uint8_t)ScriptOp::MUL);
    push(2); u8((uint8_t)ScriptOp::ADD); u8((uint8_t)ScriptOp::STORE); u8(1);
    u8((uint8_t)ScriptOp::ENDBRANCH);
    const uint16_t fin = n;
    u8((uint8_t)ScriptOp::HALT);
    c[forkAt] = (uint8_t)(p.branch2 & 0xFF); c[forkAt + 1] = (uint8_t)(p.branch2 >> 8);
    c[jmpAt] = (uint8_t)(fin & 0xFF);        c[jmpAt + 1] = (uint8_t)(fin >> 8);
    return p;
}


// Bloc « parallele ou » assemble comme script-lang.js le compile :
//   FORK L1 ; FORK L2 ; JOINANY ; JMP fin ; L1: var0 = 1 ; ENDBRANCH ;
//   L2: var1 = 2 ; ENDBRANCH ; fin: var2 = 7 ; JOIN ; HALT
ParallelProgram assembleParallelOu() {
    ParallelProgram p;
    uint8_t* c = p.code;
    uint16_t& n = p.n;
    auto u8 = [&](uint8_t v) { c[n++] = v; };
    auto u16 = [&](uint16_t v) { c[n++] = (uint8_t)(v & 0xFF); c[n++] = (uint8_t)(v >> 8); };
    auto push = [&](int32_t v) {
        u8((uint8_t)ScriptOp::PUSH);
        const uint32_t r = (uint32_t)v;
        for (uint8_t i = 0U; i < 4U; ++i) u8((uint8_t)((r >> (8U * i)) & 0xFFU));
    };
    u8((uint8_t)ScriptOp::FORK); const uint16_t f1 = n; u16(0);
    u8((uint8_t)ScriptOp::FORK); const uint16_t f2 = n; u16(0);
    u8((uint8_t)ScriptOp::JOINANY);
    u8((uint8_t)ScriptOp::JMP); const uint16_t jmpAt = n; u16(0);
    const uint16_t l1 = n;
    push(1); u8((uint8_t)ScriptOp::STORE); u8(0); u8((uint8_t)ScriptOp::ENDBRANCH);
    p.branch2 = n;
    push(2); u8((uint8_t)ScriptOp::STORE); u8(1); u8((uint8_t)ScriptOp::ENDBRANCH);
    const uint16_t fin = n;
    push(7); u8((uint8_t)ScriptOp::STORE); u8(2);
    u8((uint8_t)ScriptOp::JOIN);
    u8((uint8_t)ScriptOp::HALT);
    c[f1] = (uint8_t)(l1 & 0xFF);         c[f1 + 1] = (uint8_t)(l1 >> 8);
    c[f2] = (uint8_t)(p.branch2 & 0xFF);  c[f2 + 1] = (uint8_t)(p.branch2 >> 8);
    c[jmpAt] = (uint8_t)(fin & 0xFF);     c[jmpAt + 1] = (uint8_t)(fin >> 8);
    return p;
}

// Petit assembleur, pour que les programmes de test restent lisibles.
struct Asm {
    uint8_t code[128] = {};
    uint16_t n = 0U;
    Asm& op(ScriptOp o) { code[n++] = static_cast<uint8_t>(o); return *this; }
    Asm& u8v(uint8_t v) { code[n++] = v; return *this; }
    Asm& u16v(uint16_t v) {
        code[n++] = static_cast<uint8_t>(v & 0xFFU);
        code[n++] = static_cast<uint8_t>((v >> 8) & 0xFFU);
        return *this;
    }
    Asm& i32v(int32_t v) {
        const uint32_t raw = static_cast<uint32_t>(v);
        for (uint8_t i = 0U; i < 4U; ++i) code[n++] = static_cast<uint8_t>((raw >> (8U * i)) & 0xFFU);
        return *this;
    }
    Asm& push(int32_t v) { return op(ScriptOp::PUSH).i32v(v); }
    ScriptProgram program() const { return ScriptProgram(code, n); }
};

// Fait tourner la machine au plus maxTicks fois. Retourne le nombre de tours
// consommes : c'est LUI qui prouve que l'execution est fractionnee.
uint32_t run(ScriptVm& vm, uint32_t maxTicks, FakeHost* host = nullptr,
             uint32_t msPerTick = 0U) {
    uint32_t ticks = 0U;
    while (ticks < maxTicks) {
        const ScriptStatus st = vm.tick();
        ticks++;
        if (st == ScriptStatus::FINISHED || st == ScriptStatus::ABORTED) break;
        if (host) host->nowMs += msPerTick;
    }
    return ticks;
}

// Faux capteur : une broche que l'autotest fait claqueter a volonte.
bool g_pin = false;
RelayTopology::RelayTopologyConfig g_fakeTopology;

bool fakePinReader(uint16_t, bool& active) { active = g_pin; return true; }

void prepareFakeTopology() {
    RelayTopology::clear(g_fakeTopology);
    RelayTopology::RelayAssignment& in = g_fakeTopology.assignments[0];
    in.enabled = true;
    in.role = RelayTopology::ROLE_INPUT_LEVEL;
    in.direction = RelayTopology::DIRECTION_INPUT;
    in.id = 600U;
}

void record(JsonArray& out, const char* nom, bool ok, const char* detail) {
    JsonObject o = out.add<JsonObject>();
    o["cas"] = nom;
    o["ok"] = ok;
    o["detail"] = detail;
}

} // namespace

bool runScriptVmSelfTest(JsonDocument& doc) {
    using namespace AquaLook::Domain;

    prepareFakeTopology();
    JsonArray cases = doc["cas"].to<JsonArray>();
    uint8_t passed = 0U, total = 0U;
    char detail[64];

    // 1. Arithmetique et fin normale : 2 + 3 * 4 = 14 dans la variable 0.
    {
        total++;
        FakeHost h;
        Asm a;
        a.push(2).push(3).push(4).op(ScriptOp::MUL).op(ScriptOp::ADD)
         .op(ScriptOp::STORE).u8v(0).op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 10U);
        const bool ok = vm.status() == ScriptStatus::FINISHED && vm.variable(0) == 14;
        snprintf(detail, sizeof(detail), "var0=%ld", (long)vm.variable(0));
        record(cases, "arithmetique et HALT", ok, detail);
        if (ok) passed++;
    }

    // 2. si / sinon : l'entree vaut 1, la branche vraie doit etre prise.
    {
        total++;
        FakeHost h; h.inputValue = 1;
        Asm a;
        a.op(ScriptOp::READ_INPUT).u16v(7);
        const uint16_t jz = a.n; a.op(ScriptOp::JZ).u16v(0);
        a.push(100).op(ScriptOp::STORE).u8v(0);
        const uint16_t jmp = a.n; a.op(ScriptOp::JMP).u16v(0);
        const uint16_t elseAt = a.n;
        a.push(200).op(ScriptOp::STORE).u8v(0);
        const uint16_t endAt = a.n;
        a.op(ScriptOp::HALT);
        a.code[jz + 1] = static_cast<uint8_t>(elseAt & 0xFF);
        a.code[jz + 2] = static_cast<uint8_t>(elseAt >> 8);
        a.code[jmp + 1] = static_cast<uint8_t>(endAt & 0xFF);
        a.code[jmp + 2] = static_cast<uint8_t>(endAt >> 8);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 20U);
        const bool ok = vm.status() == ScriptStatus::FINISHED && vm.variable(0) == 100;
        snprintf(detail, sizeof(detail), "var0=%ld (attendu 100)", (long)vm.variable(0));
        record(cases, "si / sinon", ok, detail);
        if (ok) passed++;
    }

    // 3. Boucle bornee : var0 compte de 0 a 5.
    {
        total++;
        FakeHost h;
        Asm a;
        const uint16_t topAt = a.n;
        a.op(ScriptOp::LOAD).u8v(0).push(5).op(ScriptOp::LT);
        const uint16_t jz = a.n; a.op(ScriptOp::JZ).u16v(0);
        a.op(ScriptOp::LOAD).u8v(0).push(1).op(ScriptOp::ADD).op(ScriptOp::STORE).u8v(0);
        a.op(ScriptOp::JMP).u16v(topAt);
        const uint16_t endAt = a.n;
        a.op(ScriptOp::HALT);
        a.code[jz + 1] = static_cast<uint8_t>(endAt & 0xFF);
        a.code[jz + 2] = static_cast<uint8_t>(endAt >> 8);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 50U);
        const bool ok = vm.status() == ScriptStatus::FINISHED &&
                        vm.variable(0) == 5 && vm.loopsUsed() == 5U;
        snprintf(detail, sizeof(detail), "var0=%ld tours=%lu",
                 (long)vm.variable(0), (unsigned long)vm.loopsUsed());
        record(cases, "boucle bornee (for)", ok, detail);
        if (ok) passed++;
    }

    // 4. BOUCLE SANS FIN -- le cas qui justifie tout le reste.
    //    Elle doit s'arreter d'elle-meme, sur LOOP_LIMIT, sans figer.
    {
        total++;
        FakeHost h;
        Asm a;
        a.op(ScriptOp::JMP).u16v(0);          // saute sur lui-meme
        ScriptLimits lim;
        lim.stepsPerTick = 50U;
        lim.maxLoopIterations = 500U;
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h, lim);
        const uint32_t ticks = run(vm, 1000U);
        const bool ok = vm.status() == ScriptStatus::ABORTED &&
                        vm.abortReason() == ScriptAbort::LOOP_LIMIT &&
                        ticks > 1U;   // fractionnee, donc non bloquante
        snprintf(detail, sizeof(detail), "arret=%s apres %lu tours",
                 scriptAbortName(vm.abortReason()), (unsigned long)ticks);
        record(cases, "boucle sans fin arretee", ok, detail);
        if (ok) passed++;
    }

    // 5. Le budget par tour est respecte : 50 instructions par tour, donc
    //    une boucle longue prend plusieurs tours. C'est la preuve du
    //    caractere non bloquant.
    {
        total++;
        FakeHost h;
        Asm a;
        const uint16_t topAt = a.n;
        a.op(ScriptOp::LOAD).u8v(0).push(60).op(ScriptOp::LT);
        const uint16_t jz = a.n; a.op(ScriptOp::JZ).u16v(0);
        a.op(ScriptOp::LOAD).u8v(0).push(1).op(ScriptOp::ADD).op(ScriptOp::STORE).u8v(0);
        a.op(ScriptOp::JMP).u16v(topAt);
        const uint16_t endAt = a.n;
        a.op(ScriptOp::HALT);
        a.code[jz + 1] = static_cast<uint8_t>(endAt & 0xFF);
        a.code[jz + 2] = static_cast<uint8_t>(endAt >> 8);
        ScriptLimits lim; lim.stepsPerTick = 50U;
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h, lim);
        const uint32_t ticks = run(vm, 200U);
        const bool ok = vm.status() == ScriptStatus::FINISHED && ticks > 1U;
        snprintf(detail, sizeof(detail), "%lu tours pour %lu instructions",
                 (unsigned long)ticks, (unsigned long)vm.stepsUsed());
        record(cases, "budget par tour respecte", ok, detail);
        if (ok) passed++;
    }

    // 6. L'attente rend la main sans consommer d'instructions.
    {
        total++;
        FakeHost h;
        Asm a;
        a.push(2).op(ScriptOp::WAIT).push(42).op(ScriptOp::STORE).u8v(0).op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        vm.tick();
        const bool waiting = vm.status() == ScriptStatus::WAITING;
        const uint32_t stepsWhileWaiting = vm.stepsUsed();
        vm.tick();                       // toujours en attente
        const bool stillIdle = vm.stepsUsed() == stepsWhileWaiting;
        h.nowMs += 2500U;                // echeance depassee
        run(vm, 10U);
        const bool ok = waiting && stillIdle &&
                        vm.status() == ScriptStatus::FINISHED && vm.variable(0) == 42;
        snprintf(detail, sizeof(detail), "attente=%d inerte=%d var0=%ld",
                 waiting ? 1 : 0, stillIdle ? 1 : 0, (long)vm.variable(0));
        record(cases, "attente non bloquante", ok, detail);
        if (ok) passed++;
    }

    // 7. Debordement de pile.
    {
        total++;
        FakeHost h;
        Asm a;
        for (uint8_t i = 0U; i < 25U; ++i) a.push(1);
        a.op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 20U);
        const bool ok = vm.status() == ScriptStatus::ABORTED &&
                        vm.abortReason() == ScriptAbort::STACK_OVERFLOW;
        snprintf(detail, sizeof(detail), "arret=%s", scriptAbortName(vm.abortReason()));
        record(cases, "pile pleine", ok, detail);
        if (ok) passed++;
    }

    // 8. Instruction inconnue : un programme abime ne doit pas partir en vrille.
    {
        total++;
        FakeHost h;
        Asm a;
        a.u8v(0xEE).op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 10U);
        const bool ok = vm.status() == ScriptStatus::ABORTED &&
                        vm.abortReason() == ScriptAbort::BAD_OPCODE;
        snprintf(detail, sizeof(detail), "arret=%s", scriptAbortName(vm.abortReason()));
        record(cases, "instruction inconnue", ok, detail);
        if (ok) passed++;
    }

    // 9. Division par zero.
    {
        total++;
        FakeHost h;
        Asm a;
        a.push(1).push(0).op(ScriptOp::DIV).op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 10U);
        const bool ok = vm.status() == ScriptStatus::ABORTED &&
                        vm.abortReason() == ScriptAbort::DIVIDE_BY_ZERO;
        snprintf(detail, sizeof(detail), "arret=%s", scriptAbortName(vm.abortReason()));
        record(cases, "division par zero", ok, detail);
        if (ok) passed++;
    }

    // 10. Le scenario de la cuve, en reduction : si l'entree dit "vide",
    //     on arrete la zone et on previent. C'est le cas d'usage reel.
    {
        total++;
        FakeHost h; h.inputValue = 1;    // 1 = cuve vide
        Asm a;
        a.op(ScriptOp::READ_INPUT).u16v(31);
        const uint16_t jz = a.n; a.op(ScriptOp::JZ).u16v(0);
        a.push(0).op(ScriptOp::ACTION).u8v(static_cast<uint8_t>(ScriptAction::ZONE_PAUSE)).u16v(5);
        a.op(ScriptOp::NOTIFY).u16v(900);
        const uint16_t endAt = a.n;
        a.op(ScriptOp::HALT);
        a.code[jz + 1] = static_cast<uint8_t>(endAt & 0xFF);
        a.code[jz + 2] = static_cast<uint8_t>(endAt >> 8);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 20U);
        const bool ok = vm.status() == ScriptStatus::FINISHED &&
                        h.actionCount == 1U &&
                        h.lastAction == ScriptAction::ZONE_PAUSE &&
                        h.lastTarget == 5U && h.lastNotify == 900U;
        snprintf(detail, sizeof(detail), "actions=%u cible=%u notif=%u",
                 (unsigned)h.actionCount, (unsigned)h.lastTarget, (unsigned)h.lastNotify);
        record(cases, "scenario cuve vide", ok, detail);
        if (ok) passed++;
    }

    // 11. L'hote peut refuser : le script s'arrete, il ne passe pas outre.
    {
        total++;
        FakeHost h; h.refuseActions = true;
        Asm a;
        a.push(60).op(ScriptOp::ACTION).u8v(static_cast<uint8_t>(ScriptAction::ZONE_START)).u16v(1);
        a.op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 10U);
        const bool ok = vm.status() == ScriptStatus::ABORTED &&
                        vm.abortReason() == ScriptAbort::HOST_REFUSED;
        snprintf(detail, sizeof(detail), "arret=%s", scriptAbortName(vm.abortReason()));
        record(cases, "action refusee par l hote", ok, detail);
        if (ok) passed++;
    }

    // 11b. Bloc parallele sans place libre : la branche 2 est faite sur
    // place, AVANT la branche 1, puis la machine revient -- rien n'est perdu.
    {
        total++;
        FakeHost h;   // acceptFork = false
        const ParallelProgram p = assembleParallel();
        const ScriptProgram prog(p.code, p.n);
        ScriptVm vm;
        vm.load(prog, &HOST_OPS, &h);
        run(vm, 20U);
        const bool ok = validateScriptProgram(prog) == ScriptAbort::NONE &&
                        vm.status() == ScriptStatus::FINISHED &&
                        vm.variable(0) == 1 && vm.variable(1) == 2;
        snprintf(detail, sizeof(detail), "var0=%ld var1=%ld (attendu 1 et 2)",
                 (long)vm.variable(0), (long)vm.variable(1));
        record(cases, "parallele : repli sur place", ok, detail);
        if (ok) passed++;
    }

    // 11c. Bloc parallele avec place : la machine fait la branche 1, attend
    // sur JOIN tant que la branche 2 tourne ailleurs, puis termine sans
    // refaire la branche 2. Et une machine-branche ne fait QUE la branche 2.
    {
        total++;
        FakeHost h; h.acceptFork = true;
        const ParallelProgram p = assembleParallel();
        ScriptVm vm;
        vm.load(ScriptProgram(p.code, p.n), &HOST_OPS, &h);
        run(vm, 5U, &h, 100U);
        const bool waited = vm.status() == ScriptStatus::WAITING && vm.variable(0) == 1 &&
                            h.forkPc == p.branch2;
        h.branches = 0U;
        run(vm, 10U, &h, 300U);
        const bool joined = vm.status() == ScriptStatus::FINISHED && vm.variable(1) == 0;

        FakeHost hb;
        ScriptVm branch;
        branch.load(ScriptProgram(p.code, p.n), &HOST_OPS, &hb);
        branch.startBranch(p.branch2);
        run(branch, 10U);
        const bool branchOk = branch.status() == ScriptStatus::FINISHED &&
                              branch.variable(1) == 2 && branch.variable(0) == 0;
        const bool ok = waited && joined && branchOk;
        snprintf(detail, sizeof(detail), "attente=%d rendez-vous=%d branche=%d",
                 waited ? 1 : 0, joined ? 1 : 0, branchOk ? 1 : 0);
        record(cases, "parallele : attente et branche", ok, detail);
        if (ok) passed++;
    }

    // 11f. Variables globales : g4 = g4 + 5 lit et ecrit chez l'hote, et
    // une globale hors bornes (n°16, soit g17) est refusee au validateur.
    {
        total++;
        FakeHost h; h.globals[3] = 10;
        Asm a;
        a.op(ScriptOp::GLOAD).u8v(3).push(5).op(ScriptOp::ADD).op(ScriptOp::GSTORE).u8v(3)
         .op(ScriptOp::GLOAD).u8v(3).op(ScriptOp::STORE).u8v(0).op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 10U);
        Asm bad;
        bad.op(ScriptOp::GLOAD).u8v(16).op(ScriptOp::HALT);
        const bool refuse = validateScriptProgram(bad.program()) == ScriptAbort::BAD_VARIABLE;
        const bool ok = validateScriptProgram(a.program()) == ScriptAbort::NONE &&
                        vm.status() == ScriptStatus::FINISHED && h.globals[3] == 15 &&
                        vm.variable(0) == 15 && refuse;
        snprintf(detail, sizeof(detail), "g4=%ld var0=%ld refus g17=%d",
                 (long)h.globals[3], (long)vm.variable(0), refuse ? 1 : 0);
        record(cases, "variables globales", ok, detail);
        if (ok) passed++;
    }

    // 11g. « message 900 avec g4 » : l'hote recoit le code ET le numero de
    // la variable ; une globale hors bornes est refusee au validateur.
    {
        total++;
        FakeHost h;
        Asm a;
        a.op(ScriptOp::NOTIFY_VAR).u16v(900).u8v(3).op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 5U);
        Asm bad;
        bad.op(ScriptOp::NOTIFY_VAR).u16v(900).u8v(16).op(ScriptOp::HALT);
        const bool refuse = validateScriptProgram(bad.program()) == ScriptAbort::BAD_VARIABLE;
        const bool ok = validateScriptProgram(a.program()) == ScriptAbort::NONE &&
                        vm.status() == ScriptStatus::FINISHED && h.lastNotify == 900U &&
                        h.lastNotifyVar == 3U && refuse;
        snprintf(detail, sizeof(detail), "code=%u var=%u refus g17=%d",
                 (unsigned)h.lastNotify, (unsigned)h.lastNotifyVar, refuse ? 1 : 0);
        record(cases, "journal avec variable", ok, detail);
        if (ok) passed++;
    }

    // 11h. « notifier 901 avec g2 » : l'hote recoit code et numero.
    {
        total++;
        FakeHost h;
        Asm a;
        a.op(ScriptOp::ALERT_VAR).u16v(901).u8v(1).op(ScriptOp::HALT);
        ScriptVm vm;
        vm.load(a.program(), &HOST_OPS, &h);
        run(vm, 5U);
        const bool ok = validateScriptProgram(a.program()) == ScriptAbort::NONE &&
                        vm.status() == ScriptStatus::FINISHED && h.lastAlert == 901U &&
                        h.lastAlertVar == 1U;
        snprintf(detail, sizeof(detail), "code=%u var=%u",
                 (unsigned)h.lastAlert, (unsigned)h.lastAlertVar);
        record(cases, "notification avec variable", ok, detail);
        if (ok) passed++;
    }

    // 11d. « parallele ou » avec places : deux branches parties ailleurs,
    // la suite attend la PREMIERE finie, puis le HALT attend la restante.
    {
        total++;
        FakeHost h; h.acceptFork = true;
        const ParallelProgram p = assembleParallelOu();
        const ScriptProgram prog(p.code, p.n);
        ScriptVm vm;
        vm.load(prog, &HOST_OPS, &h);
        run(vm, 6U, &h, 100U);
        const bool waitAny = vm.status() == ScriptStatus::WAITING && h.forks == 2U &&
                             vm.variable(2) == 0 && vm.variable(0) == 0;
        h.anyDone = true;                 // une branche finie, l'autre tourne
        run(vm, 6U, &h, 300U);
        const bool suite = vm.variable(2) == 7 && vm.status() == ScriptStatus::WAITING;
        h.branches = 0U;                  // la restante finit
        run(vm, 6U, &h, 300U);
        const bool fini = vm.status() == ScriptStatus::FINISHED;
        const bool ok = validateScriptProgram(prog) == ScriptAbort::NONE && waitAny && suite && fini;
        snprintf(detail, sizeof(detail), "attente=%d suite=%d fin=%d forks=%u",
                 waitAny ? 1 : 0, suite ? 1 : 0, fini ? 1 : 0, (unsigned)h.forks);
        record(cases, "parallele ou : premiere finie", ok, detail);
        if (ok) passed++;
    }

    // 11e. « parallele ou » sans place : les deux branches faites sur place,
    // l'une apres l'autre, puis la suite -- rien n'est perdu ni bloque.
    {
        total++;
        FakeHost h;   // acceptFork = false
        const ParallelProgram p = assembleParallelOu();
        ScriptVm vm;
        vm.load(ScriptProgram(p.code, p.n), &HOST_OPS, &h);
        run(vm, 20U);
        const bool ok = vm.status() == ScriptStatus::FINISHED && vm.variable(0) == 1 &&
                        vm.variable(1) == 2 && vm.variable(2) == 7;
        snprintf(detail, sizeof(detail), "var0=%ld var1=%ld var2=%ld (attendu 1, 2, 7)",
                 (long)vm.variable(0), (long)vm.variable(1), (long)vm.variable(2));
        record(cases, "parallele ou : repli sur place", ok, detail);
        if (ok) passed++;
    }

    // ── Anti-rebond des entrees ─────────────────────────────────────────
    //
    // On ne peut pas faire claqueter un vrai flotteur depuis un autotest.
    // On fait donc claqueter la LECTURE, ce qui eprouve exactement la partie
    // qui doit resister : la regle de stabilite.

    // 12. Un contact qui claquette ne fait pas bouger la valeur officielle.
    {
        total++;
        g_pin = false;
        InputSampler s1;
        s1.begin(&g_fakeTopology, fakePinReader, false);
        uint32_t t = 1000U;
        // Etablir "inactif" au repos.
        for (uint8_t i = 0U; i < InputSampler::STABLE_SAMPLES + 2U; ++i) {
            s1.update(t); t += InputSampler::SAMPLE_MS;
        }
        bool v = true;
        const bool established = s1.read(600U, v) && v == false;
        // Puis 40 basculements, un par echantillon : le clapot type.
        for (uint8_t i = 0U; i < 40U; ++i) {
            g_pin = !g_pin;
            s1.update(t); t += InputSampler::SAMPLE_MS;
        }
        bool afterChatter = true;
        const bool stillStable = s1.read(600U, afterChatter) && afterChatter == false;
        const bool ok = established && stillStable && s1.transitions(600U) == 0U;
        snprintf(detail, sizeof(detail), "etabli=%d immobile=%d transitions=%lu",
                 established ? 1 : 0, stillStable ? 1 : 0,
                 (unsigned long)s1.transitions(600U));
        record(cases, "clapot ignore", ok, detail);
        if (ok) passed++;
    }

    // 13. Un changement DURABLE, lui, doit passer -- et une seule fois.
    {
        total++;
        g_pin = false;
        InputSampler s2;
        s2.begin(&g_fakeTopology, fakePinReader, false);
        uint32_t t = 1000U;
        for (uint8_t i = 0U; i < InputSampler::STABLE_SAMPLES + 2U; ++i) {
            s2.update(t); t += InputSampler::SAMPLE_MS;
        }
        g_pin = true;                       // la cuve se vide pour de bon
        for (uint8_t i = 0U; i < InputSampler::STABLE_SAMPLES + 2U; ++i) {
            s2.update(t); t += InputSampler::SAMPLE_MS;
        }
        bool v = false;
        const bool ok = s2.read(600U, v) && v == true && s2.transitions(600U) == 1U;
        snprintf(detail, sizeof(detail), "valeur=%d transitions=%lu",
                 v ? 1 : 0, (unsigned long)s2.transitions(600U));
        record(cases, "changement durable retenu", ok, detail);
        if (ok) passed++;
    }

    // 14. Carte muette : ne JAMAIS fabriquer un "inactif". Une cuve declaree
    //     pleine parce que la carte ne repond pas serait le pire des mensonges.
    {
        total++;
        InputSampler s3;
        s3.begin(&g_fakeTopology, [](uint16_t, bool&) { return false; });
        uint32_t t = 1000U;
        for (uint8_t i = 0U; i < InputSampler::STABLE_SAMPLES + 5U; ++i) {
            s3.update(t); t += InputSampler::SAMPLE_MS;
        }
        bool v = false;
        const bool ok = !s3.read(600U, v);   // rien d'etabli, donc rien d'affirme
        snprintf(detail, sizeof(detail), "lecture refusee=%d", ok ? 1 : 0);
        record(cases, "carte muette n invente rien", ok, detail);
        if (ok) passed++;
    }

    doc["reussis"] = passed;
    doc["total"] = total;
    doc["ok"] = (passed == total);
    return passed == total;
}
