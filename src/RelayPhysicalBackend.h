#pragma once

#include <stdint.h>

namespace AquaLook { namespace Runtime {

class RelayPhysicalBackend {
public:
    virtual ~RelayPhysicalBackend() = default;

    virtual bool setZoneValve(
        uint8_t zoneIndex,
        bool active,
        uint32_t nowMs = 0U
    ) = 0;

    virtual bool getZoneValveState(
        uint8_t zoneIndex,
        bool& active
    ) const = 0;
};

}} // namespace AquaLook::Runtime
