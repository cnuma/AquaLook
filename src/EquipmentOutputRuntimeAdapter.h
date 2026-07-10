#pragma once

#include <stdint.h>

#include "domain/EquipmentOutputTypes.h"
#include "domain/EquipmentRuntimeState.h"

class RelaisManager;

namespace AquaLook { namespace Runtime {

class RelayPhysicalBackend;

class EquipmentOutputRuntimeAdapter {
public:
    enum class ExecutionPath : uint8_t {
        NONE = 0,
        PHYSICAL_BACKEND = 1,
        RELAY_MANAGER_FALLBACK = 2,
        FAILED = 3
    };

    struct ExecutionCounters {
        uint32_t physicalBackend;
        uint32_t relayManagerFallback;
        uint32_t failed;

        constexpr ExecutionCounters()
            : physicalBackend(0U), relayManagerFallback(0U), failed(0U) {}
    };

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
    ExecutionPath lastExecutionPath() const;
    const ExecutionCounters& executionCounters() const;
    static const char* executionPathName(ExecutionPath path);

private:
    RelaisManager* _relayManager = nullptr;
    RelayPhysicalBackend* _physicalBackend = nullptr;
    uint16_t _nextExecutionValue = 1U;
    ExecutionPath _lastExecutionPath = ExecutionPath::NONE;
    ExecutionCounters _executionCounters;

    Domain::ExecutionId nextExecutionId();
    void recordExecutionPath(ExecutionPath path);
    static Domain::OperationResult rejected(
        Domain::EquipmentId equipmentId,
        Domain::OperationError error,
        uint32_t nowMs
    );
};

}} // namespace AquaLook::Runtime
