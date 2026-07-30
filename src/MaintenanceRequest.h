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
    STAGE_UPDATE_TEST = 7U
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
