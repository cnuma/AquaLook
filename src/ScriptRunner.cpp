#include "ScriptRunner.h"

#include "EventLog.h"

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
    }
    _primed = false;
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
    job.index = index;
    job.active = true;
    job.vm.load(ScriptProgram(job.code, meta.codeSize), &scriptHostOps(), &job.ctx);

    _lastAbort[index] = "";
    EventLog::log(LOG_INFO, "Script %u (%s) demarre", (unsigned)(index + 1U), meta.name);
    return true;
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

        const ScriptStatus st = job.vm.tick();
        if (st == ScriptStatus::FINISHED) {
            EventLog::log(LOG_INFO,
                          "Script %u termine : %u lecture(s), %u action(s), %u refus",
                          (unsigned)(job.index + 1U), (unsigned)job.ctx.reads,
                          (unsigned)job.ctx.actions, (unsigned)job.ctx.refusals);
            job.active = false;
        } else if (st == ScriptStatus::ABORTED) {
            const ScriptAbort why = job.vm.abortReason();
            _lastAbort[job.index] = scriptAbortName(why);
            // ERREUR et non avertissement : un script arrete n'a pas fait ce
            // que son auteur attendait, et personne ne le verra autrement.
            EventLog::log(LOG_ERROR, "Script %u ARRETE : %s (apres %lu instructions)",
                          (unsigned)(job.index + 1U), scriptAbortName(why),
                          (unsigned long)job.vm.stepsUsed());
            job.active = false;
        }
    }
}
