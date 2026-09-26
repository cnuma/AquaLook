#include "ScriptHostRuntime.h"

#include "EventLog.h"
#include "NotificationManager.h"

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
    if (!ctx->inputs->read(inputId, active)) { ctx->refusals++; return false; }
    ctx->reads++;
    value = active ? 1 : 0;
    return true;
}

bool zoneActive(void* raw, uint16_t zoneId, int32_t& value) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    const uint8_t z = zoneIndex(ctx, zoneId);
    if (z >= MAX_ZONES || !ctx->schedule) { if (ctx) ctx->refusals++; return false; }
    ctx->reads++;
    value = ctx->schedule->isZoneActive(z) ? 1 : 0;
    return true;
}

bool zoneRemainingSec(void* raw, uint16_t zoneId, int32_t& value) {
    ScriptRuntimeContext* ctx = ctxOf(raw);
    const uint8_t z = zoneIndex(ctx, zoneId);
    if (z >= MAX_ZONES || !ctx->schedule) { if (ctx) ctx->refusals++; return false; }
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

    const uint8_t z = zoneIndex(ctx, target);
    if (z >= MAX_ZONES) {
        EventLog::log(LOG_WARN, "Script: zone %u inconnue, action refusee",
                      (unsigned)target);
        ctx->refusals++;
        return false;
    }

    bool ok = false;
    switch (act) {
        case ScriptAction::ZONE_START:
            ok = ctx->schedule->startZoneForSeconds(z, arg > 0 ? (uint32_t)arg : 0U);
            break;
        case ScriptAction::ZONE_STOP:
            ctx->schedule->stopManualWatering(z);
            ok = true;
            break;
        case ScriptAction::ZONE_PAUSE:
            ok = ctx->schedule->pauseZone(z);
            break;
        case ScriptAction::ZONE_RESUME:
            ok = ctx->schedule->resumeZone(z);
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
            ok = false;
            break;
    }

    if (ok) ctx->actions++; else ctx->refusals++;
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
        if (ctx) ctx->refusals++;
        return false;
    }
    EventLog::log(LOG_INFO, "[SCRIPT-ALERT] code=%u envoyee", (unsigned)code);
    return true;
}

uint32_t nowMs(void*) { return millis(); }

const ScriptHostOps OPS = {
    readInput, zoneActive, zoneRemainingSec, action, notify, alert, nowMs
};

} // namespace

const AquaLook::Domain::ScriptHostOps& scriptHostOps() { return OPS; }
