#include "EquipmentConfigRepository.h"

namespace AquaLook { namespace Runtime {

EquipmentConfigRepository::EquipmentConfigRepository()
    : _profile(),
      _primary(nullptr),
      _fallback(nullptr),
      _source(EquipmentConfigSource::SAFE_DEFAULTS),
      _status("safe_defaults") {}

void EquipmentConfigRepository::bindPrimary(IEquipmentConfigStorage* storage) {
    _primary = storage;
}

void EquipmentConfigRepository::bindFallback(IEquipmentConfigStorage* storage) {
    _fallback = storage;
}

bool EquipmentConfigRepository::load() {
    EquipmentAutomationProfile candidate;

    if (_primary && _primary->isAvailable() && _primary->load(candidate)) {
        _profile = candidate;
        _source = EquipmentConfigSource::SD_CARD;
        _status = "loaded_primary";
        return true;
    }

    if (_fallback && _fallback->isAvailable() && _fallback->load(candidate)) {
        _profile = candidate;
        _source = EquipmentConfigSource::NVS_FALLBACK;
        _status = "loaded_fallback";
        return true;
    }

    resetToSafeDefaults();
    return false;
}

bool EquipmentConfigRepository::save() {
    if (_primary && _primary->isAvailable() && _primary->save(_profile)) {
        _source = EquipmentConfigSource::SD_CARD;
        _status = "saved_primary";

        if (_fallback && _fallback->isAvailable()) {
            _fallback->save(_profile);
        }
        return true;
    }

    if (_fallback && _fallback->isAvailable() && _fallback->save(_profile)) {
        _source = EquipmentConfigSource::NVS_FALLBACK;
        _status = "saved_fallback";
        return true;
    }

    _status = "save_failed";
    return false;
}

void EquipmentConfigRepository::resetToSafeDefaults() {
    clearEquipmentAutomationProfile(_profile);
    _source = EquipmentConfigSource::SAFE_DEFAULTS;
    _status = "safe_defaults";
}

EquipmentAutomationProfile& EquipmentConfigRepository::profile() {
    return _profile;
}

const EquipmentAutomationProfile& EquipmentConfigRepository::profile() const {
    return _profile;
}

EquipmentConfigSource EquipmentConfigRepository::source() const {
    return _source;
}

const char* EquipmentConfigRepository::status() const {
    return _status;
}

}} // namespace AquaLook::Runtime
