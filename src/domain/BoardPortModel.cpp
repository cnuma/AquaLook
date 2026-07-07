#include "domain/BoardPortModel.h"

namespace AquaLook { namespace Domain {

static int findControllerIndex(
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

static int findBoardIndex(BoardId id, const BoardDefinition* boards, size_t boardCount) {
    if (!boards) return -1;
    for (size_t i = 0U; i < boardCount; ++i) {
        if (boards[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

static PortCapabilityMask mapControllerCapabilities(ControllerCapabilityMask capabilities) {
    PortCapabilityMask mapped = PORT_CAP_NONE;
    if ((capabilities & CONTROLLER_CAP_DIGITAL_INPUT) != 0U) mapped |= PORT_CAP_DIGITAL_INPUT;
    if ((capabilities & CONTROLLER_CAP_DIGITAL_OUTPUT) != 0U) mapped |= PORT_CAP_DIGITAL_OUTPUT;
    if ((capabilities & CONTROLLER_CAP_PWM_OUTPUT) != 0U) mapped |= PORT_CAP_PWM_OUTPUT;
    if ((capabilities & CONTROLLER_CAP_COUNTER_INPUT) != 0U) mapped |= PORT_CAP_COUNTER_INPUT;
    if ((capabilities & CONTROLLER_CAP_ANALOG_INPUT) != 0U) mapped |= PORT_CAP_ANALOG_INPUT;
    if ((capabilities & CONTROLLER_CAP_INTERRUPT_INPUT) != 0U) mapped |= PORT_CAP_INTERRUPT_INPUT;
    if ((capabilities & CONTROLLER_CAP_RELAY_OUTPUT) != 0U) mapped |= PORT_CAP_RELAY_OUTPUT;
    if ((capabilities & CONTROLLER_CAP_SENSOR_INPUT) != 0U) mapped |= PORT_CAP_SENSOR_INPUT;
    return mapped;
}

bool isKnownPortType(PortType type) {
    return type == PortType::DIGITAL ||
           type == PortType::PWM ||
           type == PortType::COUNTER ||
           type == PortType::ANALOG ||
           type == PortType::RELAY ||
           type == PortType::SENSOR ||
           type == PortType::VIRTUAL;
}

bool isKnownPortDirection(PortDirection direction) {
    return direction == PortDirection::INPUT ||
           direction == PortDirection::OUTPUT ||
           direction == PortDirection::BIDIRECTIONAL;
}

bool isKnownPortSafeState(PortSafeState state) {
    return state == PortSafeState::UNSPECIFIED ||
           state == PortSafeState::INACTIVE ||
           state == PortSafeState::ACTIVE ||
           state == PortSafeState::HIGH_IMPEDANCE ||
           state == PortSafeState::HOLD_LAST;
}

BoardPortValidationResult validateBoardPortInventory(
    const ControllerDefinition* controllers,
    size_t controllerCount,
    const BoardDefinition* boards,
    size_t boardCount,
    const PortDefinition* ports,
    size_t portCount
) {
    if ((controllerCount != 0U && !controllers) ||
        (boardCount != 0U && !boards) ||
        (portCount != 0U && !ports)) {
        return BoardPortValidationError::ORPHAN_CONTROLLER;
    }

    for (size_t i = 0U; i < boardCount; ++i) {
        const BoardDefinition& board = boards[i];
        if (!board.id.isValid()) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_BOARD_ID,
                static_cast<uint16_t>(i), board.id
            );
        }
        if (!board.typeId.isValid()) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_BOARD_TYPE,
                static_cast<uint16_t>(i), board.id
            );
        }
        if (findControllerIndex(board.controllerId, controllers, controllerCount) < 0) {
            return BoardPortValidationResult(
                BoardPortValidationError::ORPHAN_CONTROLLER,
                static_cast<uint16_t>(i), board.id
            );
        }
        if (board.status != BoardStatus::DISABLED &&
            board.status != BoardStatus::CONFIGURED &&
            board.status != BoardStatus::AVAILABLE &&
            board.status != BoardStatus::UNAVAILABLE &&
            board.status != BoardStatus::FAULTED) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_BOARD_STATUS,
                static_cast<uint16_t>(i), board.id
            );
        }
        const size_t rangeEnd = static_cast<size_t>(board.firstPortIndex) +
                                static_cast<size_t>(board.portCount);
        if (board.portCount == 0U || rangeEnd > portCount) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_PORT_RANGE,
                static_cast<uint16_t>(i), board.id
            );
        }
        for (size_t j = 0U; j < i; ++j) {
            if (boards[j].id == board.id) {
                return BoardPortValidationResult(
                    BoardPortValidationError::DUPLICATE_BOARD_ID,
                    static_cast<uint16_t>(i), board.id
                );
            }
        }
    }

    for (size_t i = 0U; i < portCount; ++i) {
        const PortDefinition& port = ports[i];
        if (!port.id.isValid()) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_PORT_ID,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }
        const int boardIndex = findBoardIndex(port.boardId, boards, boardCount);
        if (boardIndex < 0) {
            return BoardPortValidationResult(
                BoardPortValidationError::ORPHAN_BOARD,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }
        const BoardDefinition& board = boards[boardIndex];
        if (port.controllerId != board.controllerId) {
            return BoardPortValidationResult(
                BoardPortValidationError::PORT_CONTROLLER_MISMATCH,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }
        if (!isKnownPortType(port.type)) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_PORT_TYPE,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }
        if (!isKnownPortDirection(port.direction)) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_PORT_DIRECTION,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }
        if (!isKnownPortSafeState(port.safeState)) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_SAFE_STATE,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }
        if (port.capabilities == PORT_CAP_NONE) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_PORT_CAPABILITY,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }

        const int controllerIndex = findControllerIndex(
            port.controllerId, controllers, controllerCount
        );
        if (controllerIndex < 0) {
            return BoardPortValidationResult(
                BoardPortValidationError::ORPHAN_CONTROLLER,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }
        const ControllerDefinition& controller = controllers[controllerIndex];
        if (port.channel >= controller.channelCount) {
            return BoardPortValidationResult(
                BoardPortValidationError::CHANNEL_OUT_OF_RANGE,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }
        const PortCapabilityMask supported = mapControllerCapabilities(controller.capabilities);
        if ((port.capabilities & ~supported) != 0U) {
            return BoardPortValidationResult(
                BoardPortValidationError::UNSUPPORTED_PORT_CAPABILITY,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }

        const bool hasInput = (port.capabilities &
            (PORT_CAP_DIGITAL_INPUT | PORT_CAP_COUNTER_INPUT | PORT_CAP_ANALOG_INPUT |
             PORT_CAP_INTERRUPT_INPUT | PORT_CAP_SENSOR_INPUT)) != 0U;
        const bool hasOutput = (port.capabilities &
            (PORT_CAP_DIGITAL_OUTPUT | PORT_CAP_PWM_OUTPUT | PORT_CAP_RELAY_OUTPUT)) != 0U;
        if ((port.direction == PortDirection::INPUT && !hasInput) ||
            (port.direction == PortDirection::OUTPUT && !hasOutput) ||
            (port.direction == PortDirection::BIDIRECTIONAL && !(hasInput && hasOutput))) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_PORT_CAPABILITY,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }

        const size_t first = board.firstPortIndex;
        const size_t end = first + board.portCount;
        if (i < first || i >= end) {
            return BoardPortValidationResult(
                BoardPortValidationError::INVALID_PORT_RANGE,
                static_cast<uint16_t>(i), port.boardId, port.id
            );
        }

        for (size_t j = 0U; j < i; ++j) {
            if (ports[j].id == port.id) {
                return BoardPortValidationResult(
                    BoardPortValidationError::DUPLICATE_PORT_ID,
                    static_cast<uint16_t>(i), port.boardId, port.id
                );
            }
            if (ports[j].controllerId == port.controllerId &&
                ports[j].channel == port.channel &&
                (((ports[j].flags | port.flags) & PORT_FLAG_SHARED_CHANNEL) == 0U)) {
                return BoardPortValidationResult(
                    BoardPortValidationError::CHANNEL_COLLISION,
                    static_cast<uint16_t>(i), port.boardId, port.id
                );
            }
        }
    }

    return BoardPortValidationError::NONE;
}

}} // namespace AquaLook::Domain
