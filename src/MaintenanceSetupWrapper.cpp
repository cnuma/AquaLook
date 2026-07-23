#include <Arduino.h>

#include "ConfigManager.h"
#include "EventLog.h"
#include "MaintenanceBoot.h"
#include "MaintenanceRequest.h"

namespace {
constexpr uint32_t MAINTENANCE_TASK_STACK = 16384U;
constexpr UBaseType_t MAINTENANCE_TASK_PRIORITY = 1U;
constexpr BaseType_t MAINTENANCE_TASK_CORE = 0;

void maintenanceTask(void*) {
    ConfigManager maintenanceConfig;
    maintenanceConfig.begin();

    // PROBE_GITHUB termine normalement par ESP.restart(). Si le moteur
    // retourne exceptionnellement, redemarrer proprement vers le mode normal
    // plutot que reprendre un setup partiellement intercepte.
    MaintenanceBoot::runIfRequested(maintenanceConfig);
    delay(250);
    ESP.restart();
}
}

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

    const BaseType_t created = xTaskCreatePinnedToCore(
        maintenanceTask,
        "aqualook-maint",
        MAINTENANCE_TASK_STACK,
        nullptr,
        MAINTENANCE_TASK_PRIORITY,
        nullptr,
        MAINTENANCE_TASK_CORE
    );

    if (created != pdPASS) {
        EventLog::log(
            LOG_ERROR,
            "Maintenance: creation tache impossible, retour mode normal"
        );
        MaintenanceRequestStore::clear();
        nominalAquaLookSetup();
        return;
    }

    // La maintenance s'execute sur une pile dediee. La loopTask Arduino ne
    // doit ni poursuivre le setup nominal ni consommer de pile pendant TLS.
    vTaskSuspend(nullptr);
}