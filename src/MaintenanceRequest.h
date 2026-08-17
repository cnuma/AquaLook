#pragma once

#include <Arduino.h>

// Commandes de maintenance persistées en NVS.
// La valeur NONE conserve le démarrage nominal actuel.
enum class MaintenanceRequest : uint8_t {
    NONE = 0U,
    PROBE_GITHUB = 1U,
    CHECK_VERSION = 2U,
    INSTALL_UPDATE = 3U,
    RECOVERY = 4U,
    FACTORY_RESET = 5U,
    DOWNLOAD_UPDATE_TEST = 6U,
    STAGE_UPDATE_TEST = 7U,
    // Mise a jour des ressources Web (carte SD), executee en mode maintenance.
    // Elle n'ecrit PAS en flash : seuls WiFi, TLS et la carte SD sont requis.
    // Ce mode offre ~245 Ko de tas et une tache dediee, la ou le fonctionnement
    // normal n'en laisse que ~32 Ko avec une pile partagee — ce qui faisait
    // echouer les tentatives precedentes.
    WEB_ASSETS_UPDATE = 8U
};

class MaintenanceRequestStore {
public:
    static MaintenanceRequest load();
    static bool save(MaintenanceRequest request);
    static bool clear();
    static const char* name(MaintenanceRequest request);

private:
    static bool isValid(uint8_t rawValue);
};
