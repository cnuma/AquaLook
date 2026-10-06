#include "ScriptHostRuntime.h"

#include <cstdio>

#include "EventLog.h"
#include "NotificationManager.h"
#include "ScriptStore.h"

namespace {

using namespace AquaLook::Domain;

ScriptRuntimeContext* ctxOf(void* raw) {
    return static_cast<ScriptRuntimeContext*>(raw);
}

// Traduit un identifiant STABLE de zone en index d'affichage. Retourne
// MAX_ZONES si l'identifiant ne designe plus rien -- le cas d'un script qui
// cite une zone supprimee depuis. On refuse alors, on ne devine pas.
uint8_t zoneIndex(const ScriptRuntimeContext* ctx, uint16_t zoneId) {
    if (!ctx || !ctx->config) return MAX_ZONES;
    return ctx->config->zoneIndexById(zoneId);
}

bool readInput(void* raw, uint16_t inputId, int32_t& value) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    if (!ctx || !ctx->inputs) return false;
    bool active = false;
    // read() ne repond QUE si la valeur est stabilisee. Un script ne doit
    // jamais decider sur une lecture qui n'a pas encore tenu : c'est
    // exactement ce que l'anti-rebond existe pour empecher.
    if (!ctx->inputs->read(inputId, active)) {
        ctx->refusals++;
        // read() confond deux causes tres differentes sous le meme false --
        // distinguees ici en reparcourant les entrees connues, plutot que de
        // dire "action refusee" pour un flotteur qui claquette encore.
        bool connue = false;
        for (uint8_t k = 0U; k < ctx->inputs->count(); ++k) {
            if (ctx->inputs->idAt(k) == inputId) { connue = true; break; }
        }
        snprintf(ctx->refusalReason, sizeof(ctx->refusalReason),
                 connue ? "entree %u pas encore stabilisee" : "entree %u inconnue",
                 (unsigned)inputId);
        return false;
    }
    ctx->reads++;
    value = active ? 1 : 0;
    return true;
}

bool zoneActive(void* raw, uint16_t zoneId, int32_t& value) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    const uint8_t z = zoneIndex(ctx, zoneId);
    if (z >= MAX_ZONES || !ctx->schedule) {
        if (ctx) {
            ctx->refusals++;
            snprintf(ctx->refusalReason, sizeof(ctx->refusalReason), "zone %u inconnue", (unsigned)zoneId);
        }
        return false;
    }
    ctx->reads++;
    value = ctx->schedule->isZoneActive(z) ? 1 : 0;
    return true;
}

bool zoneRemainingSec(void* raw, uint16_t zoneId, int32_t& value) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    const uint8_t z = zoneIndex(ctx, zoneId);
    if (z >= MAX_ZONES || !ctx->schedule) {
        if (ctx) {
            ctx->refusals++;
            snprintf(ctx->refusalReason, sizeof(ctx->refusalReason), "zone %u inconnue", (unsigned)zoneId);
        }
        return false;
    }
    ctx->reads++;
    // Une zone SUSPENDUE a un reliquat, meme si elle n'arrose pas. C'est
    // justement ce reliquat qui interesse un script de reprise.
    const uint32_t ms = ctx->schedule->isZonePaused(z)
        ? ctx->schedule->getPausedRemainingMs(z)
        : ctx->schedule->getRemainingMs(z);
    value = static_cast<int32_t>(ms / 1000UL);
    return true;
}

