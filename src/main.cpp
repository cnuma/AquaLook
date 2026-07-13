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
#include "RuntimeProfiler.h"
#include "EquipmentManager.h"
#include "EquipmentModel.h"
#include "EquipmentOutputRuntimeAdapter.h"
#include "EquipmentExecutionShadowRuntime.h"
#include "EquipmentRuntimeConfigStore.h"
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
EquipmentManager shadowEquipmentMgr;
EquipmentModel::EquipmentConfigSet transientEquipmentModel;
EquipmentModel::EquipmentConfigSet shadowEquipmentModel;
RelayTopology::RelayTopologyConfig shadowRelayTopology;
AquaLook::Runtime::EquipmentOutputRuntimeAdapter outputAdapter;
AquaLook::Runtime::EquipmentExecutionShadowRuntime executionShadowRuntime;
AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;
AquaLook::Domain::Xl9535SharedOutputState xl9535SharedOutputState;

static bool equipmentRuntimeReady = false;
static bool shadowPumpScenarioReady = false;

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
        if (assignmentIndex < 0 || zone >= EquipmentModel::MAX_EQUIPMENTS) return false;

        EquipmentModel::EquipmentConfig& valve = transientEquipmentModel.equipments[zone];
        valve.enabled = true;
        valve.type = EquipmentModel::EQUIP_ZONE_VALVE;
        valve.targetIndex = zone;
        valve.relayAssignmentIndex = static_cast<uint8_t>(assignmentIndex);
        snprintf(valve.name, sizeof(valve.name), "Vanne zone %u", zone + 1U);

        EquipmentModel::ZoneEquipmentLink& link = transientEquipmentModel.zoneLinks[zone];
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

static bool relayChannelAlreadyAssigned(
    const RelayTopology::RelayTopologyConfig& topology,
    uint8_t boardIndex,
    uint8_t channelIndex
) {
    for (uint8_t index = 0U; index < RelayTopology::MAX_RELAY_ASSIGNMENTS; ++index) {
        const RelayTopology::RelayAssignment& assignment = topology.assignments[index];
        if (assignment.enabled &&
            assignment.boardIndex == boardIndex &&
            assignment.channelIndex == channelIndex) {
            return true;
        }
    }
    return false;
}

static bool findFreeShadowRelayChannel(
    const RelayTopology::RelayTopologyConfig& topology,
    uint8_t& boardIndex,
    uint8_t& channelIndex
) {
    for (uint8_t board = 0U; board < RelayTopology::MAX_RELAY_BOARDS; ++board) {
        const RelayTopology::RelayBoardConfig& boardConfig = topology.boards[board];
        if (!RelayTopology::validateBoard(boardConfig)) continue;

        for (uint8_t channel = 0U; channel < boardConfig.channelCount; ++channel) {
            if (!relayChannelAlreadyAssigned(topology, board, channel)) {
                boardIndex = board;
                channelIndex = channel;
                return true;
            }
        }
    }
    return false;
}

static int16_t findFreeShadowBoardIndex(
    const RelayTopology::RelayTopologyConfig& topology
) {
    for (uint8_t index = 0U; index < RelayTopology::MAX_RELAY_BOARDS; ++index) {
        if (!topology.boards[index].enabled) {
            return static_cast<int16_t>(index);
        }
    }
    return -1;
}

static bool createSyntheticShadowRelayChannel(
    RelayTopology::RelayTopologyConfig& topology,
    uint8_t& boardIndex,
    uint8_t& channelIndex
) {
    const int16_t freeBoardIndex = findFreeShadowBoardIndex(topology);
    if (freeBoardIndex < 0) return false;

    boardIndex = static_cast<uint8_t>(freeBoardIndex);
    channelIndex = 0U;

    RelayTopology::RelayBoardConfig& board = topology.boards[boardIndex];
    board.enabled = true;
    board.controller = RelayTopology::CONTROLLER_XL9535;
    board.i2cAddress = RelayTopology::defaultAddressForController(
        RelayTopology::CONTROLLER_XL9535
    );
    board.channelCount = 1U;
    board.logic = RelayTopology::LOGIC_DIRECT;

    return RelayTopology::validateBoard(board);
}

static int16_t findFreeShadowAssignmentIndex(
    const RelayTopology::RelayTopologyConfig& topology
) {
    for (uint8_t index = 0U; index < RelayTopology::MAX_RELAY_ASSIGNMENTS; ++index) {
        if (!topology.assignments[index].enabled) {
            return static_cast<int16_t>(index);
        }
    }
    return -1;
}

static int16_t findFreeShadowEquipmentIndex(uint8_t nbZones) {
    for (uint8_t index = nbZones; index < EquipmentModel::MAX_EQUIPMENTS; ++index) {
        if (!shadowEquipmentModel.equipments[index].enabled) {
            return static_cast<int16_t>(index);
        }
    }
    return -1;
}

