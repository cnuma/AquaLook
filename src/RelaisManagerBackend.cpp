#include "RelaisManagerBackend.h"

#include "RelaisManager.h"
#include "config.h"

namespace AquaLook { namespace Runtime {

RelaisManagerBackend::RelaisManagerBackend(RelaisManager* relais)
    : _relais(relais) {}

void RelaisManagerBackend::bind(RelaisManager* relais) {
    _relais = relais;
}

bool RelaisManagerBackend::isBound() const {
    return _relais != nullptr;
}

bool RelaisManagerBackend::setZoneValve(
    uint8_t zoneIndex,
    bool active,
    uint32_t nowMs
) {
    (void)nowMs;

    if (!_relais || zoneIndex >= MAX_ZONES) {
        return false;
    }

    _relais->setRelay(zoneIndex, active);
    return true;
}

bool RelaisManagerBackend::getZoneValveState(
    uint8_t zoneIndex,
    bool& active
) const {
    if (!_relais || zoneIndex >= MAX_ZONES) {
        active = false;
        return false;
    }

    active = _relais->getState(zoneIndex);
    return true;
}

}} // namespace AquaLook::Runtime
