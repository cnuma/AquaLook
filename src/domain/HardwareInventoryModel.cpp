#include "domain/HardwareInventoryModel.h"

namespace AquaLook { namespace Domain {

static int findBusIndex(BusId id, const BusDefinition* buses, size_t busCount) {
    if (!buses) return -1;
    for (size_t i = 0U; i < busCount; ++i) {
        if (buses[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

static bool sameEndpoint(
    const ControllerDefinition& lhs,
    const ControllerDefinition& rhs
) {
    return lhs.busId == rhs.busId &&
           lhs.address.primary == rhs.address.primary &&
           lhs.address.secondary == rhs.address.secondary;
}

bool isKnownBusType(BusType type) {
    return type == BusType::GPIO ||
           type == BusType::I2C ||
           type == BusType::SPI ||
           type == BusType::UART ||
           type == BusType::ONEWIRE ||
           type == BusType::CAN ||
           type == BusType::RS485 ||
           type == BusType::REMOTE ||
           type == BusType::VIRTUAL;
}

bool isAddressableBus(BusType type) {
    return type == BusType::I2C ||
           type == BusType::SPI ||
           type == BusType::UART ||
           type == BusType::ONEWIRE ||
           type == BusType::CAN ||
           type == BusType::RS485 ||
           type == BusType::REMOTE;
}

bool isValidAddress(BusType type, const ControllerAddress& address) {
    switch (type) {
        case BusType::I2C:
            return address.secondary == 0U &&
                   address.primary >= 0x08U && address.primary <= 0x77U;
        case BusType::SPI:
            return address.secondary == 0U && address.primary <= 255U;
        case BusType::UART:
        case BusType::RS485:
            return address.secondary == 0U && address.primary <= 247U;
        case BusType::ONEWIRE:
            return !address.isEmpty();
        case BusType::CAN:
            return address.secondary == 0U && address.primary <= 0x1FFFFFFFU;
        case BusType::REMOTE:
            return !address.isEmpty();
        case BusType::GPIO:
        case BusType::VIRTUAL:
            return address.isEmpty();
        default:
            return false;
    }
}

HardwareInventoryValidationResult validateHardwareInventory(
    const BusDefinition* buses,
    size_t busCount,
    const ControllerDefinition* controllers,
    size_t controllerCount
) {
    if ((busCount != 0U && !buses) || (controllerCount != 0U && !controllers)) {
        return HardwareInventoryValidationError::ORPHAN_BUS;
    }

    for (size_t i = 0U; i < busCount; ++i) {
        const BusDefinition& bus = buses[i];
        if (!bus.id.isValid()) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::INVALID_BUS_ID,
                static_cast<uint16_t>(i), bus.id
            );
        }
        if (!isKnownBusType(bus.type)) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::UNKNOWN_BUS_TYPE,
                static_cast<uint16_t>(i), bus.id
            );
        }
        if (bus.type != BusType::GPIO && bus.type != BusType::VIRTUAL &&
            bus.frequencyHz == 0U) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::INVALID_BUS_FREQUENCY,
                static_cast<uint16_t>(i), bus.id
            );
        }

        for (size_t j = 0U; j < i; ++j) {
            if (buses[j].id == bus.id) {
                return HardwareInventoryValidationResult(
                    HardwareInventoryValidationError::DUPLICATE_BUS_ID,
                    static_cast<uint16_t>(i), bus.id
                );
            }
            if (buses[j].type == bus.type && buses[j].instance == bus.instance) {
                return HardwareInventoryValidationResult(
                    HardwareInventoryValidationError::DUPLICATE_BUS_INSTANCE,
                    static_cast<uint16_t>(i), bus.id
                );
            }
        }
    }

    for (size_t i = 0U; i < controllerCount; ++i) {
        const ControllerDefinition& controller = controllers[i];
        if (!controller.id.isValid()) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::INVALID_CONTROLLER_ID,
                static_cast<uint16_t>(i), BusId(), controller.id
            );
        }
        if (!controller.typeId.isValid()) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::INVALID_CONTROLLER_TYPE,
                static_cast<uint16_t>(i), controller.busId, controller.id
            );
        }
        if (controller.channelCount == 0U) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::INVALID_CHANNEL_COUNT,
                static_cast<uint16_t>(i), controller.busId, controller.id
            );
        }
        if (controller.status != ControllerStatus::DISABLED &&
            controller.status != ControllerStatus::CONFIGURED &&
            controller.status != ControllerStatus::AVAILABLE &&
            controller.status != ControllerStatus::UNAVAILABLE &&
            controller.status != ControllerStatus::FAULTED) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::INVALID_CONTROLLER_STATUS,
                static_cast<uint16_t>(i), controller.busId, controller.id
            );
        }

        const int busIndex = findBusIndex(controller.busId, buses, busCount);
        if (busIndex < 0) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::ORPHAN_BUS,
                static_cast<uint16_t>(i), controller.busId, controller.id
            );
        }

        const BusDefinition& bus = buses[busIndex];
        const bool addressRequired =
            isAddressableBus(bus.type) ||
            (controller.flags & CONTROLLER_FLAG_ADDRESS_REQUIRED) != 0U;

        if (addressRequired && controller.address.isEmpty()) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::ADDRESS_REQUIRED,
                static_cast<uint16_t>(i), controller.busId, controller.id
            );
        }
        if (!isAddressableBus(bus.type) && !controller.address.isEmpty()) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::ADDRESS_NOT_ALLOWED,
                static_cast<uint16_t>(i), controller.busId, controller.id
            );
        }
        if (!isValidAddress(bus.type, controller.address)) {
            return HardwareInventoryValidationResult(
                HardwareInventoryValidationError::INVALID_ADDRESS,
                static_cast<uint16_t>(i), controller.busId, controller.id
            );
        }

        for (size_t j = 0U; j < i; ++j) {
            if (controllers[j].id == controller.id) {
                return HardwareInventoryValidationResult(
                    HardwareInventoryValidationError::DUPLICATE_CONTROLLER_ID,
                    static_cast<uint16_t>(i), controller.busId, controller.id
                );
            }
            if (sameEndpoint(controllers[j], controller) &&
                !controller.address.isEmpty() &&
                (((controllers[j].flags | controller.flags) &
                  CONTROLLER_FLAG_EXCLUSIVE_ENDPOINT) != 0U ||
                 isAddressableBus(bus.type))) {
                return HardwareInventoryValidationResult(
                    HardwareInventoryValidationError::ENDPOINT_COLLISION,
                    static_cast<uint16_t>(i), controller.busId, controller.id
                );
            }
        }
    }

    return HardwareInventoryValidationError::NONE;
}

}} // namespace AquaLook::Domain
