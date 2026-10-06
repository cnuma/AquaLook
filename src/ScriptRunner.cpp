#include "ScriptRunner.h"

#include "EventLog.h"

#include <string.h>

using AquaLook::Domain::ScriptAbort;
using AquaLook::Domain::ScriptProgram;
using AquaLook::Domain::ScriptStatus;
using AquaLook::Domain::scriptAbortName;

void ScriptRunner::begin(const InputSampler* inputs, ScheduleManager* schedule,
                         const ConfigManager* config) {
    _inputs = inputs;
    _schedule = schedule;
    _config = config;
    for (uint8_t i = 0U; i < ScriptStore::MAX_SCRIPTS; ++i) {
        _seenTransitions[i] = 0U;
        _seenZoneActive[i] = false;
        _lastStartMs[i] = 0U;
        _lastAbort[i] = "";
        _lastAbortDetail[i][0] = '\0';
        _lastAbortPc[i] = 0U;
    }
    _primed = false;
}

uint8_t ScriptRunner::freeCount() const {
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < MAX_CONCURRENT; ++i) if (!_jobs[i].active) n++;
    return n;
}

int8_t ScriptRunner::freeSlot() const {
    for (uint8_t i = 0U; i < MAX_CONCURRENT; ++i) {
        if (!_jobs[i].active) return static_cast<int8_t>(i);
    }
    return -1;
}

bool ScriptRunner::isRunning(uint8_t index) const {
    for (uint8_t i = 0U; i < MAX_CONCURRENT; ++i) {
        if (_jobs[i].active && _jobs[i].index == index) return true;
    }
    return false;
}

uint8_t ScriptRunner::runningCount() const {
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < MAX_CONCURRENT; ++i) if (_jobs[i].active) n++;
    return n;
}

const char* ScriptRunner::lastAbort(uint8_t index) const {
    return index < ScriptStore::MAX_SCRIPTS ? _lastAbort[index] : "";
}

const char* ScriptRunner::lastAbortDetail(uint8_t index) const {
    return index < ScriptStore::MAX_SCRIPTS ? _lastAbortDetail[index] : "";
}

uint16_t ScriptRunner::lastAbortPc(uint8_t index) const {
    return index < ScriptStore::MAX_SCRIPTS ? _lastAbortPc[index] : 0U;
}

bool ScriptRunner::start(uint8_t index, const char*& reason) {
    reason = "";
    if (isRunning(index)) { reason = "deja en cours"; return false; }

    const int8_t slot = freeSlot();
    if (slot < 0) { reason = "aucune place libre"; return false; }

    Job& job = _jobs[slot];
    ScriptStore::Meta meta;
    if (!ScriptStore::load(index, meta, job.code, sizeof(job.code))) {
        reason = "emplacement vide";
        return false;
    }
    if (!meta.enabled) { reason = "script desactive"; return false; }

    job.ctx = ScriptRuntimeContext();
    job.ctx.inputs = _inputs;
    job.ctx.schedule = _schedule;
    job.ctx.config = _config;
    // Le nom vit dans la structure du magasin, qui est locale a cette
    // fonction : on le recopie dans le travail, qui, lui, survit au tick.
    strlcpy(job.name, meta.name, sizeof(job.name));
    job.ctx.name = job.name;
    job.ctx.selfIndex = index;
    job.index = index;
    job.isBranch = false;
    job.parentSlot = 0xFF;
    job.codeSize = meta.codeSize;
    job.active = true;
    job.vm.load(ScriptProgram(job.code, meta.codeSize), &scriptHostOps(), &job.ctx);

    _lastAbort[index] = "";
    _lastAbortDetail[index][0] = '\0';
    _lastAbortPc[index] = 0U;
    EventLog::log(LOG_INFO, "Script %u (%s) demarre", (unsigned)(index + 1U), meta.name);
    return true;
}

bool ScriptRunner::startBranch(uint8_t parentSlot, uint16_t pc) {
    const int8_t slot = freeSlot();
    if (slot < 0) return false;
    Job& parent = _jobs[parentSlot];
    Job& b = _jobs[slot];
    // Copie du bytecode : chaque travail possede son tampon, et le parent
    // peut se terminer (place reutilisee) pendant que la branche tourne.
    memcpy(b.code, parent.code, sizeof(b.code));
    b.codeSize = parent.codeSize;
    b.ctx = ScriptRuntimeContext();
    b.ctx.inputs = _inputs;
    b.ctx.schedule = _schedule;
    b.ctx.config = _config;
    strlcpy(b.name, parent.name, sizeof(b.name));
    b.ctx.name = b.name;
    b.ctx.selfIndex = parent.index;
    b.ctx.isBranch = true;
    b.index = parent.index;
    b.isBranch = true;
    b.parentSlot = parentSlot;
    b.active = true;
    b.vm.load(ScriptProgram(b.code, b.codeSize), &scriptHostOps(), &b.ctx);
    b.vm.startBranch(pc);
    EventLog::log(LOG_INFO, "Script %u : branche parallele demarree",
                  (unsigned)(parent.index + 1U));
    return true;
}

