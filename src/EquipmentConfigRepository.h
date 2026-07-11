#pragma once

#include "EquipmentAutomationProfile.h"

namespace AquaLook { namespace Runtime {

enum class EquipmentConfigSource : uint8_t {
    NONE = 0,
    SD_CARD = 1,
    NVS_FALLBACK = 2,
    SAFE_DEFAULTS = 3
};

class IEquipmentConfigStorage {
public:
    virtual ~IEquipmentConfigStorage() = default;

    virtual bool isAvailable() const = 0;
    virtual bool load(EquipmentAutomationProfile& profile) = 0;
    virtual bool save(const EquipmentAutomationProfile& profile) = 0;
    virtual const char* name() const = 0;
};

class EquipmentConfigRepository {
public:
    EquipmentConfigRepository();

    void bindPrimary(IEquipmentConfigStorage* storage);
    void bindFallback(IEquipmentConfigStorage* storage);

    bool load();
    bool save();
    void resetToSafeDefaults();

    EquipmentAutomationProfile& profile();
    const EquipmentAutomationProfile& profile() const;
    EquipmentConfigSource source() const;
    const char* status() const;

private:
    EquipmentAutomationProfile _profile;
    IEquipmentConfigStorage* _primary;
    IEquipmentConfigStorage* _fallback;
    EquipmentConfigSource _source;
    const char* _status;
};

}} // namespace AquaLook::Runtime
