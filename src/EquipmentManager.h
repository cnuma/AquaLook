#pragma once

#include <Arduino.h>
#include "EquipmentModel.h"
#include "RelayTopology.h"

class RelaisManager;

// Orchestration des équipements AquaLook.
// À ce stade, seules les électrovannes de zones sont exécutables.
// Les dépendances pompe sont résolues mais pas encore orchestrées.
class EquipmentManager {
public:
    enum ActionResult : uint8_t {
        ACTION_OK = 0,
        ACTION_NOT_INITIALIZED,
        ACTION_INVALID_ZONE,
        ACTION_ZONE_LINK_NOT_FOUND,
        ACTION_EQUIPMENT_NOT_FOUND,
        ACTION_INVALID_EQUIPMENT,
        ACTION_RELAY_MAPPING_NOT_FOUND,
        ACTION_RELAY_ROLE_MISMATCH,
        ACTION_EXECUTOR_NOT_CONNECTED,
        ACTION_EXECUTION_FAILED
    };

    struct EquipmentResolution {
        ActionResult result;
        uint8_t equipmentIndex;
        uint8_t relayAssignmentIndex;
        RelayTopology::MappingResolution relay;

        EquipmentResolution();
        bool valid() const;
    };

    struct ZoneDependencyResolution {
        ActionResult result;
        uint8_t linkIndex;
        EquipmentResolution valve;
        bool requiresPump;
        EquipmentResolution pump;

        ZoneDependencyResolution();
        bool valid() const;
    };

    using ZoneResolution = EquipmentResolution;

    void begin(
        const EquipmentModel::EquipmentConfigSet* equipmentModel,
        const RelayTopology::RelayTopologyConfig* relayTopology,
        uint8_t nbZones,
        RelaisManager* relayExecutor = nullptr
    );

    void setRelayExecutor(RelaisManager* relayExecutor);
    bool isInitialized() const;
    bool hasRelayExecutor() const;
    ZoneResolution resolveZone(uint8_t zone) const;
    ZoneDependencyResolution resolveZoneDependencies(uint8_t zone) const;

    ActionResult startZone(uint8_t zone);
    ActionResult stopZone(uint8_t zone);

private:
    const EquipmentModel::EquipmentConfigSet* _equipmentModel = nullptr;
    const RelayTopology::RelayTopologyConfig* _relayTopology = nullptr;
    RelaisManager* _relayExecutor = nullptr;
    uint8_t _nbZones = 0;

    ActionResult resolveEquipment(
        uint8_t equipmentIndex,
        EquipmentResolution& resolution
    ) const;
    ActionResult validateZoneRequest(uint8_t zone, ZoneResolution& resolution) const;
    ActionResult executeZone(uint8_t zone, bool state);
};
