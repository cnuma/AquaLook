#pragma once

#include <stdint.h>

#include "RelayPhysicalBackend.h"

namespace AquaLook { namespace Runtime {

class V4RelayPhysicalBackend : public RelayPhysicalBackend {
public:
    V4RelayPhysicalBackend() = default;

    bool setZoneValve(
        uint8_t zoneIndex,
        bool active,
        uint32_t nowMs = 0U
    ) override;

    bool getZoneValveState(
        uint8_t zoneIndex,
        bool& active
    ) const override;

    bool isZoneMigrated(uint8_t zoneIndex) const;
    bool hasAnyMigratedZone() const;

private:
    uint32_t _migratedZoneMask = 0U;
};

}} // namespace AquaLook::Runtime
