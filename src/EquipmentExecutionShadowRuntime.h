#pragma once

#include <stdint.h>

#include "EquipmentExecutionEngine.h"

namespace AquaLook { namespace Runtime {

// Supervise un moteur passif par zone afin de préserver le parallélisme
// du programmateur. Ce composant observe les plans mais ne commande aucun
// équipement et ne possède aucun accès aux backends matériels.
class EquipmentExecutionShadowRuntime {
public:
    EquipmentExecutionShadowRuntime();

    void begin(uint8_t nbZones);

    bool submit(
        uint8_t zone,
        const EquipmentManager::ZoneExecutionPlan& plan,
        bool starting,
        uint32_t nowMs
    );

    void update(uint32_t nowMs);

    bool isEnabled() const;
    uint8_t zoneCount() const;

private:
    struct ZoneSlot {
        EquipmentExecutionEngine engine;
        PassiveExecutionState observedState;
        uint8_t observedStep;
        bool occupied;

        ZoneSlot();
    };

    ZoneSlot _slots[MAX_ZONES];
    uint8_t _nbZones;
    uint16_t _nextActivityId;
    uint16_t _nextExecutionId;
    bool _enabled;

    static uint16_t nextValidId(uint16_t current);
    void logProgress(uint8_t zone, ZoneSlot& slot, uint32_t nowMs);
};

}} // namespace AquaLook::Runtime
