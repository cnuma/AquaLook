#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/DomainIdentifiers.h"
#include "domain/HardwareInventoryModel.h"

namespace AquaLook { namespace Domain {

enum class BoardStatus : uint8_t {
    DISABLED = 0,
    CONFIGURED = 1,
    AVAILABLE = 2,
    UNAVAILABLE = 3,
    FAULTED = 4
};

enum BoardFlags : uint8_t {
    BOARD_FLAG_NONE = 0U,
    BOARD_FLAG_ENABLED = 1U << 0,
    BOARD_FLAG_EXTERNAL = 1U << 1,
    BOARD_FLAG_HOTPLUGGABLE = 1U << 2
};

struct BoardDefinition {
    BoardId id;
    BoardTypeId typeId;
    ControllerId controllerId;
    uint16_t modelVersion;
    uint16_t firstPortIndex;
    uint16_t portCount;
    BoardStatus status;
    uint8_t flags;
    uint16_t reserved;

    constexpr BoardDefinition()
        : id(), typeId(), controllerId(), modelVersion(0U), firstPortIndex(0U),
          portCount(0U), status(BoardStatus::DISABLED), flags(BOARD_FLAG_NONE),
          reserved(0U) {}
};

using PortCapabilityMask = uint32_t;

enum PortCapability : PortCapabilityMask {
    PORT_CAP_NONE = 0U,
    PORT_CAP_DIGITAL_INPUT = 1UL << 0,
    PORT_CAP_DIGITAL_OUTPUT = 1UL << 1,
    PORT_CAP_PWM_OUTPUT = 1UL << 2,
    PORT_CAP_COUNTER_INPUT = 1UL << 3,
    PORT_CAP_ANALOG_INPUT = 1UL << 4,
    PORT_CAP_INTERRUPT_INPUT = 1UL << 5,
    PORT_CAP_RELAY_OUTPUT = 1UL << 6,
    PORT_CAP_SENSOR_INPUT = 1UL << 7
};

enum class PortType : uint8_t {
    UNKNOWN = 0,
    DIGITAL = 1,
    PWM = 2,
    COUNTER = 3,
    ANALOG = 4,
    RELAY = 5,
    SENSOR = 6,
    VIRTUAL = 7
};

enum class PortDirection : uint8_t {
    UNSPECIFIED = 0,
    INPUT = 1,
    OUTPUT = 2,
    BIDIRECTIONAL = 3
};

enum class PortSafeState : uint8_t {
    UNSPECIFIED = 0,
    INACTIVE = 1,
    ACTIVE = 2,
    HIGH_IMPEDANCE = 3,
    HOLD_LAST = 4
};

enum PortFlags : uint8_t {
    PORT_FLAG_NONE = 0U,
    PORT_FLAG_ENABLED = 1U << 0,
    PORT_FLAG_INVERTED = 1U << 1,
    PORT_FLAG_SHARED_CHANNEL = 1U << 2,
    PORT_FLAG_CRITICAL = 1U << 3
};

struct PortDefinition {
    ControllerId controllerId;
    BoardId boardId;
    PortId id;
    uint16_t channel;
    PortCapabilityMask capabilities;
    PortType type;
    PortDirection direction;
    PortSafeState safeState;
    uint8_t flags;

    constexpr PortDefinition()
        : controllerId(), boardId(), id(), channel(0U), capabilities(PORT_CAP_NONE),
          type(PortType::UNKNOWN), direction(PortDirection::UNSPECIFIED),
          safeState(PortSafeState::UNSPECIFIED), flags(PORT_FLAG_NONE) {}
};

enum class BoardPortValidationError : uint8_t {
    NONE = 0,
    INVALID_BOARD_ID,
    INVALID_BOARD_TYPE,
    ORPHAN_CONTROLLER,
    INVALID_BOARD_STATUS,
    INVALID_PORT_RANGE,
    DUPLICATE_BOARD_ID,
    INVALID_PORT_ID,
    ORPHAN_BOARD,
    PORT_CONTROLLER_MISMATCH,
    INVALID_PORT_TYPE,
    INVALID_PORT_DIRECTION,
    INVALID_SAFE_STATE,
    INVALID_PORT_CAPABILITY,
    UNSUPPORTED_PORT_CAPABILITY,
    CHANNEL_OUT_OF_RANGE,
    DUPLICATE_PORT_ID,
    CHANNEL_COLLISION
};

struct BoardPortValidationResult {
    BoardPortValidationError error;
    uint16_t index;
    BoardId boardId;
    PortId portId;

    constexpr BoardPortValidationResult(
        BoardPortValidationError value = BoardPortValidationError::NONE,
        uint16_t itemIndex = 0U,
        BoardId board = BoardId(),
        PortId port = PortId()
    ) : error(value), index(itemIndex), boardId(board), portId(port) {}

    constexpr bool ok() const { return error == BoardPortValidationError::NONE; }
};

bool isKnownPortType(PortType type);
bool isKnownPortDirection(PortDirection direction);
bool isKnownPortSafeState(PortSafeState state);

BoardPortValidationResult validateBoardPortInventory(
    const ControllerDefinition* controllers,
    size_t controllerCount,
    const BoardDefinition* boards,
    size_t boardCount,
    const PortDefinition* ports,
    size_t portCount
);

static_assert(sizeof(BoardDefinition) == 16U, "BoardDefinition layout changed");
static_assert(sizeof(PortDefinition) == 16U, "PortDefinition layout changed");

}} // namespace AquaLook::Domain
