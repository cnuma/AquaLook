#include "domain/HardwareCatalog.h"

namespace AquaLook { namespace Domain {

namespace {

constexpr ControllerTypeDescriptor CONTROLLER_CATALOG[] = {
    {
        ControllerTypeIds::LOCAL_GPIO,
        BusType::GPIO,
        CONTROLLER_CAP_DIGITAL_INPUT | CONTROLLER_CAP_DIGITAL_OUTPUT |
            CONTROLLER_CAP_PWM_OUTPUT | CONTROLLER_CAP_COUNTER_INPUT |
            CONTROLLER_CAP_ANALOG_INPUT | CONTROLLER_CAP_INTERRUPT_INPUT,
        1U,
        64U,
        0U,
        0U,
        "local_gpio"
    },
    {
        ControllerTypeIds::XL9535,
        BusType::I2C,
        CONTROLLER_CAP_DIGITAL_INPUT | CONTROLLER_CAP_DIGITAL_OUTPUT |
            CONTROLLER_CAP_RELAY_OUTPUT,
        8U,
        16U,
        0x20U,
        0x27U,
        "xl9535"
    },
    {
        ControllerTypeIds::MCP23017,
        BusType::I2C,
        CONTROLLER_CAP_DIGITAL_INPUT | CONTROLLER_CAP_DIGITAL_OUTPUT |
            CONTROLLER_CAP_INTERRUPT_INPUT | CONTROLLER_CAP_RELAY_OUTPUT |
            CONTROLLER_CAP_COUNTER_INPUT,
        16U,
        16U,
        0x20U,
        0x27U,
        "mcp23017"
    },
    {
        ControllerTypeIds::MCP23S17,
        BusType::SPI,
        CONTROLLER_CAP_DIGITAL_INPUT | CONTROLLER_CAP_DIGITAL_OUTPUT |
            CONTROLLER_CAP_INTERRUPT_INPUT | CONTROLLER_CAP_RELAY_OUTPUT |
            CONTROLLER_CAP_COUNTER_INPUT,
        16U,
        16U,
        0U,
        7U,
        "mcp23s17"
    },
    {
        ControllerTypeIds::REMOTE_GENERIC,
        BusType::REMOTE,
        CONTROLLER_CAP_DIGITAL_INPUT | CONTROLLER_CAP_DIGITAL_OUTPUT |
            CONTROLLER_CAP_PWM_OUTPUT | CONTROLLER_CAP_COUNTER_INPUT |
            CONTROLLER_CAP_ANALOG_INPUT | CONTROLLER_CAP_INTERRUPT_INPUT |
            CONTROLLER_CAP_RELAY_OUTPUT | CONTROLLER_CAP_SENSOR_INPUT |
            CONTROLLER_CAP_REMOTE_NODE,
        1U,
        256U,
        1U,
        0xFFFFFFFFU,
        "remote_generic"
    }
};

constexpr BoardTypeDescriptor BOARD_CATALOG[] = {
    {
        BoardTypeIds::LOCAL_GPIO_BANK,
        ControllerTypeIds::LOCAL_GPIO,
        PORT_CAP_DIGITAL_INPUT | PORT_CAP_DIGITAL_OUTPUT |
            PORT_CAP_PWM_OUTPUT | PORT_CAP_COUNTER_INPUT |
            PORT_CAP_ANALOG_INPUT | PORT_CAP_INTERRUPT_INPUT,
        1U,
        64U,
        1U,
        "local_gpio_bank"
    },
    {
        BoardTypeIds::RELAY_8_XL9535,
        ControllerTypeIds::XL9535,
        PORT_CAP_DIGITAL_OUTPUT | PORT_CAP_RELAY_OUTPUT,
        1U,
        8U,
        1U,
        "relay_8_xl9535"
    },
    {
        BoardTypeIds::IO_16_MCP23017,
        ControllerTypeIds::MCP23017,
        PORT_CAP_DIGITAL_INPUT | PORT_CAP_DIGITAL_OUTPUT |
            PORT_CAP_COUNTER_INPUT | PORT_CAP_INTERRUPT_INPUT |
            PORT_CAP_RELAY_OUTPUT,
        1U,
        16U,
        1U,
        "io_16_mcp23017"
    },
    {
        BoardTypeIds::IO_16_MCP23S17,
        ControllerTypeIds::MCP23S17,
        PORT_CAP_DIGITAL_INPUT | PORT_CAP_DIGITAL_OUTPUT |
            PORT_CAP_COUNTER_INPUT | PORT_CAP_INTERRUPT_INPUT |
            PORT_CAP_RELAY_OUTPUT,
        1U,
        16U,
        1U,
        "io_16_mcp23s17"
    },
    {
        BoardTypeIds::REMOTE_GENERIC,
        ControllerTypeIds::REMOTE_GENERIC,
        PORT_CAP_DIGITAL_INPUT | PORT_CAP_DIGITAL_OUTPUT |
            PORT_CAP_PWM_OUTPUT | PORT_CAP_COUNTER_INPUT |
            PORT_CAP_ANALOG_INPUT | PORT_CAP_INTERRUPT_INPUT |
            PORT_CAP_RELAY_OUTPUT | PORT_CAP_SENSOR_INPUT,
        1U,
        256U,
        1U,
        "remote_generic"
    }
};

constexpr size_t CONTROLLER_COUNT =
    sizeof(CONTROLLER_CATALOG) / sizeof(CONTROLLER_CATALOG[0]);
constexpr size_t BOARD_COUNT = sizeof(BOARD_CATALOG) / sizeof(BOARD_CATALOG[0]);

int findBusIndex(BusId id, const BusDefinition* buses, size_t busCount) {
    if (!buses) return -1;
    for (size_t i = 0U; i < busCount; ++i) {
        if (buses[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int findControllerIndex(
    ControllerId id,
    const ControllerDefinition* controllers,
    size_t controllerCount
) {
    if (!controllers) return -1;
    for (size_t i = 0U; i < controllerCount; ++i) {
        if (controllers[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

} // namespace

const ControllerTypeDescriptor* controllerTypeCatalog(size_t& count) {
    count = CONTROLLER_COUNT;
    return CONTROLLER_CATALOG;
}

const BoardTypeDescriptor* boardTypeCatalog(size_t& count) {
    count = BOARD_COUNT;
    return BOARD_CATALOG;
}

const ControllerTypeDescriptor* findControllerTypeDescriptor(ControllerTypeId id) {
    for (size_t i = 0U; i < CONTROLLER_COUNT; ++i) {
        if (CONTROLLER_CATALOG[i].id == id) return &CONTROLLER_CATALOG[i];
    }
    return nullptr;
}

const BoardTypeDescriptor* findBoardTypeDescriptor(BoardTypeId id) {
    for (size_t i = 0U; i < BOARD_COUNT; ++i) {
        if (BOARD_CATALOG[i].id == id) return &BOARD_CATALOG[i];
    }
    return nullptr;
}

bool isControllerTypeCompiled(ControllerTypeId id) {
    const ControllerTypeDescriptor* descriptor = findControllerTypeDescriptor(id);
    return descriptor && isProtocolCompiled(descriptor->requiredBusType);
}

bool isBoardTypeCompiled(BoardTypeId id) {
    const BoardTypeDescriptor* descriptor = findBoardTypeDescriptor(id);
    return descriptor && isControllerTypeCompiled(descriptor->requiredControllerType);
}

BuildProfileValidationResult validateInventoryAgainstBuildProfile(
    const BusDefinition* buses,
    size_t busCount,
    const ControllerDefinition* controllers,
    size_t controllerCount,
    const BoardDefinition* boards,
    size_t boardCount
) {
    if ((busCount != 0U && !buses) ||
        (controllerCount != 0U && !controllers) ||
        (boardCount != 0U && !boards)) {
        return BuildProfileValidationError::UNKNOWN_BUS_TYPE;
    }

    for (size_t i = 0U; i < busCount; ++i) {
        const ProtocolMask feature = protocolFeatureForBusType(buses[i].type);
        if (feature == PROTOCOL_NONE) {
            return BuildProfileValidationResult(
                BuildProfileValidationError::UNKNOWN_BUS_TYPE,
                static_cast<uint16_t>(i), buses[i].id
            );
        }
        if (!isProtocolCompiled(buses[i].type)) {
            return BuildProfileValidationResult(
                BuildProfileValidationError::BUS_PROTOCOL_NOT_COMPILED,
                static_cast<uint16_t>(i), buses[i].id
            );
        }
    }

    for (size_t i = 0U; i < controllerCount; ++i) {
        const ControllerTypeDescriptor* descriptor =
            findControllerTypeDescriptor(controllers[i].typeId);
        if (!descriptor || !isControllerTypeCompiled(controllers[i].typeId)) {
            return BuildProfileValidationResult(
                BuildProfileValidationError::CONTROLLER_TYPE_NOT_COMPILED,
                static_cast<uint16_t>(i), controllers[i].busId, controllers[i].id
            );
        }

        const int busIndex = findBusIndex(controllers[i].busId, buses, busCount);
        if (busIndex < 0 || buses[busIndex].type != descriptor->requiredBusType) {
            return BuildProfileValidationResult(
                BuildProfileValidationError::CONTROLLER_TYPE_NOT_COMPILED,
                static_cast<uint16_t>(i), controllers[i].busId, controllers[i].id
            );
        }

        if (controllers[i].channelCount < descriptor->minimumChannels ||
            controllers[i].channelCount > descriptor->maximumChannels ||
            controllers[i].address.primary < descriptor->minimumAddress ||
            controllers[i].address.primary > descriptor->maximumAddress) {
            return BuildProfileValidationResult(
                BuildProfileValidationError::CONTROLLER_TYPE_NOT_COMPILED,
                static_cast<uint16_t>(i), controllers[i].busId, controllers[i].id
            );
        }
    }

    for (size_t i = 0U; i < boardCount; ++i) {
        const BoardTypeDescriptor* descriptor =
            findBoardTypeDescriptor(boards[i].typeId);
        if (!descriptor || !isBoardTypeCompiled(boards[i].typeId)) {
            return BuildProfileValidationResult(
                BuildProfileValidationError::BOARD_TYPE_NOT_COMPILED,
                static_cast<uint16_t>(i), BusId(), boards[i].controllerId,
                boards[i].id
            );
        }

        const int controllerIndex = findControllerIndex(
            boards[i].controllerId, controllers, controllerCount
        );
        if (controllerIndex < 0 ||
            controllers[controllerIndex].typeId != descriptor->requiredControllerType ||
            boards[i].portCount < descriptor->minimumPorts ||
            boards[i].portCount > descriptor->maximumPorts ||
            boards[i].modelVersion != descriptor->modelVersion) {
            return BuildProfileValidationResult(
                BuildProfileValidationError::BOARD_TYPE_NOT_COMPILED,
                static_cast<uint16_t>(i), BusId(), boards[i].controllerId,
                boards[i].id
            );
        }
    }

    return BuildProfileValidationError::NONE;
}

}} // namespace AquaLook::Domain
