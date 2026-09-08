#include "V4PilotRuntime.h"
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "EventBus.h"
#include "EventLog.h"
#include "HeapMetrics.h"
#include "FaultManager.h"
#include "MaintenanceResult.h"   // datation differee du dernier resultat
#include "OtaBootGuard.h"
#include "WiFiManager.h"
#include "NTPManager.h"
#include "NotificationManager.h"
#include "WeatherManager.h"
#include "InputSampler.h"
#include "RelaisManager.h"
#include "IoExpanderManager.h"
#include "ScheduleManager.h"
#include "WebManager.h"
#include "DisplayManager.h"
#include "DisplayPlanningDecor.h"
#include "ConfigManager.h"
#include "StorageManager.h"
#include "BootLoopGuard.h"
#include "SystemDiagnostics.h"
#include "CloudSync.h"
#include "UpdateCheckScheduler.h"
#include "RuntimeProfiler.h"
#include "EquipmentManager.h"
#include "EquipmentModel.h"
#include "EquipmentOutputRuntimeAdapter.h"
#include "EquipmentExecutionShadowRuntime.h"
#include "EquipmentRuntimeConfigStore.h"
#include "EquipmentOrchestrator.h"
#include "domain/I2cExpanderSharedOutputState.h"

WiFiManager wifiMgr;
NTPManager ntpMgr;
WeatherManager weatherMgr;
RelaisManager relaisMgr;
IoExpanderManager ioExpander;
AquaLook::Runtime::V4PilotRuntime v4PilotRuntime;
InputSampler inputSampler;
ScheduleManager scheduleMgr;
WebManager webMgr;
DisplayManager displayMgr;
ConfigManager configMgr;
UpdateCheckScheduler updateCheckScheduler;
CloudSyncScheduler cloudSyncScheduler;
StorageManager storageMgr;
EquipmentManager equipmentMgr;
EquipmentManager shadowEquipmentMgr;
EquipmentModel::EquipmentConfigSet transientEquipmentModel;
EquipmentModel::EquipmentConfigSet shadowEquipmentModel;
RelayTopology::RelayTopologyConfig shadowRelayTopology;
AquaLook::Runtime::EquipmentOutputRuntimeAdapter outputAdapter;
AquaLook::Runtime::EquipmentExecutionShadowRuntime executionShadowRuntime;
AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;
AquaLook::Application::EquipmentOrchestrator equipmentOrchestrator;
AquaLook::Domain::I2cExpanderSharedOutputState sharedOutputState;

static bool equipmentRuntimeReady = false;
static bool shadowPumpScenarioReady = false;
static bool equipmentOrchestratorShadowReady = false;

