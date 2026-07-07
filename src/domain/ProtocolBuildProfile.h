#pragma once

#include <stdint.h>

#include "domain/HardwareInventoryModel.h"

#ifndef AQUALOOK_V4_ENABLE_GPIO
#define AQUALOOK_V4_ENABLE_GPIO 1
#endif

#ifndef AQUALOOK_V4_ENABLE_I2C
#define AQUALOOK_V4_ENABLE_I2C 1
#endif

#ifndef AQUALOOK_V4_ENABLE_SPI
#define AQUALOOK_V4_ENABLE_SPI 0
#endif

#ifndef AQUALOOK_V4_ENABLE_UART
#define AQUALOOK_V4_ENABLE_UART 0
#endif

#ifndef AQUALOOK_V4_ENABLE_ONEWIRE
#define AQUALOOK_V4_ENABLE_ONEWIRE 0
#endif

#ifndef AQUALOOK_V4_ENABLE_CAN
#define AQUALOOK_V4_ENABLE_CAN 0
#endif

#ifndef AQUALOOK_V4_ENABLE_RS485
#define AQUALOOK_V4_ENABLE_RS485 0
#endif

#ifndef AQUALOOK_V4_ENABLE_REMOTE
#define AQUALOOK_V4_ENABLE_REMOTE 0
#endif

#ifndef AQUALOOK_V4_ENABLE_VIRTUAL
#define AQUALOOK_V4_ENABLE_VIRTUAL 1
#endif

namespace AquaLook { namespace Domain {

using ProtocolMask = uint16_t;

enum ProtocolFeature : ProtocolMask {
    PROTOCOL_NONE    = 0U,
    PROTOCOL_GPIO    = 1U << 0,
    PROTOCOL_I2C     = 1U << 1,
    PROTOCOL_SPI     = 1U << 2,
    PROTOCOL_UART    = 1U << 3,
    PROTOCOL_ONEWIRE = 1U << 4,
    PROTOCOL_CAN     = 1U << 5,
    PROTOCOL_RS485   = 1U << 6,
    PROTOCOL_REMOTE  = 1U << 7,
    PROTOCOL_VIRTUAL = 1U << 8
};

constexpr ProtocolMask COMPILED_PROTOCOL_MASK =
    (AQUALOOK_V4_ENABLE_GPIO ? PROTOCOL_GPIO : PROTOCOL_NONE) |
    (AQUALOOK_V4_ENABLE_I2C ? PROTOCOL_I2C : PROTOCOL_NONE) |
    (AQUALOOK_V4_ENABLE_SPI ? PROTOCOL_SPI : PROTOCOL_NONE) |
    (AQUALOOK_V4_ENABLE_UART ? PROTOCOL_UART : PROTOCOL_NONE) |
    (AQUALOOK_V4_ENABLE_ONEWIRE ? PROTOCOL_ONEWIRE : PROTOCOL_NONE) |
    (AQUALOOK_V4_ENABLE_CAN ? PROTOCOL_CAN : PROTOCOL_NONE) |
    (AQUALOOK_V4_ENABLE_RS485 ? PROTOCOL_RS485 : PROTOCOL_NONE) |
    (AQUALOOK_V4_ENABLE_REMOTE ? PROTOCOL_REMOTE : PROTOCOL_NONE) |
    (AQUALOOK_V4_ENABLE_VIRTUAL ? PROTOCOL_VIRTUAL : PROTOCOL_NONE);

constexpr ProtocolMask protocolFeatureForBusType(BusType type) {
    return type == BusType::GPIO ? PROTOCOL_GPIO :
           type == BusType::I2C ? PROTOCOL_I2C :
           type == BusType::SPI ? PROTOCOL_SPI :
           type == BusType::UART ? PROTOCOL_UART :
           type == BusType::ONEWIRE ? PROTOCOL_ONEWIRE :
           type == BusType::CAN ? PROTOCOL_CAN :
           type == BusType::RS485 ? PROTOCOL_RS485 :
           type == BusType::REMOTE ? PROTOCOL_REMOTE :
           type == BusType::VIRTUAL ? PROTOCOL_VIRTUAL : PROTOCOL_NONE;
}

constexpr bool isProtocolCompiled(BusType type) {
    return protocolFeatureForBusType(type) != PROTOCOL_NONE &&
           (COMPILED_PROTOCOL_MASK & protocolFeatureForBusType(type)) ==
               protocolFeatureForBusType(type);
}

enum class BuildProfileValidationError : uint8_t {
    NONE = 0,
    UNKNOWN_BUS_TYPE,
    BUS_PROTOCOL_NOT_COMPILED,
    CONTROLLER_TYPE_NOT_COMPILED,
    BOARD_TYPE_NOT_COMPILED
};

struct BuildProfileValidationResult {
    BuildProfileValidationError error;
    uint16_t index;
    BusId busId;
    ControllerId controllerId;
    BoardId boardId;

    constexpr BuildProfileValidationResult(
        BuildProfileValidationError value = BuildProfileValidationError::NONE,
        uint16_t itemIndex = 0U,
        BusId bus = BusId(),
        ControllerId controller = ControllerId(),
        BoardId board = BoardId()
    ) : error(value), index(itemIndex), busId(bus),
        controllerId(controller), boardId(board) {}

    constexpr bool ok() const { return error == BuildProfileValidationError::NONE; }
};

}} // namespace AquaLook::Domain
