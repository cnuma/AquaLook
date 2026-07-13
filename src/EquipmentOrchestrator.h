#pragma once

#include <Arduino.h>
#include "EquipmentManager.h"

namespace AquaLook { namespace Application {

// Couche applicative placée entre les intentions métier et EquipmentManager.
// RUN7.1 reste strictement passif : l'orchestrateur construit et expose un
// aperçu du plan, sans exécuter d'action matérielle.
class EquipmentOrchestrator {
public:
    enum Intent : uint8_t {
        INTENT_NONE = 0,
        INTENT_START_ZONE,
        INTENT_STOP_ZONE
    };

    enum PreviewStatus : uint8_t {
        PREVIEW_READY = 0,
        PREVIEW_NOT_INITIALIZED,
        PREVIEW_INVALID_ZONE,
        PREVIEW_PLAN_REJECTED
    };

    struct Preview {
        PreviewStatus status;
        Intent intent;
        uint8_t zone;
        bool requiresPump;
        uint8_t stepCount;
        EquipmentManager::ActionResult planResult;

        constexpr Preview()
            : status(PREVIEW_NOT_INITIALIZED),
              intent(INTENT_NONE),
              zone(0U),
              requiresPump(false),
              stepCount(0U),
              planResult(EquipmentManager::ACTION_NOT_INITIALIZED) {}

        constexpr bool ready() const {
            return status == PREVIEW_READY;
        }
    };

    void begin(const EquipmentManager* equipmentManager, uint8_t nbZones);
    bool isInitialized() const;
    uint8_t zoneCount() const;

    Preview previewStartZone(uint8_t zone) const;
    Preview previewStopZone(uint8_t zone) const;

private:
    const EquipmentManager* _equipmentManager = nullptr;
    uint8_t _nbZones = 0U;

    Preview previewZone(Intent intent, uint8_t zone) const;
};

}} // namespace AquaLook::Application