static bool buildShadowPumpScenario(
    uint8_t nbZones,
    const AquaLook::Runtime::EquipmentRuntimeConfig& runtimeConfig
) {
    if (nbZones == 0U || nbZones >= EquipmentModel::MAX_EQUIPMENTS) return false;

    shadowEquipmentModel = transientEquipmentModel;
    shadowRelayTopology = relaisMgr.topology();

    const int16_t assignmentIndex = findFreeShadowAssignmentIndex(shadowRelayTopology);
    const int16_t equipmentIndex = findFreeShadowEquipmentIndex(nbZones);
    uint8_t boardIndex = 0U;
    uint8_t channelIndex = 0U;
    bool syntheticBoard = false;

    if (assignmentIndex < 0 || equipmentIndex < 0) return false;

    if (!findFreeShadowRelayChannel(shadowRelayTopology, boardIndex, channelIndex)) {
        syntheticBoard = true;
        if (!createSyntheticShadowRelayChannel(
                shadowRelayTopology,
                boardIndex,
                channelIndex)) {
            return false;
        }
    }

    RelayTopology::RelayAssignment& pumpAssignment =
        shadowRelayTopology.assignments[assignmentIndex];
    pumpAssignment.enabled = true;
    pumpAssignment.role = RelayTopology::ROLE_PUMP;
    pumpAssignment.targetIndex = runtimeConfig.pump.targetIndex;
    pumpAssignment.boardIndex = boardIndex;
    pumpAssignment.channelIndex = channelIndex;

    EquipmentModel::EquipmentConfig& pump =
        shadowEquipmentModel.equipments[equipmentIndex];
    pump.enabled = true;
    pump.type = EquipmentModel::EQUIP_PUMP;
    pump.targetIndex = runtimeConfig.pump.targetIndex;
    pump.relayAssignmentIndex = static_cast<uint8_t>(assignmentIndex);
    pump.startupDelayMs = runtimeConfig.pump.startupDelayMs;
    pump.shutdownDelayMs = runtimeConfig.pump.shutdownDelayMs;
    pump.minOnSec = runtimeConfig.pump.minOnSec;
    pump.minOffSec = runtimeConfig.pump.minOffSec;
    snprintf(pump.name, sizeof(pump.name), "Pompe shadow");

    if (!RelayTopology::validateAssignment(
            shadowRelayTopology,
            static_cast<uint8_t>(assignmentIndex)) ||
        !EquipmentModel::validateEquipment(
            shadowEquipmentModel,
            static_cast<uint8_t>(equipmentIndex))) {
        return false;
    }

    for (uint8_t zone = 0U; zone < nbZones; ++zone) {
        shadowEquipmentModel.zoneLinks[zone].pumpEquipmentIndex =
            static_cast<uint8_t>(equipmentIndex);
        if (!EquipmentModel::validateZoneLink(shadowEquipmentModel, zone, nbZones)) {
            return false;
        }
    }

    shadowEquipmentMgr.begin(
        &shadowEquipmentModel,
        &shadowRelayTopology,
        nbZones,
        nullptr
    );

    EventLog::log(
        LOG_INFO,
        "Shadow pump: scenario pret equipment=%u assignment=%u board=%u channel=%u source=%s delays=%u/%u passive=yes",
        static_cast<unsigned>(equipmentIndex),
        static_cast<unsigned>(assignmentIndex),
        static_cast<unsigned>(boardIndex),
        static_cast<unsigned>(channelIndex),
        syntheticBoard ? "synthetic_board" : "free_channel",
        static_cast<unsigned>(runtimeConfig.pump.startupDelayMs),
        static_cast<unsigned>(runtimeConfig.pump.shutdownDelayMs)
    );
    return shadowEquipmentMgr.isInitialized();
}