// Compteurs de parite d'execution V4 vs legacy (voir
// AQUALOOK_V4_REPRISE_MIGRATION_DECISION.md). L'utilisateur n'etant pas au
// module, c'est par le port serie qu'on prouve que V4 saurait executer ce que
// le legacy decide -- avant toute bascule d'autorite. Purement observationnel.
uint32_t g_parityAgree = 0U;      // expose via /api/diagnostics
uint32_t g_parityDisagree = 0U;   // pour un soak mesurable sans le log
// L'arbitrage V4-vs-legacy a l'execution a ete retire : le moteur est
// desormais choisi a la COMPILATION (profil AQUALOOK_RELAY_BACKEND_V4),
// ce qui supprime une couche de decision inutile a chaque commande. Le
// rollback n'est plus un repli runtime mais un reflash du profil legacy.


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
        EquipmentManager::ZoneExecutionPlan shadowPlan;
        bool shadowPlanFromOrchestrator = false;

        if (equipmentOrchestratorShadowReady) {
            const AquaLook::Application::EquipmentOrchestrator::Preview orchestratorPreview = state
                ? equipmentOrchestrator.previewStartZone(zone)
                : equipmentOrchestrator.previewStopZone(zone);
            const AquaLook::Application::EquipmentOrchestrator::ObservationStats& orchestratorStats =
                equipmentOrchestrator.stats();
            EventLog::log(
                orchestratorPreview.ready() ? LOG_INFO : LOG_WARN,
                "Orchestrator shadow: zone=%u intent=%s status=%u plan=%u steps=%u pump=%s authority=no stats=%lu/%lu/%lu ready=%lu rejected=%lu pumpPlans=%lu plannedSteps=%lu",
                zone + 1U,
                state ? "START" : "STOP",
                static_cast<unsigned>(orchestratorPreview.status),
                static_cast<unsigned>(orchestratorPreview.planResult),
                static_cast<unsigned>(orchestratorPreview.stepCount),
                orchestratorPreview.requiresPump ? "yes" : "no",
                static_cast<unsigned long>(orchestratorStats.totalRequests),
                static_cast<unsigned long>(orchestratorStats.startRequests),
                static_cast<unsigned long>(orchestratorStats.stopRequests),
                static_cast<unsigned long>(orchestratorStats.readyPlans),
                static_cast<unsigned long>(orchestratorStats.rejectedPlans),
                static_cast<unsigned long>(orchestratorStats.plansWithPump),
                static_cast<unsigned long>(orchestratorStats.plannedSteps)
            );
            shadowPlan = orchestratorPreview.plan;
            shadowPlanFromOrchestrator = true;
        } else {
            const EquipmentManager& shadowPlanManager =
                shadowPumpScenarioReady ? shadowEquipmentMgr : equipmentMgr;
            shadowPlan = state
                ? shadowPlanManager.buildZoneStartPlan(zone)
                : shadowPlanManager.buildZoneStopPlan(zone);
        }

        EventLog::log(
            shadowPlan.valid() ? LOG_INFO : LOG_WARN,
            "Orchestrator handoff: zone=%u intent=%s source=%s result=%u steps=%u pump=%s authority=no",
            zone + 1U,
            state ? "START" : "STOP",
            shadowPlanFromOrchestrator ? "orchestrator" : "plan_builder",
            static_cast<unsigned>(shadowPlan.result),
            static_cast<unsigned>(shadowPlan.stepCount),
            shadowPlan.requiresPump ? "yes" : "no"
        );

        executionShadowRuntime.submit(zone, shadowPlan, state, nowMs);

        // ── Parite d'execution STRICTE : meme voie, meme etat ─────────────
        // "ACCORD" ne signifie plus "plan valide" mais "V4 pilote la MEME voie
        // physique, dans le MEME etat, que ce que dit la table de cablage lue
        // directement". Ce n'est pas une comparaison avec le moteur historique
        // -- il n'y en a plus -- mais un controle du planificateur contre sa
        // source de verite. Cinq criteres, tous
        // via des accesseurs publics : bonne zone, une seule commande de vanne,
        // etat concordant, meme equipement de vanne, meme carte/canal. Au
        // moindre doute on penche vers DESACCORD -- jamais de fausse confiance.
        const uint8_t nbZonesNow = configMgr.nbZones();
        // ATTENTION au nom qu'avait cette variable ("tableValve") : elle ne
        // vient PAS du moteur historique. C'est une lecture directe de la
        // table de cablage, qui sert de reference independante au plan
        // construit par V4. Les deux appartiennent a la generation actuelle ;
        // seul le mot "legacy" laissait croire le contraire.
        const RelayTopology::MappingResolution tableValve =
            RelayTopology::resolveZoneValve(relaisMgr.topology(), zone, nbZonesNow);
        const EquipmentManager::ZoneResolution v4Valve = equipmentMgr.resolveZone(zone);

        uint8_t valveSteps = 0U;
        bool v4ValveOn = false;
        uint8_t v4ValveEquip = 0xFFU;
        for (uint8_t s = 0U;
             s < shadowPlan.stepCount && s < EquipmentManager::MAX_PLAN_STEPS; ++s) {
            const EquipmentManager::PlanStep& st = shadowPlan.steps[s];
            if (st.action == EquipmentManager::PLAN_ACTION_VALVE_ON) {
                valveSteps++; v4ValveOn = true; v4ValveEquip = st.equipmentIndex;
            } else if (st.action == EquipmentManager::PLAN_ACTION_VALVE_OFF) {
                valveSteps++; v4ValveOn = false; v4ValveEquip = st.equipmentIndex;
            }
        }

        const bool cZone  = (shadowPlan.zone == zone);
        const bool cOne   = (valveSteps == 1U);
        const bool cState = (v4ValveOn == state);
        const bool cEquip = v4Valve.valid() && (v4ValveEquip == v4Valve.equipmentIndex);
        const bool cChan  = tableValve.valid && v4Valve.relay.valid &&
                            tableValve.boardIndex   == v4Valve.relay.boardIndex &&
                            tableValve.channelIndex == v4Valve.relay.channelIndex;
        const bool accord = cZone && cOne && cState && cEquip && cChan;
        if (accord) g_parityAgree++; else g_parityDisagree++;

        // Ligne compacte (< LOG_MSG_LEN=72) : le niveau porte le verdict
        // (INFO=accord, WARN=desaccord), donc `grep WARN PARITE` debusque
        // instantanement toute divergence. Le detail des criteres n'est
        // journalise qu'en cas de desaccord, pour comprendre lequel a lache.
        EventLog::log(
            accord ? LOG_INFO : LOG_WARN,
            "PARITE z%u %s L=%u.%u/%s V=%u.%u/%s ok=%lu ko=%lu %s",
            zone + 1U, state ? "OUVRE" : "FERME",
            static_cast<unsigned>(tableValve.boardIndex),
            static_cast<unsigned>(tableValve.channelIndex), state ? "ON" : "OFF",
            static_cast<unsigned>(v4Valve.relay.boardIndex),
            static_cast<unsigned>(v4Valve.relay.channelIndex), v4ValveOn ? "ON" : "OFF",
            static_cast<unsigned long>(g_parityAgree),
            static_cast<unsigned long>(g_parityDisagree),
            accord ? "ACCORD" : "DESACCORD"
        );
        if (!accord) {
            EventLog::log(LOG_WARN,
                "PARITE-KO z%u crit zone=%u une=%u etat=%u equip=%u voie=%u",
                zone + 1U, static_cast<unsigned>(cZone), static_cast<unsigned>(cOne),
                static_cast<unsigned>(cState), static_cast<unsigned>(cEquip),
                static_cast<unsigned>(cChan));
        }

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

    // Garde OTA : doit s'executer avant toute initialisation lourde. Ne
    // redemarre que si un retour arriere automatique est necessaire suite a
    // une bascule OTA qui n'a pas ete validee.
    // Avant toute initialisation lourde : si les demarrages precedents se
    // sont mal passes, les sous-systemes de confort ne doivent meme pas
    // demarrer.
    BootLoopGuard::onBoot();
    OtaBootGuard::onBoot();

    FaultManager::begin();
    EventLog::begin();
    EventLog::log(LOG_INFO, "AquaLook v2.0 demarrage");
    // Mesure ici, une fois pour toutes, les tailles totales de tas et de
    // PSRAM : l'IDF ne les expose qu'au prix d'un parcours complet du tas,
    // qui plus tard - WiFi actif, requetes HTTP en cours - ferait sauter le
    // chien de garde d'interruption (voir HeapMetrics.h). A ce point du
    // demarrage la radio n'est pas encore lancee : c'est le moment sur.
    AquaLook::Heap::warmUp();
    SystemDiagnostics::begin();

    RELAY_WIRE_BUS.begin(SDA_PIN, SCL_PIN);
    // 400 kHz (mode rapide) plutot que les 100 kHz par defaut d'Arduino. Le
    // XL9535 le supporte, et la lecture d'etat des relais s'est revelee
    // couter jusqu'a 98 ms par passage de boucle sur la carte S3 - mesure
    // le 29 aout 2026 en instrumentant DisplayManager::update(), qui
    // interroge getState() pour chaque zone a chaque tour.
    // Meme correctif que pour le bus tactile (voir DisplayManager::begin) :
    // les deux bus etaient restes a la vitesse par defaut.
    RELAY_WIRE_BUS.setClock(400000UL);

    EventLog::log(LOG_INFO, "I2C: scan demarre");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        RELAY_WIRE_BUS.beginTransmission(addr);
        if (RELAY_WIRE_BUS.endTransmission() == 0) {
            EventLog::log(LOG_INFO, "I2C: peripherique trouve a 0x%02X", addr);
            found++;
        }
    }
    EventLog::log(found > 0 ? LOG_INFO : LOG_WARN,
                  "I2C: scan termine, %u peripherique(s)", found);

    configMgr.begin();
    NotificationManager::bindConfig(&configMgr);
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

    relaisMgr.setI2cExpanderSharedOutputState(&sharedOutputState);
    relaisMgr.begin(&configMgr);
    // Couche E/S TOR (MCP23017 configurables) : inerte tant qu'aucune
    // carte n'est declaree. Apres relaisMgr.begin : le bus I2C est pret.
    ioExpander.begin(&relaisMgr);

    const bool v4PilotReady = v4PilotRuntime.begin(
        relaisMgr.topology(),
        sharedOutputState
    );
    if (v4PilotReady) {
        outputAdapter.setPhysicalBackend(&v4PilotRuntime.backend());
        // Sens des broches et etat de repos poses des le demarrage, par V4.
        // C'etait RelaisManager::initBoard() qui le faisait ; le faire ici
        // est ce qui permet a l'etage I2C historique de disparaitre.
        const size_t configured =
            v4PilotRuntime.backend().configureAllZones(configMgr.nbZones());
        EventLog::log(LOG_INFO,
                      "Relais V4: %u voie(s) configuree(s) au demarrage",
                      (unsigned)configured);
        // Le decompte reel est journalise par V4PilotRuntime::begin() : ne
        // pas affirmer ici une couverture qui n'a jamais ete verifiee.
        EventLog::log(LOG_INFO, "Relais V4: moteur V4 actif sur les sorties");
    } else {
        // AUCUNE bascule vers le moteur historique : il n'existe plus dans ce
        // firmware. Si le pilote V4 ne demarre pas, le module ne pilote rien
        // et le dit -- un module muet qu'on repare vaut mieux qu'un module qui
        // marche par un chemin dont personne ne sait qu'il est emprunte.
        FaultManager::setActive(FaultId::RELAY_I2C, true);
        EventLog::log(LOG_ERROR,
                      "Relais V4: pilote indisponible, aucune sortie pilotable");
    }

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

    EquipmentManager* orchestratorShadowManager = shadowPumpScenarioReady
        ? &shadowEquipmentMgr
        : &equipmentMgr;
    equipmentOrchestrator.begin(orchestratorShadowManager, nbZones);
    equipmentOrchestratorShadowReady = equipmentOrchestrator.isInitialized();
    EventLog::log(
        equipmentOrchestratorShadowReady ? LOG_INFO : LOG_WARN,
        "Orchestrator shadow: status=%s source=%s authority=no zones=%u",
        equipmentOrchestratorShadowReady ? "ready" : "unavailable",
        shadowPumpScenarioReady ? "pump_shadow" : "runtime_model",
        static_cast<unsigned>(nbZones)
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
    updateCheckScheduler.begin();
    cloudSyncScheduler.setConfigTarget(&configMgr);
    cloudSyncScheduler.setScheduleTarget(&scheduleMgr);
    cloudSyncScheduler.begin();
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
        &configMgr,
        &wifiMgr
    );
    webMgr.setDisplay(&displayMgr);
    webMgr.setUpdateCheckScheduler(&updateCheckScheduler);
    webMgr.setCloudSyncScheduler(&cloudSyncScheduler);
    webMgr.setIoExpander(&ioExpander);
    // L'echantillonneur lit la broche ; le reste du module lit
    // l'echantillonneur. Personne d'autre ne touche l'entree.
    inputSampler.begin(&relaisMgr.topology(), [](uint16_t id, bool& active) {
        return v4PilotRuntime.readInputById(id, active);
    });
    webMgr.setInputSampler(&inputSampler);

    EventLog::log(LOG_INFO, "Main: setup termine, boucle demarree");
    // N annonce que ce dont ce message est sur. Le perimetre pilote depend
    // du masque de zones migrees, qui evolue : le graver ici avait deja
    // produit un journal contradictoire au Gate 3 (le demarrage annoncait
    // a la fois "toutes les zones" et "la zone 1 seule"). Le perimetre
    // exact est journalise par V4PilotRuntime, qui, lui, le connait.
    EventLog::log(LOG_INFO,
                  "Parite: plan V4 vs table de cablage a chaque decision");
    EventLog::log(LOG_INFO, "HW: PSRAM %u octets", AquaLook::Heap::totalPsramBytes());
}