void ScriptRunner::stopBranches(uint8_t parentSlot, const char* why) {
    for (uint8_t t = 0U; t < MAX_CONCURRENT; ++t) {
        Job& b = _jobs[t];
        if (!b.active || !b.isBranch || b.parentSlot != parentSlot) continue;
        b.active = false;
        EventLog::log(LOG_WARN, "Script %u : branche parallele arretee (%s)",
                      (unsigned)(b.index + 1U), why);
    }
}

void ScriptRunner::launchRequested(uint8_t caller, uint8_t mask) {
    const uint32_t now = millis();
    for (uint8_t i = 0U; i < ScriptStore::MAX_SCRIPTS; ++i) {
        if (!(mask & (1U << i))) continue;
        // Meme garde que pour un declenchement : A qui lance B qui relance A
        // tournerait sans fin, chaque script finissant avant d'etre relance.
        if (_lastStartMs[i] != 0U && (now - _lastStartMs[i]) < MIN_RESTART_MS) {
            EventLog::log(LOG_WARN,
                          "Script %u : lancement du script %u refuse "
                          "(moins de %lus depuis son dernier depart)",
                          (unsigned)(caller + 1U), (unsigned)(i + 1U),
                          (unsigned long)(MIN_RESTART_MS / 1000UL));
            continue;
        }
        const char* reason = "";
        if (start(i, reason)) {
            _lastStartMs[i] = now;
            EventLog::log(LOG_INFO, "Script %u a lance le script %u",
                          (unsigned)(caller + 1U), (unsigned)(i + 1U));
        } else {
            EventLog::log(LOG_WARN, "Script %u : lancement du script %u refuse (%s)",
                          (unsigned)(caller + 1U), (unsigned)(i + 1U), reason);
        }
    }
}

bool ScriptRunner::runNow(uint8_t index, const char*& reason) {
    if (index >= ScriptStore::MAX_SCRIPTS) { reason = "emplacement invalide"; return false; }
    return start(index, reason);
}

