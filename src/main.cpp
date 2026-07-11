#include "V4PilotRuntime.h"
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"
#include "WiFiManager.h"
#include "NTPManager.h"
#include "WeatherManager.h"
#include "RelaisManager.h"
#include "RelaisManagerBackend.h"
#include "ScheduleManager.h"
#include "WebManager.h"
#include "DisplayManager.h"
#include "DisplayPlanningDecor.h"
#include "ConfigManager.h"
#include "StorageManager.h"
#include "SystemDiagnostics.h"
#include "EquipmentManager.h"
#include "EquipmentModel.h"
#include "EquipmentOutputRuntimeAdapter.h"
#include "EquipmentExecutionShadowRuntime.h"
#include "domain/Xl9535SharedOutputState.h"

WiFiManager wifiMgr;
NTPManager ntpMgr;
WeatherManager weatherMgr;
RelaisManager relaisMgr;
AquaLook::Runtime::RelaisManagerBackend relaisBackend;
AquaLook::Runtime::V4PilotRuntime v4PilotRuntime;
ScheduleManager scheduleMgr;
WebManager webMgr;
DisplayManager displayMgr;
ConfigManager configMgr;
StorageManager storageMgr;
EquipmentManager equipmentMgr;
EquipmentModel::EquipmentConfigSet transientEquipmentModel;
AquaLook::Runtime::EquipmentOutputRuntimeAdapter outputAdapter;
AquaLook::Runtime::EquipmentExecutionShadowRuntime executionShadowRuntime;
AquaLook::Domain::Xl9535SharedOutputState xl9535SharedOutputState;

static bool equipmentRuntimeReady = false;

static int16_t findZoneAssignmentIndex(
    const RelayTopology::RelayTopologyConfig& topology,
    uint8_t zone
) {
    for (uint8_t index = 0U; index < RelayTopology::MAX_RELAY_ASSIGNMENTS; ++index) {
        const RelayTopology::RelayAssignment& assignment = topology.assignments[index];
        if (assignment.enabled &&
            assignment.role == RelayTopology::ROLE_ZONE_VALVE &&
            assignment.targetIndex == zone &&
            RelayTopology::validateAssignment(topology, index)) {
            return static_cast<int16_t>(index);
        }
    }
    return -1;
}

static bool buildTransientEquipmentModel(uint8_t nbZones) {
    EquipmentModel::clear(transientEquipmentModel);
    const RelayTopology::RelayTopologyConfig& topology = relaisMgr.topology();

    for (uint8_t zone = 0U; zone < nbZones; ++zone) {
        const int16_t assignmentIndex = findZoneAssignmentIndex(topology, zone);
        if (assignmentIndex < 0 || zone >= EquipmentModel::MAX_EQUIPMENTS) {
            return false;
        }

        EquipmentModel::EquipmentConfig& valve =
            transientEquipmentModel.equipments[zone];
        valve.enabled = true;
        valve.type = EquipmentModel::EQUIP_ZONE_VALVE;
        valve.targetIndex = zone;
        valve.relayAssignmentIndex = static_cast<uint8_t>(assignmentIndex);
        snprintf(valve.name, sizeof(valve.name), "Vanne zone %u", zone + 1U);

        EquipmentModel::ZoneEquipmentLink& link =
            transientEquipmentModel.zoneLinks[zone];
        link.enabled = true;
        link.zoneIndex = zone;
        link.valveEquipmentIndex = zone;
        link.pumpEquipmentIndex = EquipmentModel::INVALID_INDEX;

        if (!EquipmentModel::validateEquipment(transientEquipmentModel, zone) ||
            !EquipmentModel::validateZoneLink(transientEquipmentModel, zone, nbZones)) {
            return false;
        }
    }

    return true;
}

static void onRelayRequest(uint8_t zone, bool state) {
    if (equipmentRuntimeReady) {
        const uint32_t nowMs = millis();
        const EquipmentManager::ZoneExecutionPlan shadowPlan = state
            ? equipmentMgr.buildZoneStartPlan(zone)
            : equipmentMgr.buildZoneStopPlan(zone);
        executionShadowRuntime.submit(zone, shadowPlan, state, nowMs);

        const EquipmentManager::ActionResult result = state
            ? equipmentMgr.startZone(zone)
            : equipmentMgr.stopZone(zone);
        if (result == EquipmentManager::ACTION_OK) {
            EventBus::displayDirty = true;
            return;
        }
        EventLog::log(
            LOG_WARN,
            "Equipment: zone %u echec=%u, fallback adaptateur",
            zone + 1U,
            static_cast<unsigned>(result)
        );
    }

    outputAdapter.setZoneValve(zone, state, millis());
    EventBus::displayDirty = true;
}

static uint8_t _splashStep = 0;

static void splashStep(const char* label) {
    displayMgr.showSplash(_splashStep, label);
    _splashStep++;
}

