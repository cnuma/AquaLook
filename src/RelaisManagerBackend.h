#pragma once
#include "config.h"
#if AQUALOOK_RELAY_BACKEND_LEGACY
// Backend qui delegue au moteur historique. Le firmware V4 ne le
// contient pas : il n a plus de second moteur derriere lui.

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

#endif
