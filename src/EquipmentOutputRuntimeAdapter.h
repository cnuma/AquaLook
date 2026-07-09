#pragma once

#include <stdint.h>

#include "domain/EquipmentOutputTypes.h"
#include "domain/EquipmentRuntimeState.h"

class RelaisManager;

namespace AquaLook { namespace Runtime {

class RelayPhysicalBackend;

class EquipmentOutputRuntimeAdapter {
public:
    void bind(RelaisManager* relayManager);
    void setPhysicalBackend(RelayPhysicalBackend* physicalBackend);
    bool isBound() const;

    Domain::OperationResult command(
        const Domain::EquipmentOutputCommand& command,
        uint32_t nowMs = 0U
    );

    Domain::OperationResult setZoneValve(
        uint8_t zoneIndex,
        bool active,
        uint32_t nowMs = 0U
    );

    Domain::EquipmentStateValue getZoneValveState(uint8_t zoneIndex) const;

private:
    RelaisManager* _relayManager = nullptr;
    RelayPhysicalBackend* _physicalBackend = nullptr;
    uint16_t _nextExecutionValue = 1U;

    Domain::ExecutionId nextExecutionId();
    static Domain::OperationResult rejected(
        Domain::EquipmentId equipmentId,
        Domain::OperationError error,
        uint32_t nowMs
    );
};

}} // namespace AquaLook::Runtime