void setup() {
    Serial.begin(115200);
    delay(300);

    FaultManager::begin();
    EventLog::log(LOG_INFO, "AquaLook v2.0 demarrage");
    SystemDiagnostics::begin();

    Wire.begin(SDA_PIN, SCL_PIN);

    EventLog::log(LOG_INFO, "I2C: scan demarre");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            EventLog::log(
                LOG_INFO,
                "I2C: peripherique trouve a 0x%02X",
                addr
            );
            found++;
        }
    }
    EventLog::log(
        found > 0 ? LOG_INFO : LOG_WARN,
        "I2C: scan termine, %u peripherique(s)",
        found
    );

    configMgr.begin();

    displayMgr.initTft();
    displayMgr.showSplash(0, "Initialisation...");

    storageMgr.begin();
    splashStep(storageMgr.isSdAvailable() ? "Carte SD" : "SD indisponible");

    EventLog::log(
        LOG_INFO,
        "Config: SSID='%s', mot de passe present=%s",
        configMgr.wifi().ssid,
        strlen(configMgr.wifi().password) > 0 ? "oui" : "non"
    );

    splashStep("Configuration");

    relaisMgr.setXl9535SharedOutputState(&xl9535SharedOutputState);
    relaisMgr.begin(&configMgr);
    relaisBackend.bind(&relaisMgr);

#if AQUALOOK_RELAY_BACKEND_V4
    const bool v4PilotReady = v4PilotRuntime.begin(
        relaisMgr.topology(),
        xl9535SharedOutputState
    );
    if (v4PilotReady) {
        outputAdapter.setPhysicalBackend(&v4PilotRuntime.backend());
        EventLog::log(
            LOG_WARN,
            "Relais V4: zone pilote 1 active, fallback legacy conserve"
        );
    } else {
        outputAdapter.setPhysicalBackend(&relaisBackend);
        EventLog::log(
            LOG_ERROR,
            "Relais V4: pilote indisponible, backend legacy force"
        );
    }
#else
    outputAdapter.setPhysicalBackend(&relaisBackend);
    EventLog::log(LOG_INFO, "Relais: profil backend legacy");
#endif

    outputAdapter.bind(&relaisMgr);

    const uint8_t nbZones = configMgr.nbZones();
    if (buildTransientEquipmentModel(nbZones)) {
        equipmentMgr.begin(
            &transientEquipmentModel,
            &relaisMgr.topology(),
            nbZones,
            &relaisMgr
        );
        equipmentMgr.setOutputAdapter(&outputAdapter);
        equipmentRuntimeReady = equipmentMgr.isInitialized() && equipmentMgr.hasExecutor();
    }

    EventLog::log(
        equipmentRuntimeReady ? LOG_INFO : LOG_WARN,
        equipmentRuntimeReady
            ? "Equipment: modele transitoire pret pour %u zone(s)"
            : "Equipment: modele indisponible, fallback adaptateur direct",
        nbZones
    );
    executionShadowRuntime.begin(equipmentRuntimeReady ? nbZones : 0U);
    splashStep("Relais");

    scheduleMgr.begin();
    scheduleMgr.setRelayCallback(onRelayRequest);
    configMgr.applyToSchedule(scheduleMgr);
    splashStep("Planning");

    wifiMgr.begin(
        configMgr.wifi().ssid,
        configMgr.wifi().password
    );
    splashStep("WiFi");

    ntpMgr.begin(&configMgr);
    splashStep("NTP");

    webMgr.setOutputAdapter(&outputAdapter);
    webMgr.registerSdStaticHandler(&storageMgr);
    webMgr.registerFaultRoutes();
    webMgr.begin(
        &ntpMgr,
        &weatherMgr,
        &relaisMgr,
        &scheduleMgr,
        &configMgr,
        &wifiMgr
    );
    splashStep("Serveur web");

    weatherMgr.begin(&configMgr);
    splashStep("Meteo");

    delay(800);

    displayMgr.setOutputAdapter(&outputAdapter);
    displayMgr.begin(
        &ntpMgr,
        &weatherMgr,
        &relaisMgr,
        &scheduleMgr,
        &configMgr
    );

    EventLog::log(LOG_INFO, "Main: setup termine, boucle demarree");
    EventLog::log(
        LOG_INFO,
        "HW: PSRAM %u octets",
        ESP.getPsramSize()
    );
}

void loop() {
    SystemDiagnostics::loopEnter();

    FaultManager::update();
    storageMgr.update();

    wifiMgr.update();
    const bool connected = wifiMgr.isConnected();

    if (connected) {
        ntpMgr.update();
        weatherMgr.update(true);
    }

    if (ntpMgr.isSynced()) {
        scheduleMgr.update(
            ntpMgr.getHour(),
            ntpMgr.getMinute(),
            ntpMgr.getWeekday(),
            ntpMgr.getEpochDay(),
            weatherMgr.getRainMm()
        );
    }

    executionShadowRuntime.update(millis());
    relaisMgr.update();
    webMgr.update();
    displayMgr.update();
    displayPlanningDecorDraw(displayMgr);

    FaultManager::update();
    yield();
    SystemDiagnostics::loopExit();
}
