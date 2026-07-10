#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/BinaryActuatorDriverRegistry.h"
#include "domain/BoardPortModel.h"
#include "domain/HardwareInventoryModel.h"
#include "domain/Xl9535BinaryActuatorDriver.h"
#include "domain/Xl9535SharedOutputState.h"
#include "RelayTopology.h"
#include "V4RelayPhysicalBackend.h"

namespace AquaLook { namespace Runtime {

class V4PilotRuntime {
public:
    V4PilotRuntime();

    bool begin(
        const RelayTopology::RelayTopologyConfig& topology,
        Domain::Xl9535SharedOutputState& sharedOutputState
    );

    bool isReady() const;
    V4RelayPhysicalBackend& backend();

private:
    static constexpr size_t CONTROLLER_COUNT = 1U;
    static constexpr size_t BOARD_COUNT = 1U;
    static constexpr size_t PORT_COUNT = 16U;
    static constexpr size_t DRIVER_CAPACITY = 1U;

    Domain::ControllerDefinition _controllers[CONTROLLER_COUNT];
    Domain::BoardDefinition _boards[BOARD_COUNT];
    Domain::PortDefinition _ports[PORT_COUNT];
    Domain::BinaryActuatorDriverBinding _driverStorage[DRIVER_CAPACITY];
    Domain::BinaryActuatorDriverRegistry _driverRegistry;
    Domain::Xl9535BinaryActuatorContext _xl9535Context;
    V4RelayPhysicalBackend _backend;
    bool _ready;
};

}} // namespace AquaLook::Runtime
