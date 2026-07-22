#include "FaultManager.h"

// Stubs strictement réservés au profil PlatformIO test_ota_https.
// Ils évitent d'embarquer TFT_eSPI et le gestionnaire visuel des défauts
// dans le banc TLS isolé. Ce fichier est exclu de tous les profils nominaux.

uint32_t FaultManager::_activeMask = 0U;
bool FaultManager::_unacknowledged = false;
bool FaultManager::_started = false;

void FaultManager::begin() {
    _started = true;
    _activeMask = 0U;
    _unacknowledged = false;
}

void FaultManager::update() {}

void FaultManager::setActive(FaultId id, bool active) {
    const uint32_t mask = 1UL << static_cast<uint8_t>(id);
    if (active) {
        _activeMask |= mask;
    } else {
        _activeMask &= ~mask;
    }
}

void FaultManager::notifyError() {
    _unacknowledged = true;
}

void FaultManager::acknowledge() {
    _unacknowledged = false;
}

bool FaultManager::hasActiveFaults() {
    return _activeMask != 0U;
}

bool FaultManager::hasUnacknowledgedErrors() {
    return _unacknowledged;
}

bool FaultManager::isAcknowledged() {
    return !_unacknowledged;
}

uint32_t FaultManager::activeMask() {
    return _activeMask;
}

void FaultManager::resolveColor(uint8_t normalRed,
                                uint8_t normalGreen,
                                uint8_t normalBlue,
                                uint8_t& outRed,
                                uint8_t& outGreen,
                                uint8_t& outBlue) {
    outRed = normalRed;
    outGreen = normalGreen;
    outBlue = normalBlue;
}
