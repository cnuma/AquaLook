#include <Arduino.h>

#include "ConfigManager.h"
#include "EventLog.h"
#include "MaintenanceBoot.h"
#include "MaintenanceRequest.h"

// Le linker redirige l'appel Arduino vers ce wrapper avec
// -Wl,--wrap=_Z5setupv. Le setup historique reste accessible sous
// __real__Z5setupv et n'est donc ni reconstruit ni duplique.
void nominalAquaLookSetup() asm("__real__Z5setupv");
void maintenanceSetupWrapper() asm("__wrap__Z5setupv");

void maintenanceSetupWrapper() {
    const MaintenanceRequest request = MaintenanceRequestStore::load();
    if (request == MaintenanceRequest::NONE) {
        nominalAquaLookSetup();
        return;
    }

    Serial.begin(115200);
    delay(300);
    EventLog::log(
        LOG_WARN,
        "Maintenance: entree sure avant setup type=%s",
        MaintenanceRequestStore::name(request)
    );

    ConfigManager maintenanceConfig;
    maintenanceConfig.begin();

    // PROBE_GITHUB termine normalement par ESP.restart(). Une commande
    // inconnue, non implementee ou une anomalie NVS rend la main au setup
    // nominal afin de ne jamais immobiliser le programmateur.
    MaintenanceBoot::runIfRequested(maintenanceConfig);
    nominalAquaLookSetup();
}
