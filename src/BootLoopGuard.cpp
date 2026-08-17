#include "BootLoopGuard.h"

#include <Preferences.h>

#include "EventLog.h"
#include "FaultManager.h"

// Namespace NVS dedie, volontairement minuscule et independant de tout le
// reste : ce compteur doit rester lisible et inscriptible meme quand le module
// va mal, y compris si la configuration principale est corrompue. C'est
// justement la situation ou il sert.
//
// Declare dans l'enumeration de /api/debug/nvs-stats (WebManager.cpp).
static constexpr const char* NVS_NAMESPACE = "aq_boot";
static constexpr const char* KEY_SUSPECT   = "susp";
static constexpr const char* KEY_EXPECTED  = "exp";
static constexpr const char* KEY_DEGRADED  = "degr";

uint8_t BootLoopGuard::_suspectCount = 0U;
bool BootLoopGuard::_degraded = false;
bool BootLoopGuard::_cleared = false;
bool BootLoopGuard::_started = false;

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

    const bool expected = prefs.getBool(KEY_EXPECTED, false);
    _degraded = prefs.getBool(KEY_DEGRADED, false);
    uint8_t count = prefs.getUChar(KEY_SUSPECT, 0U);

    if (expected) {
        // Redemarrage voulu : la marque est consommee immediatement, pour
        // qu'un plantage survenant juste apres ne beneficie pas de l'excuse
        // du redemarrage precedent.
        prefs.putBool(KEY_EXPECTED, false);
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
                      "Garde anti-boucle: sortie du mode degrade uniquement sur "
                      "action explicite, apres avoir constate la cause");
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
    if (!_started || _cleared) return;
    if (_suspectCount == 0U) { _cleared = true; return; }
    if (millis() < STABLE_UPTIME_MS) return;

    _cleared = true;
    persistCount(0U);
    EventLog::log(LOG_INFO,
                  "Garde anti-boucle: %lu s de fonctionnement stable, compteur "
                  "de demarrages remis a zero",
                  static_cast<unsigned long>(STABLE_UPTIME_MS / 1000UL));
}

bool BootLoopGuard::isDegraded() { return _degraded; }

uint8_t BootLoopGuard::suspectBootCount() { return _suspectCount; }

bool BootLoopGuard::clearDegraded() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    prefs.putBool(KEY_DEGRADED, false);
    prefs.putUChar(KEY_SUSPECT, 0U);
    prefs.end();

    _degraded = false;
    _suspectCount = 0U;
    _cleared = true;
    FaultManager::setActive(FaultId::BOOT_LOOP, false);
    EventLog::log(LOG_WARN,
                  "Garde anti-boucle: mode degrade leve a la demande — les "
                  "fonctions suspendues reprendront au prochain demarrage");
    return true;
}

void BootLoopGuard::restartDeliberately(const char* reason) {
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
