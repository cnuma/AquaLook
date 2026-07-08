#include "domain/BinaryActuatorDriverBootstrap.h"

#include "domain/HardwareCatalog.h"

namespace AquaLook { namespace Domain {

namespace {

uint8_t countRequestedDrivers(uint8_t enabledDrivers) {
    uint8_t count = 0U;
    if ((enabledDrivers & BOOTSTRAP_DRIVER_SIMULATED) != 0U) ++count;
    if ((enabledDrivers & BOOTSTRAP_DRIVER_GPIO) != 0U) ++count;
    if ((enabledDrivers & BOOTSTRAP_DRIVER_XL9535) != 0U) ++count;
    return count;
}

BinaryActuatorDriverBootstrapResult registerBinding(
    BinaryActuatorDriverRegistry& registry,
    const BinaryActuatorDriverBinding& binding,
    uint8_t requested,
    uint8_t registered
) {
    const DriverRegistryResult result = registry.registerDriver(binding);
    if (!result.ok()) {
        return BinaryActuatorDriverBootstrapResult(
            result.error,
            requested,
            registered,
            result.controllerTypeId
        );
    }
    return BinaryActuatorDriverBootstrapResult(
        DriverRegistryError::NONE,
        requested,
        static_cast<uint8_t>(registered + 1U),
        ControllerTypeId()
    );
}

} // namespace

BinaryActuatorDriverBootstrapResult bootstrapBinaryActuatorDrivers(
    BinaryActuatorDriverRegistry& registry,
    const BinaryActuatorDriverBootstrapPlan& plan
) {
    const uint8_t compiledMask = compiledBinaryActuatorDriverMask();
    const uint8_t requestedMask = static_cast<uint8_t>(
        plan.enabledDrivers & compiledMask
    );
    const uint8_t requested = countRequestedDrivers(requestedMask);
    uint8_t registered = 0U;

    if ((requestedMask & BOOTSTRAP_DRIVER_SIMULATED) != 0U) {
        if (!plan.simulated) {
            return BinaryActuatorDriverBootstrapResult(
                DriverRegistryError::INVALID_ARGUMENT,
                requested,
                registered,
                ControllerTypeId()
            );
        }
        BinaryActuatorDriverBinding binding;
        binding.controllerTypeId = ControllerTypeIds::REMOTE_GENERIC;
        binding.operations = &simulatedBinaryActuatorDriverOps();
        binding.context = plan.simulated;

        const BinaryActuatorDriverBootstrapResult result = registerBinding(
            registry, binding, requested, registered
        );
        if (!result.ok()) return result;
        registered = result.registeredDrivers;
    }

#if AQUALOOK_V4_ENABLE_GPIO
    if ((requestedMask & BOOTSTRAP_DRIVER_GPIO) != 0U) {
        if (!plan.gpio) {
            return BinaryActuatorDriverBootstrapResult(
                DriverRegistryError::INVALID_ARGUMENT,
                requested,
                registered,
                ControllerTypeIds::LOCAL_GPIO
            );
        }
        const BinaryActuatorDriverBootstrapResult result = registerBinding(
            registry,
            makeGpioBinaryActuatorDriverBinding(*plan.gpio),
            requested,
            registered
        );
        if (!result.ok()) return result;
        registered = result.registeredDrivers;
    }
#endif

#if AQUALOOK_V4_ENABLE_I2C
    if ((requestedMask & BOOTSTRAP_DRIVER_XL9535) != 0U) {
        if (!plan.xl9535) {
            return BinaryActuatorDriverBootstrapResult(
                DriverRegistryError::INVALID_ARGUMENT,
                requested,
                registered,
                ControllerTypeIds::XL9535
            );
        }
        const BinaryActuatorDriverBootstrapResult result = registerBinding(
            registry,
            makeXl9535BinaryActuatorDriverBinding(*plan.xl9535),
            requested,
            registered
        );
        if (!result.ok()) return result;
        registered = result.registeredDrivers;
    }
#endif

    return BinaryActuatorDriverBootstrapResult(
        DriverRegistryError::NONE,
        requested,
        registered,
        ControllerTypeId()
    );
}

}} // namespace AquaLook::Domain