static void onRelayRequest(uint8_t zone, bool state) {
    if (equipmentRuntimeReady) {
        const uint32_t nowMs = millis();
        const EquipmentManager& shadowPlanManager =
            shadowPumpScenarioReady ? shadowEquipmentMgr : equipmentMgr;
        const EquipmentManager::ZoneExecutionPlan shadowPlan = state
            ? shadowPlanManager.buildZoneStartPlan(zone)
            : shadowPlanManager.buildZoneStopPlan(zone);
        executionShadowRuntime.submit(zone, shadowPlan, state, nowMs);

        const EquipmentManager::ActionResult result = state
            ? equipmentMgr.startZone(zone)
            : equipmentMgr.stopZone(zone);
        if (result == EquipmentManager::ACTION_OK) {
            displayMgr.requestDynamicRefresh();
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
    displayMgr.requestDynamicRefresh();
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
            EventLog::log(LOG_INFO, "I2C: peripherique trouve a 0x%02X", addr);
            found++;
        }
    }
    EventLog::log(found > 0 ? LOG_INFO : LOG_WARN,
                  "I2C: scan termine, %u peripherique(s)", found);

    configMgr.begin();
    const bool equipmentConfigStoreReady = equipmentConfigStore.begin();
    const AquaLook::Runtime::EquipmentRuntimeConfig& equipmentConfig =
        equipmentConfigStore.config();
    EventLog::log(
        equipmentConfigStoreReady ? LOG_INFO : LOG_WARN,
        "Equipment config runtime: status=%s enabled=%s mode=%s assignment=%u delays=%u/%u",
        equipmentConfigStore.lastStatus(),
        equipmentConfig.pump.enabled ? "yes" : "no",
        AquaLook::Runtime::equipmentControlModeName(equipmentConfig.pump.mode),
        static_cast<unsigned>(equipmentConfig.pump.relayAssignmentIndex),
        static_cast<unsigned>(equipmentConfig.pump.startupDelayMs),
        static_cast<unsigned>(equipmentConfig.pump.shutdownDelayMs)
    );

    displayMgr.initTft();
    displayMgr.showSplash(0, "Initialisation...");

    storageMgr.begin();
    splashStep(storageMgr.isSdAvailable() ? "Carte SD" : "SD indisponible");

    EventLog::log(LOG_INFO,
                  "Config: SSID='%s', mot de passe present=%s",
                  configMgr.wifi().ssid,
                  strlen(configMgr.wifi().password) > 0 ? "oui" : "non");

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
        EventLog::log(LOG_WARN,
                      "Relais V4: zone pilote 1 active, fallback legacy conserve");
    } else {
        outputAdapter.setPhysicalBackend(&relaisBackend);
        EventLog::log(LOG_ERROR,
                      "Relais V4: pilote indisponible, backend legacy force");
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

    const bool pumpConfigured = equipmentConfig.pump.enabled &&
        equipmentConfig.pump.mode != AquaLook::Runtime::EquipmentControlMode::MODE_DISABLED;
    const bool physicalModeRequested =
        equipmentConfig.pump.mode == AquaLook::Runtime::EquipmentControlMode::MODE_PHYSICAL;

    if (physicalModeRequested) {
        EventLog::log(
            LOG_WARN,
            "Equipment config runtime: mode physical demande mais bloque, execution shadow forcee"
        );
    }

    shadowPumpScenarioReady = equipmentRuntimeReady &&
        pumpConfigured &&
        buildShadowPumpScenario(nbZones, equipmentConfig);

    EventLog::log(
        shadowPumpScenarioReady ? LOG_INFO : (pumpConfigured ? LOG_WARN : LOG_INFO),
        shadowPumpScenarioReady
            ? "Shadow pump: configuration NVS active mode_effectif=shadow passive=yes"
            : (pumpConfigured
                ? "Shadow pump: configuration demandee mais scenario indisponible"
                : "Shadow pump: desactive par configuration NVS")
    );

    executionShadowRuntime.begin(equipmentRuntimeReady ? nbZones : 0U);
    splashStep("Relais");

    scheduleMgr.begin();
    scheduleMgr.setRelayCallback(onRelayRequest);
    configMgr.applyToSchedule(scheduleMgr);
    splashStep("Planning");

    wifiMgr.begin(configMgr.wifi().ssid, configMgr.wifi().password);
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
    EventLog::log(LOG_INFO, "HW: PSRAM %u octets", ESP.getPsramSize());
}

void loop() {
    SystemDiagnostics::loopEnter();

    uint32_t startedUs = RuntimeProfiler::start();
    FaultManager::update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::FAULTS_PRE, startedUs);

    startedUs = RuntimeProfiler::start();
    storageMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::STORAGE, startedUs);

    startedUs = RuntimeProfiler::start();
    wifiMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::WIFI, startedUs);
    const bool connected = wifiMgr.isConnected();

    if (connected) {
        startedUs = RuntimeProfiler::start();
        ntpMgr.update();
        RuntimeProfiler::stop(RuntimeProfiler::Component::NTP, startedUs);

        startedUs = RuntimeProfiler::start();
        weatherMgr.update(true);
        RuntimeProfiler::stop(RuntimeProfiler::Component::WEATHER, startedUs);
    }

    if (ntpMgr.isSynced()) {
        startedUs = RuntimeProfiler::start();
        scheduleMgr.update(
            ntpMgr.getHour(),
            ntpMgr.getMinute(),
            ntpMgr.getWeekday(),
            ntpMgr.getEpochDay(),
            weatherMgr.getRainMm()
        );
        RuntimeProfiler::stop(RuntimeProfiler::Component::SCHEDULE, startedUs);
    }

    startedUs = RuntimeProfiler::start();
    executionShadowRuntime.update(millis());
    RuntimeProfiler::stop(RuntimeProfiler::Component::EQUIPMENT_SHADOW, startedUs);

    startedUs = RuntimeProfiler::start();
    relaisMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::RELAY, startedUs);

    startedUs = RuntimeProfiler::start();
    webMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::WEB, startedUs);

    startedUs = RuntimeProfiler::start();
    displayMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::DISPLAY_MANAGER, startedUs);

    startedUs = RuntimeProfiler::start();
    displayPlanningDecorDraw(displayMgr);
    RuntimeProfiler::stop(RuntimeProfiler::Component::PLANNING_DECOR, startedUs);

    startedUs = RuntimeProfiler::start();
    FaultManager::update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::FAULTS_POST, startedUs);

    startedUs = RuntimeProfiler::start();
    yield();
    RuntimeProfiler::stop(RuntimeProfiler::Component::YIELD, startedUs);

    SystemDiagnostics::loopExit();
}
