#pragma once

#include <Arduino.h>
#include "EquipmentRuntimeConfig.h"

namespace AquaLook { namespace Runtime {

class EquipmentRuntimeConfigStore {
public:
    EquipmentRuntimeConfigStore();

    bool begin();
    bool load();
    bool save(const EquipmentRuntimeConfig& config);
    bool reset();

    const EquipmentRuntimeConfig& config() const;
    bool isLoaded() const;
    bool usedSafeDefaults() const;
    const char* lastStatus() const;

private:
    EquipmentRuntimeConfig _config;
    bool _loaded;
    bool _safeDefaults;
    const char* _lastStatus;

    void applySafeDefaults(const char* status);
};

}} // namespace AquaLook::Runtime
