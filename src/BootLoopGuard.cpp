#include "BootLoopGuard.h"

#include <Preferences.h>

#include "EventLog.h"
#include "FaultManager.h"
#include "NotificationManager.h"
#include "ScriptGlobals.h"

// Namespace NVS dedie, volontairement minuscule et independant de tout le
// reste : ce compteur doit rester lisible et inscriptible meme quand le module
// va mal, y compris si la configuration principale est corrompue. C'est
// justement la situation ou il sert.
//
// Declare dans l'enumeration de /api/debug/nvs-stats (WebManager.cpp).
static constexpr const char* NVS_NAMESPACE   = "aq_boot";
static constexpr const char* KEY_SUSPECT     = "susp";
static constexpr const char* KEY_EXPECTED    = "exp";
static constexpr const char* KEY_DEGRADED    = "degr";
// A l'essai : les fonctions suspendues viennent d'etre relancees pour de
// vrai, sous surveillance. Distinct de KEY_DEGRADED : les deux ne sont
// jamais vrais en meme temps.
static constexpr const char* KEY_PROBATION   = "prob";
// Un essai a deja ete tente pour cet episode et a echoue (rechute). Bloque
// tout nouvel essai automatique tant qu'un clearDegraded() manuel ne
// l'efface pas -- sans quoi un module dont la cause reelle persiste
// alternerait indefiniment entre degrade et essai.
static constexpr const char* KEY_PROBE_FAILED = "pfail";

uint8_t  BootLoopGuard::_suspectCount   = 0U;
bool     BootLoopGuard::_degraded       = false;
bool     BootLoopGuard::_cleared        = false;
bool     BootLoopGuard::_started        = false;
bool     BootLoopGuard::_onProbation    = false;
uint32_t BootLoopGuard::_probationStartMs = 0U;

void BootLoopGuard::persistCount(uint8_t count) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putUChar(KEY_SUSPECT, count);
    prefs.end();
}

void BootLoopGuard::onBoot() {
    if (_started) return;
    _started = true;

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        // Sans NVS on ne peut pas compter. On ne degrade pas pour autant :
        // degrader sur une incertitude serait un faux positif, et un faux
        // positif coute la confiance qu'on cherche a etablir.
        EventLog::log(LOG_WARN,
                      "Garde anti-boucle: NVS indisponible, comptage inactif");
        return;
    }

    const bool expected   = prefs.getBool(KEY_EXPECTED, false);
    const bool wasOnTrial = prefs.getBool(KEY_PROBATION, false);
    _degraded = prefs.getBool(KEY_DEGRADED, false);
    uint8_t count = prefs.getUChar(KEY_SUSPECT, 0U);

    if (expected) {
        // Redemarrage voulu : la marque est consommee immediatement, pour
        // qu'un plantage survenant juste apres ne beneficie pas de l'excuse
        // du redemarrage precedent.
        prefs.putBool(KEY_EXPECTED, false);
        if (wasOnTrial) {
            // Ni preuve ni refutation : quelqu'un a choisi de redemarrer
            // (mise a jour, intervention) pendant l'essai. On l'arrete sans
            // juger -- la surveillance normale reprend sur ce demarrage.
            prefs.putBool(KEY_PROBATION, false);
            EventLog::log(LOG_INFO,
                          "Garde anti-boucle: redemarrage volontaire pendant "
                          "l'essai -- ni preuve ni echec, essai interrompu");
        }
    } else if (wasOnTrial) {
        // Redemarrage NON planifie pendant l'essai : c'est la preuve
        // directe qu'une fonction tout juste relancee est bien en cause.
        // Traite a part, AVANT la logique de comptage habituelle : la
        // preuve directe vaut mieux que l'heuristique des quatre coups.
        prefs.putBool(KEY_PROBATION, false);
        prefs.putBool(KEY_PROBE_FAILED, true);
        prefs.putBool(KEY_DEGRADED, true);
        prefs.end();

        _degraded = true;
        FaultManager::setActive(FaultId::BOOT_LOOP, true);
        FaultManager::notifyError();
        EventLog::log(LOG_ERROR,
                      "Garde anti-boucle: RECHUTE pendant l'essai -- une "
                      "fonction relancee (meteo, mise a jour ou "
                      "notifications) a provoque ce redemarrage. Retour "
                      "immediat en mode degrade, aucun nouvel essai "
                      "automatique pour cet episode");
        _suspectCount = count;
        return;
    } else {
        if (count < 255U) count++;
        prefs.putUChar(KEY_SUSPECT, count);
    }

    if (!_degraded && count >= DEGRADED_THRESHOLD) {
        _degraded = true;
        prefs.putBool(KEY_DEGRADED, true);
    }
    prefs.end();

    _suspectCount = count;

    if (_degraded) {
        FaultManager::setActive(FaultId::BOOT_LOOP, true);
        FaultManager::notifyError();
        EventLog::log(LOG_ERROR,
                      "Garde anti-boucle: MODE DEGRADE actif apres %u demarrages "
                      "sans periode stable — arrosage, horloge, ecran et page Web "
                      "conserves ; meteo, verification de mise a jour et "
                      "notifications suspendues",
                      static_cast<unsigned>(count));
        EventLog::log(LOG_WARN,
                      "Garde anti-boucle: apres une periode stable, les fonctions "
                      "suspendues seront relancees a l'essai et surveillees -- "
                      "sortie manuelle toujours possible entre-temps");
    } else if (!expected) {
        EventLog::log(count > 1U ? LOG_WARN : LOG_INFO,
                      "Garde anti-boucle: demarrage non planifie %u/%u "
                      "(compteur remis a zero apres %lu s de fonctionnement)",
                      static_cast<unsigned>(count),
                      static_cast<unsigned>(DEGRADED_THRESHOLD),
                      static_cast<unsigned long>(STABLE_UPTIME_MS / 1000UL));
    }
}