void ScriptRunner::update() {
    if (!_inputs) return;

    ScriptStore::Meta metas[ScriptStore::MAX_SCRIPTS];
    ScriptStore::loadAllMeta(metas, ScriptStore::MAX_SCRIPTS);

    const uint32_t now = millis();

    for (uint8_t i = 0U; i < ScriptStore::MAX_SCRIPTS; ++i) {
        const ScriptStore::Meta& m = metas[i];
        if (!m.used || !m.enabled) continue;
        if (m.trigger == ScriptStore::TRIGGER_NONE) continue;
        if (m.triggerTarget == 0U) continue;

        bool declenche = false;

        if (m.trigger == ScriptStore::TRIGGER_INPUT_CHANGE) {
            const uint32_t seen = _inputs->transitions(m.triggerTarget);
            // Premier passage : on prend acte sans rien declencher. Sinon un
            // simple redemarrage relancerait tous les scripts dont l'entree a
            // bouge un jour -- des vannes qui s'ouvrent au reveil du module
            // sans que personne ne l'ait demande.
            if (!_primed) { _seenTransitions[i] = seen; continue; }
            declenche = (seen != _seenTransitions[i]);
            _seenTransitions[i] = seen;

        } else if (ScriptStore::triggerIsZone(m.trigger)) {
            if (!_schedule || !_config) continue;
            const uint8_t z = _config->zoneIndexById(m.triggerTarget);
            if (z >= MAX_ZONES) continue;
            const bool actif = _schedule->isZoneActive(z);
            if (!_primed) { _seenZoneActive[i] = actif; continue; }
            // La TRANSITION, pas l'etat : un script attache au demarrage de la
            // zone 5 part quand elle passe de fermee a ouverte, une fois.
            if (actif != _seenZoneActive[i]) {
                declenche = (m.trigger == ScriptStore::TRIGGER_ZONE_START)
                    ? actif : !actif;
            }
            _seenZoneActive[i] = actif;
        }

        if (!declenche) continue;

        // Un script qui commande la zone qui le declenche se rappellerait
        // aussitot. Le refus de relancer un script EN COURS ne suffit pas :
        // un script court a le temps de finir avant de se voir relancer.
        if (_lastStartMs[i] != 0U && (now - _lastStartMs[i]) < MIN_RESTART_MS) {
            EventLog::log(LOG_WARN,
                          "Script %u non relance : moins de %lus depuis son "
                          "dernier depart (boucle probable)",
                          (unsigned)(i + 1U),
                          (unsigned long)(MIN_RESTART_MS / 1000UL));
            continue;
        }

        const char* reason = "";
        if (start(i, reason)) {
            _lastStartMs[i] = now;
        } else {
            // Ne jamais avaler un declenchement manque : c'est precisement le
            // cas ou l'utilisateur croira que sa regle a joue.
            EventLog::log(LOG_WARN, "Script %u non lance (%s)",
                          (unsigned)(i + 1U), reason);
        }
    }
    _primed = true;

    for (uint8_t s = 0U; s < MAX_CONCURRENT; ++s) {
        Job& job = _jobs[s];
        if (!job.active) continue;

        // Places libres au moment du tick : c'est ce que fork() consulte pour
        // accepter une branche parallele, qui sera demarree juste apres.
        job.ctx.freeSlots = freeCount();
        const ScriptStatus st = job.vm.tick();
        const uint16_t forkPc = job.ctx.forkPc;
        job.ctx.forkPc = ScriptRuntimeContext::NO_FORK;
        // Relever les lancements demandes pendant ce tour AVANT de liberer
        // la place : un script qui en lance un autre puis se termine laisse
        // ainsi sa propre place au script lance.
        const uint8_t launches = job.ctx.launchMask;
        const uint8_t caller = job.index;
        job.ctx.launchMask = 0U;
        if (st == ScriptStatus::FINISHED && job.isBranch) {
            // Fin normale d'une branche 2 : le parent, en attente sur JOIN,
            // pourra continuer.
            Job& parent = _jobs[job.parentSlot];
            if (parent.active && !parent.isBranch && parent.ctx.branchesRunning > 0U) {
                parent.ctx.branchesRunning--;
            }
            EventLog::log(LOG_INFO, "Script %u : branche parallele terminee",
                          (unsigned)(job.index + 1U));
            job.active = false;
        } else if (st == ScriptStatus::FINISHED) {
            EventLog::log(LOG_INFO,
                          "Script %u termine : %u lecture(s), %u action(s), %u refus",
                          (unsigned)(job.index + 1U), (unsigned)job.ctx.reads,
                          (unsigned)job.ctx.actions, (unsigned)job.ctx.refusals);
            job.active = false;
            stopBranches(s, "script termine");
        } else if (st == ScriptStatus::ABORTED) {
            const ScriptAbort why = job.vm.abortReason();
            _lastAbort[job.index] = scriptAbortName(why);
            _lastAbortPc[job.index] = job.vm.programCounter();
            // Le detail n'a de sens que pour un refus de l'hote : les autres
            // categories (limite de pas, pile...) n'en produisent pas, et
            // job.ctx.refusalReason garderait alors un residu d'un refus
            // plus ancien s'il n'etait pas efface ici.
            if (why == ScriptAbort::HOST_REFUSED && job.ctx.refusalReason[0] != '\0') {
                strlcpy(_lastAbortDetail[job.index], job.ctx.refusalReason,
                        sizeof(_lastAbortDetail[job.index]));
            } else {
                _lastAbortDetail[job.index][0] = '\0';
            }
            // ERREUR et non avertissement : un script arrete n'a pas fait ce
            // que son auteur attendait, et personne ne le verra autrement.
            EventLog::log(LOG_ERROR,
                          "Script %u ARRETE : %s%s%s (apres %lu instructions, position %u)",
                          (unsigned)(job.index + 1U), scriptAbortName(why),
                          _lastAbortDetail[job.index][0] ? " -- " : "",
                          _lastAbortDetail[job.index],
                          (unsigned long)job.vm.stepsUsed(),
                          (unsigned)job.vm.programCounter());
            job.active = false;
            if (job.isBranch) {
                // Une branche est une partie du script : son arret arrete le
                // script entier, comme une erreur dans un script unique.
                Job& parent = _jobs[job.parentSlot];
                if (parent.active && !parent.isBranch && parent.index == job.index) {
                    parent.active = false;
                    EventLog::log(LOG_ERROR, "Script %u ARRETE : sa branche parallele s'est arretee",
                                  (unsigned)(job.index + 1U));
                    stopBranches(job.parentSlot, "script arrete");
                }
            } else {
                stopBranches(s, "script arrete");
            }
        } else if (forkPc != ScriptRuntimeContext::NO_FORK && !job.isBranch) {
            // fork() n'a accepte que s'il restait une place : elle est libre,
            // rien n'a demarre entre le tick et ici.
            if (!startBranch(s, forkPc)) {
                if (job.ctx.branchesRunning > 0U) job.ctx.branchesRunning--;
                EventLog::log(LOG_ERROR, "Script %u : branche parallele non demarree",
                              (unsigned)(job.index + 1U));
            }
        }
        // Apres le traitement de fin : `job` peut desormais etre libre et
        // reutilise par start(). Un script lance dans une place d'indice
        // superieur avance des ce tour-ci, sinon au tour suivant.
        if (launches) launchRequested(caller, launches);
    }
}