bool action(void* raw, ScriptAction act, uint16_t target, int32_t arg) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    if (!ctx || !ctx->schedule) return false;

    // Lancer un autre script : jamais un refus. Decision du 6 oct. 2026 -- un
    // lancement rate (place prise, script inactif ou deja en cours) ne doit
    // pas interrompre le script appelant, qui tient peut-etre une zone
    // ouverte. Le runner journalise ce qui n'a pas pu partir.
    if (act == ScriptAction::SCRIPT_RUN) {
        if (target < 1U || target > ScriptStore::MAX_SCRIPTS) {
            EventLog::log(LOG_WARN, "Script: lancement du script %u ignore (emplacement 1 a %u)",
                          (unsigned)target, (unsigned)ScriptStore::MAX_SCRIPTS);
        } else if (target - 1U == ctx->selfIndex) {
            EventLog::log(LOG_WARN, "Script %u: se lancer lui-meme est ignore",
                          (unsigned)target);
        } else {
            ctx->launchMask |= static_cast<uint8_t>(1U << (target - 1U));
            ctx->actions++;
        }
        return true;
    }

    const uint8_t z = zoneIndex(ctx, target);
    if (z >= MAX_ZONES) {
        EventLog::log(LOG_WARN, "Script: zone %u inconnue, action refusee",
                      (unsigned)target);
        snprintf(ctx->refusalReason, sizeof(ctx->refusalReason), "zone %u inconnue", (unsigned)target);
        ctx->refusals++;
        return false;
    }

    bool ok = false;
    const char* pourquoi = "action refusee";
    switch (act) {
        case ScriptAction::ZONE_START:
            if (arg <= 0) {
                // startZoneForSeconds() refuse aussi une duree nulle, mais
                // sans le dire : distingue ici plutot que de melanger ce cas
                // avec un refus venu du planificateur lui-meme.
                pourquoi = "duree nulle ou negative";
                ok = false;
            } else {
                ok = ctx->schedule->startZoneForSeconds(z, (uint32_t)arg);
                if (!ok) pourquoi = "demarrage refuse par le planificateur";
            }
            break;
        case ScriptAction::ZONE_STOP:
            ctx->schedule->stopManualWatering(z);
            ok = true;
            break;
        case ScriptAction::ZONE_PAUSE:
            ok = ctx->schedule->pauseZone(z);
            if (!ok) pourquoi = "zone non active, rien a suspendre";
            break;
        case ScriptAction::ZONE_RESUME:
            ok = ctx->schedule->resumeZone(z);
            if (!ok) pourquoi = "zone non suspendue, rien a reprendre";
            break;
        case ScriptAction::SET_OUTPUT:
        default:
            // Pas encore implemente : le dire par un refus vaut mieux que de
            // retourner "fait" sans rien faire. Un script qui croit avoir
            // lance la pompe attendrait un remplissage qui n'arrive jamais.
            // [SCRIPT-ACTION] : code interne ScriptAction, pas un identifiant
            // choisi par l'utilisateur. Voir /logs/messages.tsv.
            EventLog::log(LOG_WARN, "[SCRIPT-ACTION] %u non_implementee",
                          (unsigned)act);
            pourquoi = "action non implementee";
            ok = false;
            break;
    }

    if (ok) {
        ctx->actions++;
    } else {
        ctx->refusals++;
        snprintf(ctx->refusalReason, sizeof(ctx->refusalReason), "zone %u : %s", (unsigned)target, pourquoi);
    }
    return ok;
}

bool notify(void* raw, uint16_t code) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    if (ctx) ctx->lastNotify = code;
    // [SCRIPT-NOTIFY] : code choisi par l'utilisateur dans l'editeur de
    // scripts (commande "message"), pas resolu ici. Voir /logs/messages.tsv.
    EventLog::log(LOG_INFO, "[SCRIPT-NOTIFY] code=%u", (unsigned)code);
    return true;
}

bool alert(void* raw, uint16_t code) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    if (ctx) ctx->lastAlert = code;
    if (!NotificationManager::enqueueScriptMessage(code, ctx ? ctx->name : "")) {
        // Refuser plutot que d'accepter sans rien envoyer. Un script qui croit
        // avoir alerte quelqu'un est plus dangereux qu'un script arrete : on
        // compte dessus pour etre prevenu d'une cuve vide.
        // [SCRIPT-ALERT] : code choisi par l'utilisateur (commande
        // "alerte"), pas resolu ici. Voir /logs/messages.tsv.
        EventLog::log(LOG_WARN,
                      "[SCRIPT-ALERT] code=%u non_envoyee",
                      (unsigned)code);
        if (ctx) {
            ctx->refusals++;
            snprintf(ctx->refusalReason, sizeof(ctx->refusalReason),
                     "notification %u non envoyee", (unsigned)code);
        }
        return false;
    }
    EventLog::log(LOG_INFO, "[SCRIPT-ALERT] code=%u envoyee", (unsigned)code);
    return true;
}

uint32_t nowMs(void*) { return millis(); }

bool fork(void* raw, uint16_t pc) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    if (!ctx || ctx->isBranch || ctx->forkPc != ScriptRuntimeContext::NO_FORK) return false;
    if (ctx->freeSlots == 0U) {
        // La machine fait alors la branche 2 elle-meme, a la suite : le dire,
        // sinon l'utilisateur croira ses deux zones arrosees ensemble.
        EventLog::log(LOG_WARN,
                      "Script %u : branche parallele executee a la suite (aucune place libre)",
                      (unsigned)(ctx->selfIndex + 1U));
        return false;
    }
    ctx->forkPc = pc;
    ctx->freeSlots--;
    ctx->branchesRunning++;
    return true;
}

uint8_t branchesRunning(void* raw) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    return ctx ? ctx->branchesRunning : 0U;
}

const ScriptHostOps OPS = {
    readInput, zoneActive, zoneRemainingSec, action, notify, alert, nowMs,
    fork, branchesRunning
};

} // namespace

const AquaLook::Domain::ScriptHostOps& scriptHostOps() { return OPS; }