void BootLoopGuard::update() {
    if (_onProbation) {
        // Seconde fenetre : l'essai lui-meme doit tenir STABLE_UPTIME_MS,
        // fonctions suspendues bel et bien relancees, avant confirmation.
        if (millis() - _probationStartMs >= STABLE_UPTIME_MS) confirmHealed();
        return;
    }

    if (!_started || _cleared) return;
    if (_suspectCount == 0U) { _cleared = true; return; }
    if (millis() < STABLE_UPTIME_MS) return;

    const uint8_t triggeringCount = _suspectCount;
    _cleared = true;
    persistCount(0U);
    // Remise a zero AUSSI en memoire, et pas seulement en NVS.
    //
    // Sans cette ligne, suspectBootCount() continuait de rendre la valeur
    // lue au demarrage alors que le compteur persistant valait deja zero.
    // L'API publiait donc un "3/4 avant mode degrade" dementi par l'etat
    // reel, ce qui conduit a repousser un redemarrage parfaitement sur -
    // constate le 31 aout 2026, uptime 263 s et compteur toujours affiche
    // a 3. Le drapeau _cleared empeche ce bloc de repasser, la correction
    // ne change donc rien au comportement du garde lui-meme.
    _suspectCount = 0U;
    EventLog::log(LOG_INFO,
                  "Garde anti-boucle: %lu s de fonctionnement stable, compteur "
                  "de demarrages remis a zero",
                  static_cast<unsigned long>(STABLE_UPTIME_MS / 1000UL));

    if (_degraded) {
        Preferences prefs;
        bool alreadyFailed = false;
        if (prefs.begin(NVS_NAMESPACE, true)) {
            alreadyFailed = prefs.getBool(KEY_PROBE_FAILED, false);
            prefs.end();
        }
        if (alreadyFailed) {
            EventLog::log(LOG_WARN,
                          "Garde anti-boucle: stable, mais un essai a deja "
                          "echoue pour cet episode -- reste degrade, sortie "
                          "manuelle necessaire");
        } else {
            beginProbation(triggeringCount);
        }
    }
}

void BootLoopGuard::beginProbation(uint8_t triggeringCount) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putBool(KEY_DEGRADED, false);
    prefs.putBool(KEY_PROBATION, true);
    prefs.putUChar(KEY_SUSPECT, triggeringCount);  // repris pour la notification eventuelle
    prefs.end();

    _degraded = false;
    _onProbation = true;
    _probationStartMs = millis();
    FaultManager::setActive(FaultId::BOOT_LOOP, false);
    EventLog::log(LOG_WARN,
                  "Garde anti-boucle: %lu s stables en mode degrade -- meteo, "
                  "mise a jour et notifications relancees A L'ESSAI, sous "
                  "surveillance %lu s de plus",
                  static_cast<unsigned long>(STABLE_UPTIME_MS / 1000UL),
                  static_cast<unsigned long>(STABLE_UPTIME_MS / 1000UL));
}

void BootLoopGuard::confirmHealed() {
    _onProbation = false;
    _cleared = true;

    Preferences prefs;
    uint8_t triggeringCount = DEGRADED_THRESHOLD;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        triggeringCount = prefs.getUChar(KEY_SUSPECT, DEGRADED_THRESHOLD);
        prefs.putBool(KEY_PROBATION, false);
        prefs.putBool(KEY_PROBE_FAILED, false);
        prefs.putUChar(KEY_SUSPECT, 0U);
        prefs.end();
    }
    _suspectCount = 0U;

    EventLog::log(LOG_INFO,
                  "Garde anti-boucle: essai confirme -- fonctionnement normal "
                  "retabli tout seul apres %u demarrage(s) sans stabilite",
                  static_cast<unsigned>(triggeringCount));
    // Sur maintenant, puisque l'essai vient justement de le prouver : cette
    // notification ne peut plus relancer la boucle qu'elle decrit.
    NotificationManager::enqueueAutoHeal(triggeringCount);
}

bool BootLoopGuard::isDegraded() { return _degraded; }

bool BootLoopGuard::isOnProbation() { return _onProbation; }

uint8_t BootLoopGuard::suspectBootCount() { return _suspectCount; }

bool BootLoopGuard::clearDegraded() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    prefs.putBool(KEY_DEGRADED, false);
    prefs.putBool(KEY_PROBATION, false);
    prefs.putBool(KEY_PROBE_FAILED, false);
    prefs.putUChar(KEY_SUSPECT, 0U);
    prefs.end();

    _degraded = false;
    _onProbation = false;
    _suspectCount = 0U;
    _cleared = true;
    FaultManager::setActive(FaultId::BOOT_LOOP, false);
    EventLog::log(LOG_WARN,
                  "Garde anti-boucle: mode degrade leve a la demande -- les "
                  "fonctions suspendues reprennent immediatement");
    return true;
}

void BootLoopGuard::restartDeliberately(const char* reason) {
    // Variables globales des scripts : leur ecriture est regroupee (au plus
    // une par minute) ; ne pas perdre la derniere minute sur un redemarrage
    // voulu.
    ScriptGlobals::flush();
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putBool(KEY_EXPECTED, true);
        prefs.end();
    }
    EventLog::log(LOG_WARN, "Redemarrage voulu: %s",
                  (reason != nullptr && reason[0] != '\0') ? reason : "sans motif");
    // Laisse le temps au journal de partir sur le port serie avant la coupure.
    delay(120);
    ESP.restart();
}
