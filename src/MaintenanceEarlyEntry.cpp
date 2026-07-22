#include <Arduino.h>

#include "ConfigManager.h"
#include "MaintenanceBoot.h"
#include "MaintenanceRequest.h"

// Hook Arduino-ESP32 exécuté après l'initialisation du framework et avant
// setup(). Le chemin nominal ne réalise qu'une lecture NVS puis retourne.
void initVariant() {
    const MaintenanceRequest request = MaintenanceRequestStore::load();
    if (request == MaintenanceRequest::NONE) {
        return;
    }

    Serial.begin(115200);
    delay(300);
    Serial.printf(
        "[Maintenance] early request=%s\n",
        MaintenanceRequestStore::name(request)
    );

    // Les commandes futures sont reconnues mais ne doivent jamais bloquer
    // le programmateur tant que leur moteur n'est pas implémenté.
    if (request != MaintenanceRequest::PROBE_GITHUB) {
        const bool cleared = MaintenanceRequestStore::clear();
        Serial.printf(
            "[Maintenance] unsupported request=%s cleared=%s normalBoot=yes\n",
            MaintenanceRequestStore::name(request),
            cleared ? "yes" : "no"
        );
        return;
    }

    ConfigManager maintenanceConfig;
    maintenanceConfig.begin();

    // Pour PROBE_GITHUB, cette fonction termine toujours par ESP.restart().
    // Si elle retourne à cause d'une anomalie NVS, le démarrage nominal reste
    // autorisé afin d'éviter de rendre le programmateur indisponible.
    MaintenanceBoot::runIfRequested(maintenanceConfig);
}
