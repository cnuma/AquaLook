#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/BoardPortModel.h"
#include "domain/ProtocolBuildProfile.h"

namespace AquaLook { namespace Domain {

namespace ControllerTypeIds {
constexpr ControllerTypeId LOCAL_GPIO(1U);
constexpr ControllerTypeId XL9535(2U);
constexpr ControllerTypeId MCP23017(3U);
constexpr ControllerTypeId MCP23S17(4U);
constexpr ControllerTypeId REMOTE_GENERIC(5U);
}

namespace BoardTypeIds {
constexpr BoardTypeId LOCAL_GPIO_BANK(1U);
constexpr BoardTypeId RELAY_8_XL9535(2U);
constexpr BoardTypeId IO_16_MCP23017(3U);
constexpr BoardTypeId IO_16_MCP23S17(4U);
constexpr BoardTypeId REMOTE_GENERIC(5U);
}

struct ControllerTypeDescriptor {
    ControllerTypeId id;
    BusType requiredBusType;
    ControllerCapabilityMask capabilities;
    uint16_t minimumChannels;
    uint16_t maximumChannels;
    uint32_t minimumAddress;
    uint32_t maximumAddress;
    const char* technicalName;
};

struct BoardTypeDescriptor {
    BoardTypeId id;
    ControllerTypeId requiredControllerType;
    PortCapabilityMask portCapabilities;
    uint16_t minimumPorts;
    uint16_t maximumPorts;
    uint16_t modelVersion;
    const char* technicalName;
};

const ControllerTypeDescriptor* controllerTypeCatalog(size_t& count);
const BoardTypeDescriptor* boardTypeCatalog(size_t& count);

const ControllerTypeDescriptor* findControllerTypeDescriptor(ControllerTypeId id);
const BoardTypeDescriptor* findBoardTypeDescriptor(BoardTypeId id);

bool isControllerTypeCompiled(ControllerTypeId id);
bool isBoardTypeCompiled(BoardTypeId id);

BuildProfileValidationResult validateInventoryAgainstBuildProfile(
    const BusDefinition* buses,
    size_t busCount,
    const ControllerDefinition* controllers,
    size_t controllerCount,
    const BoardDefinition* boards,
    size_t boardCount
);

}} // namespace AquaLook::Domain
