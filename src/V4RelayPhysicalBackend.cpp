#include "V4RelayPhysicalBackend.h"

#include "config.h"

namespace AquaLook { namespace Runtime {

bool V4RelayPhysicalBackend::setZoneValve(
    uint8_t zoneIndex,
    bool active,
    uint32_t nowMs
) {
    (void)active;
    (void)nowMs;

    if (zoneIndex >= MAX_ZONES) {
        return false;
    }

    if (!isZoneMigrated(zoneIndex)) {
        return false;
    }

    return false;
}

bool V4RelayPhysicalBackend::getZoneValveState(
    uint8_t zoneIndex,
    bool& active
) const {
    active = false;

    if (zoneIndex >= MAX_ZONES) {
        return false;
    }

    if (!isZoneMigrated(zoneIndex)) {
        return false;
    }

    return false;
}

bool V4RelayPhysicalBackend::isZoneMigrated(uint8_t zoneIndex) const {
    if (zoneIndex >= MAX_ZONES || zoneIndex >= 32U) {
        return false;
    }

    const uint32_t zoneBit = static_cast<uint32_t>(1UL << zoneIndex);
    return (_migratedZoneMask & zoneBit) != 0U;
}

bool V4RelayPhysicalBackend::hasAnyMigratedZone() const {
    return _migratedZoneMask != 0U;
}

}} // namespace AquaLook::Runtime
