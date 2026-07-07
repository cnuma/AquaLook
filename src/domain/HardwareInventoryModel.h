#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/DomainIdentifiers.h"

namespace AquaLook { namespace Domain {

enum class BusType : uint8_t {
    UNKNOWN = 0,
    GPIO = 1,
    I2C = 2,
    SPI = 3,
    UART = 4,
    ONEWIRE = 5,
    CAN = 6,
    RS485 = 7,
    REMOTE = 8,
    VIRTUAL = 9
};

enum BusFlags : uint8_t {
    BUS_FLAG_NONE = 0U,
    BUS_FLAG_ENABLED = 1U << 0,
    BUS_FLAG_SHARED = 1U << 1,
    BUS_FLAG_EXTERNAL = 1U << 2,
    BUS_FLAG_ADDRESSABLE = 1U << 3
};

struct BusPins {
    int8_t primary;
    int8_t secondary;
    int8_t tertiary;
    int8_t quaternary;

    constexpr BusPins()
        : primary(-1), secondary(-1), tertiary(-1), quaternary(-1) {}
};

struct BusDefinition {
    uint32_t frequencyHz;
    uint16_t timeoutMs;
    BusId id;
    BusType type;
    uint8_t instance;
    uint8_t flags;
    uint8_t reserved;
    BusPins pins;

    constexpr BusDefinition()
        : frequencyHz(0U), timeoutMs(0U), id(), type(BusType::UNKNOWN),
          instance(0U), flags(BUS_FLAG_NONE), reserved(0U), pins() {}
};

using ControllerCapabilityMask = uint32_t;

enum ControllerCapability : ControllerCapabilityMask {
    CONTROLLER_CAP_NONE = 0U,
    CONTROLLER_CAP_DIGITAL_INPUT = 1UL << 0,
    CONTROLLER_CAP_DIGITAL_OUTPUT = 1UL << 1,
    CONTROLLER_CAP_PWM_OUTPUT = 1UL << 2,
    CONTROLLER_CAP_COUNTER_INPUT = 1UL << 3,
    CONTROLLER_CAP_ANALOG_INPUT = 1UL << 4,
    CONTROLLER_CAP_INTERRUPT_INPUT = 1UL << 5,
    CONTROLLER_CAP_RELAY_OUTPUT = 1UL << 6,
    CONTROLLER_CAP_SENSOR_INPUT = 1UL << 7,
    CONTROLLER_CAP_REMOTE_NODE = 1UL << 8
};

enum class ControllerStatus : uint8_t {
    DISABLED = 0,
    CONFIGURED = 1,
    AVAILABLE = 2,
    UNAVAILABLE = 3,
    FAULTED = 4
};

enum ControllerFlags : uint8_t {
    CONTROLLER_FLAG_NONE = 0U,
    CONTROLLER_FLAG_ENABLED = 1U << 0,
    CONTROLLER_FLAG_ADDRESS_REQUIRED = 1U << 1,
    CONTROLLER_FLAG_EXCLUSIVE_ENDPOINT = 1U << 2
};

struct ControllerAddress {
    uint32_t primary;
    uint32_t secondary;

    constexpr ControllerAddress() : primary(0U), secondary(0U) {}
    constexpr ControllerAddress(uint32_t first, uint32_t second = 0U)
        : primary(first), secondary(second) {}

    constexpr bool isEmpty() const { return primary == 0U && secondary == 0U; }
};

struct ControllerDefinition {
    ControllerAddress address;
    ControllerCapabilityMask capabilities;
    ControllerId id;
    ControllerTypeId typeId;
    BusId busId;
    uint16_t channelCount;
    ControllerStatus status;
    uint8_t flags;
    uint16_t reserved;

    constexpr ControllerDefinition()
        : address(), capabilities(CONTROLLER_CAP_NONE), id(), typeId(), busId(),
          channelCount(0U), status(ControllerStatus::DISABLED),
          flags(CONTROLLER_FLAG_NONE), reserved(0U) {}
};

enum class HardwareInventoryValidationError : uint8_t {
    NONE = 0,
    INVALID_BUS_ID,
    UNKNOWN_BUS_TYPE,
    DUPLICATE_BUS_ID,
    DUPLICATE_BUS_INSTANCE,
    INVALID_BUS_FREQUENCY,
    INVALID_CONTROLLER_ID,
    INVALID_CONTROLLER_TYPE,
    ORPHAN_BUS,
    INVALID_CONTROLLER_STATUS,
    INVALID_CHANNEL_COUNT,
    ADDRESS_REQUIRED,
    ADDRESS_NOT_ALLOWED,
    INVALID_ADDRESS,
    DUPLICATE_CONTROLLER_ID,
    ENDPOINT_COLLISION
};

struct HardwareInventoryValidationResult {
    HardwareInventoryValidationError error;
    uint16_t index;
    BusId busId;
    ControllerId controllerId;

    constexpr HardwareInventoryValidationResult(
        HardwareInventoryValidationError value = HardwareInventoryValidationError::NONE,
        uint16_t itemIndex = 0U,
        BusId bus = BusId(),
        ControllerId controller = ControllerId()
    ) : error(value), index(itemIndex), busId(bus), controllerId(controller) {}

    constexpr bool ok() const {
        return error == HardwareInventoryValidationError::NONE;
    }
};

bool isKnownBusType(BusType type);
bool isAddressableBus(BusType type);
bool isValidAddress(BusType type, const ControllerAddress& address);

HardwareInventoryValidationResult validateHardwareInventory(
    const BusDefinition* buses,
    size_t busCount,
    const ControllerDefinition* controllers,
    size_t controllerCount
);

static_assert(sizeof(BusPins) == 4U, "BusPins layout changed");
static_assert(sizeof(BusDefinition) == 16U, "BusDefinition layout changed");
static_assert(sizeof(ControllerAddress) == 8U, "ControllerAddress layout changed");
static_assert(sizeof(ControllerDefinition) == 24U, "ControllerDefinition layout changed");

}} // namespace AquaLook::Domain
