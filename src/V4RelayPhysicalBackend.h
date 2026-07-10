#pragma once

#include <stddef.h>
#include <stdint.h>

#include "RelayPhysicalBackend.h"
#include "domain/BinaryActuatorDriver.h"
#include "domain/BinaryActuatorDriverRegistry.h"
#include "domain/BoardPortModel.h"
#include "domain/HardwareInventoryModel.h"
#include "RelayTopology.h"
#include "config.h"

namespace AquaLook { namespace Runtime {

class V4RelayPhysicalBackend : public RelayPhysicalBackend {
public:
    V4RelayPhysicalBackend() = default;

    void bind(
        const RelayTopology::RelayTopologyConfig* topology,
        const Domain::ControllerDefinition* controllers,
        size_t controllerCount,
        const Domain::BoardDefinition* boards,
        size_t boardCount,
        const Domain::PortDefinition* ports,
        size_t portCount,
        Domain::BinaryActuatorDriverRegistry* driverRegistry
    );

    bool isReady() const;

    bool setZoneValve(
        uint8_t zoneIndex,
        bool active,
        uint32_t nowMs = 0U
    ) override;

    bool getZoneValveState(
        uint8_t zoneIndex,
        bool& active
    ) const override;

    void setMigratedZoneMask(uint32_t mask) {
        _migratedZoneMask = mask;
    }

    bool isZoneMigrated(uint8_t zoneIndex) const;
    bool hasAnyMigratedZone() const;

private:
    struct ResolvedZoneTarget {
        const Domain::ControllerDefinition* controller = nullptr;
        const Domain::BoardDefinition* board = nullptr;
        const Domain::PortDefinition* port = nullptr;
        Domain::BinaryActuatorDriverBinding* driver = nullptr;

        bool valid() const {
            return controller != nullptr && board != nullptr &&
                   port != nullptr && driver != nullptr;
        }
    };

    bool resolveZoneTarget(uint8_t zoneIndex, ResolvedZoneTarget& target) const;
    const Domain::ControllerDefinition* findController(
        Domain::ControllerId controllerId
    ) const;
    const Domain::BoardDefinition* findBoardByTopologyIndex(
        uint8_t boardIndex
    ) const;
    const Domain::PortDefinition* findPort(
        const Domain::BoardDefinition& board,
        uint8_t channelIndex
    ) const;

    const RelayTopology::RelayTopologyConfig* _topology = nullptr;
    const Domain::ControllerDefinition* _controllers = nullptr;
    size_t _controllerCount = 0U;
    const Domain::BoardDefinition* _boards = nullptr;
    size_t _boardCount = 0U;
    const Domain::PortDefinition* _ports = nullptr;
    size_t _portCount = 0U;
    Domain::BinaryActuatorDriverRegistry* _driverRegistry = nullptr;
    mutable Domain::BinaryActuatorSession _sessions[MAX_ZONES];
    uint32_t _migratedZoneMask = 0U;
};

}} // namespace AquaLook::Runtime
