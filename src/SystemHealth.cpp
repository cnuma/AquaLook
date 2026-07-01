#include "SystemHealth.h"
#include "EventBus.h"
#include "EventLog.h"

uint8_t SystemHealth::_faults = 0;

void SystemHealth::setFault(SystemFault fault, bool active) {
    const uint8_t mask = (uint8_t)(1U << (uint8_t)fault);
    const bool previous = (_faults & mask) != 0;
    if (previous == active) return;

    if (active) _faults |= mask;
    else _faults &= (uint8_t)~mask;

    EventBus::displayDirty = true;

    const char* name = "NTP";
    if (fault == SystemFault::LITTLEFS) name = "LittleFS";
    else if (fault == SystemFault::WIFI) name = "WiFi";

    EventLog::log(active ? LOG_ERROR : LOG_INFO,
                  "Systeme: defaut %s %s", name,
                  active ? "actif" : "resolu");
}

void SystemHealth::updateRuntime(bool wifiConnected, bool ntpSynced) {
    if (millis() < RUNTIME_GRACE_MS) return;
    setFault(SystemFault::WIFI, !wifiConnected);
    setFault(SystemFault::NTP, wifiConnected && !ntpSynced);
}

bool SystemHealth::hasAny() {
    return _faults != 0;
}

bool SystemHealth::hasFault(SystemFault fault) {
    return (_faults & (uint8_t)(1U << (uint8_t)fault)) != 0;
}

const char* SystemHealth::title() {
    if (hasFault(SystemFault::LITTLEFS)) return "LittleFS indisponible";
    if (hasFault(SystemFault::WIFI)) return "WiFi deconnecte";
    if (hasFault(SystemFault::NTP)) return "Heure non synchronisee";
    return "Aucune erreur";
}

const char* SystemHealth::adviceLine1() {
    if (hasFault(SystemFault::LITTLEFS)) return "Recharger les ressources Web";
    if (hasFault(SystemFault::WIFI)) return "Verifier le routeur et le reseau";
    if (hasFault(SystemFault::NTP)) return "Verifier Internet et serveur NTP";
    return "";
}

const char* SystemHealth::adviceLine2() {
    if (hasFault(SystemFault::LITTLEFS)) return "par USB puis redemarrer";
    if (hasFault(SystemFault::WIFI)) return "Un reboot peut aider";
    if (hasFault(SystemFault::NTP)) return "Attendre ou redemarrer";
    return "";
}

bool SystemHealth::rebootRecommended() {
    return hasFault(SystemFault::WIFI) || hasFault(SystemFault::NTP);
}
