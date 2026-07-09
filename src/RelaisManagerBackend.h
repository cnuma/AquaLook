#pragma once

#include "RelayPhysicalBackend.h"

class RelaisManager;

namespace AquaLook { namespace Runtime {

class RelaisManagerBackend : public RelayPhysicalBackend {
public:
    RelaisManagerBackend() = default;
    explicit RelaisManagerBackend(RelaisManager* relais);

    void bind(RelaisManager* relais);
    bool isBound() const;

    bool setZoneValve(
        uint8_t zoneIndex,
        bool active,
        uint32_t nowMs = 0U
    ) override;

    bool getZoneValveState(
        uint8_t zoneIndex,
        bool& active
    ) const override;

private:
    RelaisManager* _relais = nullptr;
};

}} // namespace AquaLook::Runtime
