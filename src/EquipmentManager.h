#pragma once

#include <Arduino.h>
#include "EquipmentModel.h"
#include "RelayTopology.h"

// Squelette d'orchestration des équipements.
// Aucun appel matériel n'est effectué dans ce run : RelaisManager,
// ScheduleManager, ConfigManager, le Web et le NVS restent inchangés.
class EquipmentManager {
public:
    enum ActionResult : uint8_t {
        ACTION_OK = 0,
        ACTION_NOT_INITIALIZED,
        ACTION_INVALID_ZONE,
        ACTION_EQUIPMENT_NOT_FOUND,
        ACTION_INVALID_EQUIPMENT,
        ACTION_RELAY_MAPPING_NOT_FOUND,
        ACTION_RELAY_ROLE_MISMATCH,
        ACTION_EXECUTOR_NOT_CONNECTED
    };

    struct ZoneResolution {
        ActionResult result;
        uint8_t equipmentIndex;
        uint8_t relayAssignmentIndex;
        RelayTopology::MappingResolution relay;

        ZoneResolution();
        bool valid() const;
    };

    void begin(
        const EquipmentModel::EquipmentConfigSet* equipmentModel,
        const RelayTopology::RelayTopologyConfig* relayTopology,
        uint8_t nbZones
    );

    bool isInitialized() const;
    ZoneResolution resolveZone(uint8_t zone) const;

    // API réservée à la future orchestration pompe/électrovanne.
    // Elle valide actuellement le modèle puis indique explicitement
    // qu'aucun exécuteur matériel n'est encore connecté.
    ActionResult startZone(uint8_t zone);
    ActionResult stopZone(uint8_t zone);

private:
    const EquipmentModel::EquipmentConfigSet* _equipmentModel = nullptr;
    const RelayTopology::RelayTopologyConfig* _relayTopology = nullptr;
    uint8_t _nbZones = 0;

    ActionResult validateZoneRequest(uint8_t zone, ZoneResolution& resolution) const;
};
