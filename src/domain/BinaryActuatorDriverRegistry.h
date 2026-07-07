#pragma once

#include <stddef.h>

#include "domain/BinaryActuatorDriver.h"

namespace AquaLook { namespace Domain {

enum class DriverRegistryError : uint8_t {
    NONE = 0,
    INVALID_ARGUMENT,
    DUPLICATE_CONTROLLER_TYPE,
    CAPACITY_EXCEEDED,
    DRIVER_NOT_FOUND
};

struct DriverRegistryResult {
    DriverRegistryError error;
    uint16_t index;
    ControllerTypeId controllerTypeId;

    constexpr DriverRegistryResult(
        DriverRegistryError value = DriverRegistryError::NONE,
        uint16_t itemIndex = 0U,
        ControllerTypeId typeId = ControllerTypeId()
    ) : error(value), index(itemIndex), controllerTypeId(typeId) {}

    constexpr bool ok() const { return error == DriverRegistryError::NONE; }
};

class BinaryActuatorDriverRegistry {
public:
    constexpr BinaryActuatorDriverRegistry(
        BinaryActuatorDriverBinding* storage,
        size_t capacity
    ) : storage_(storage), size_(0U), capacity_(storage ? capacity : 0U) {}

    constexpr size_t size() const { return size_; }
    constexpr size_t capacity() const { return capacity_; }
    constexpr bool empty() const { return size_ == 0U; }
    constexpr bool full() const { return size_ >= capacity_; }

    DriverRegistryResult registerDriver(
        const BinaryActuatorDriverBinding& binding
    ) {
        if (!storage_ || !binding.controllerTypeId.isValid() ||
            !binding.operations ||
            !hasCompleteBinaryActuatorOps(*binding.operations)) {
            return DriverRegistryResult(
                DriverRegistryError::INVALID_ARGUMENT,
                static_cast<uint16_t>(size_), binding.controllerTypeId
            );
        }

        for (size_t i = 0U; i < size_; ++i) {
            if (storage_[i].controllerTypeId == binding.controllerTypeId) {
                return DriverRegistryResult(
                    DriverRegistryError::DUPLICATE_CONTROLLER_TYPE,
                    static_cast<uint16_t>(i), binding.controllerTypeId
                );
            }
        }

        if (full()) {
            return DriverRegistryResult(
                DriverRegistryError::CAPACITY_EXCEEDED,
                static_cast<uint16_t>(size_), binding.controllerTypeId
            );
        }

        storage_[size_] = binding;
        const uint16_t index = static_cast<uint16_t>(size_);
        ++size_;
        return DriverRegistryResult(
            DriverRegistryError::NONE, index, binding.controllerTypeId
        );
    }

    const BinaryActuatorDriverBinding* find(
        ControllerTypeId controllerTypeId
    ) const {
        for (size_t i = 0U; i < size_; ++i) {
            if (storage_[i].controllerTypeId == controllerTypeId) {
                return &storage_[i];
            }
        }
        return nullptr;
    }

    BinaryActuatorDriverBinding* find(ControllerTypeId controllerTypeId) {
        for (size_t i = 0U; i < size_; ++i) {
            if (storage_[i].controllerTypeId == controllerTypeId) {
                return &storage_[i];
            }
        }
        return nullptr;
    }

    void clear() {
        if (storage_) {
            for (size_t i = 0U; i < size_; ++i) {
                storage_[i] = BinaryActuatorDriverBinding();
            }
        }
        size_ = 0U;
    }

private:
    BinaryActuatorDriverBinding* storage_;
    size_t size_;
    size_t capacity_;
};

}} // namespace AquaLook::Domain