void loop() {
    SystemDiagnostics::loopEnter();

    BootLoopGuard::update();
    OtaBootGuard::update();
    configMgr.update();  // applique une sauvegarde NVS differee en attente, si echue

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

    // Date le resultat de maintenance des que l'heure est connue. Le mode
    // maintenance, ou ce resultat est ecrit, n'a pas d'horloge fiable : sans
    // ce rattrapage l'interface affiche "date inconnue" pour une operation
    // faite trente secondes plus tot. Une seule fois par demarrage, et la
    // fonction ne reecrit rien si la date est deja posee.
    static bool s_resultDateStamped = false;
    if (!s_resultDateStamped && ntpMgr.isSynced()) {
        s_resultDateStamped = true;
        MaintenanceResultStore::stampDateIfMissing();
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

    // Verification periodique des mises a jour. Place APRES le planificateur :
    // si l'echeance tombe pile au demarrage d'un creneau, l'arrosage est deja
    // lance et la vanne ouverte fait echouer la condition de declenchement.
    // L'ordre inverse laisserait une fenetre d'un tour de boucle pendant
    // laquelle le module redemarrerait juste avant d'ouvrir la vanne.
    updateCheckScheduler.update(
        ntpMgr.isSynced(),
        ntpMgr.getHour(),
        ntpMgr.getMinute(),
        ntpMgr.getEpochDay(),
        &wifiMgr,
        &relaisMgr,
        &configMgr
    );

    // Meme emplacement et meme raison que ci-dessus : apres le planificateur,
    // jamais avant.
    cloudSyncScheduler.update(
        ntpMgr.isSynced(),
        static_cast<uint32_t>(time(nullptr)),
        &wifiMgr,
        &relaisMgr,
        &configMgr
    );

    startedUs = RuntimeProfiler::start();
    executionShadowRuntime.update(millis());
    RuntimeProfiler::stop(RuntimeProfiler::Component::EQUIPMENT_SHADOW, startedUs);

    startedUs = RuntimeProfiler::start();
    // Avant relaisMgr.update() : une entree qui vient de basculer doit etre
    // connue AVANT que quoi que ce soit ne decide sur sa foi.
    inputSampler.update(millis());

    relaisMgr.update();

    // Coupure de securite : RelaisManager signale les zones qui ont depasse la
    // duree maximale, on les coupe par le chemin de pilotage NORMAL. Avant, la
    // securite commandait l'etage I2C historique directement -- un chemin que
    // le firmware V4 n'empruntait plus jamais, donc que personne ne testait.
    {
        uint16_t safetyCut = relaisMgr.consumeSafetyCutMask();
        for (uint8_t z = 0U; safetyCut != 0U && z < MAX_ZONES; ++z) {
            if ((safetyCut & (1U << z)) == 0U) continue;
            safetyCut = static_cast<uint16_t>(safetyCut & ~(1U << z));
            outputAdapter.setZoneValve(z, false, millis());
            displayMgr.requestDynamicRefresh();
        }
    }
    RuntimeProfiler::stop(RuntimeProfiler::Component::RELAY, startedUs);

    // Couche E/S TOR : scrute les entrees a sa propre periode, pilote les
    // sorties. Non bloquant, inerte si desactivee. Apres relaisMgr.update()
    // pour que l'etat des zones (gating presence-vanne) soit a jour.
    ioExpander.update(millis(), configMgr.nbZones());

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
