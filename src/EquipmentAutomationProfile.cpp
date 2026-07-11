#include "EquipmentAutomationProfile.h"

#include <cstring>

namespace AquaLook { namespace Runtime {

static constexpr uint16_t EQUIPMENT_AUTOMATION_PROFILE_SCHEMA = 1U;
static constexpr uint16_t MAX_TRANSITION_DELAY_MS = 30000U;
static constexpr uint16_t MAX_MIN_STATE_SEC = 3600U;

ManagedEquipmentConfig::ManagedEquipmentConfig()
    : enabled(false),
      mode(EquipmentExecutionMode::MODE_DISABLED),
      type(EquipmentModel::EQUIP_UNUSED),
      targetIndex(0U),
      relayAssignmentIndex(EquipmentModel::INVALID_INDEX),
      startupDelayMs(0U),
      shutdownDelayMs(0U),
      minOnSec(0U),
      minOffSec(0U),
      name{} {}

ZoneEquipmentDependencies::ZoneEquipmentDependencies()
    : enabled(false),
      zoneIndex(0U),
      valveEquipmentIndex(EquipmentModel::INVALID_INDEX),
      supportEquipmentIndex(EquipmentModel::INVALID_INDEX) {}

EquipmentAutomationProfile::EquipmentAutomationProfile()
    : schemaVersion(EQUIPMENT_AUTOMATION_PROFILE_SCHEMA),
      equipments{},
      zoneDependencies{} {}

const char* equipmentExecutionModeName(EquipmentExecutionMode mode) {
    switch (mode) {
        case EquipmentExecutionMode::MODE_DISABLED: return "disabled";
        case EquipmentExecutionMode::MODE_SHADOW: return "shadow";
        case EquipmentExecutionMode::MODE_PHYSICAL: return "physical";
        default: return "invalid";
    }
}

void clearEquipmentAutomationProfile(EquipmentAutomationProfile& profile) {
    profile = EquipmentAutomationProfile();
}

bool validateManagedEquipment(
    const EquipmentAutomationProfile& profile,
    uint8_t equipmentIndex
) {
    if (equipmentIndex >= EquipmentModel::MAX_EQUIPMENTS) return false;

    const ManagedEquipmentConfig& equipment = profile.equipments[equipmentIndex];
    if (!equipment.enabled) return false;
    if (!EquipmentModel::isSupportedType(equipment.type) ||
        equipment.type == EquipmentModel::EQUIP_UNUSED) {
        return false;
    }

    const uint8_t modeValue = static_cast<uint8_t>(equipment.mode);
    if (modeValue > static_cast<uint8_t>(EquipmentExecutionMode::MODE_PHYSICAL)) {
        return false;
    }

    if (equipment.startupDelayMs > MAX_TRANSITION_DELAY_MS ||
        equipment.shutdownDelayMs > MAX_TRANSITION_DELAY_MS ||
        equipment.minOnSec > MAX_MIN_STATE_SEC ||
        equipment.minOffSec > MAX_MIN_STATE_SEC) {
        return false;
    }

    if (EquipmentModel::typeUsesRelay(equipment.type) &&
        equipment.relayAssignmentIndex == EquipmentModel::INVALID_INDEX) {
        return false;
    }

    return true;
}

bool validateZoneDependencies(
    const EquipmentAutomationProfile& profile,
    uint8_t dependencyIndex,
    uint8_t nbZones
) {
    if (dependencyIndex >= MAX_ZONES || dependencyIndex >= nbZones) return false;

    const ZoneEquipmentDependencies& dependency =
        profile.zoneDependencies[dependencyIndex];
    if (!dependency.enabled || dependency.zoneIndex >= nbZones) return false;

    if (!validateManagedEquipment(profile, dependency.valveEquipmentIndex)) {
        return false;
    }

    const ManagedEquipmentConfig& valve =
        profile.equipments[dependency.valveEquipmentIndex];
    if (valve.type != EquipmentModel::EQUIP_ZONE_VALVE ||
        valve.targetIndex != dependency.zoneIndex) {
        return false;
    }

    if (dependency.supportEquipmentIndex == EquipmentModel::INVALID_INDEX) {
        return true;
    }

    return validateManagedEquipment(profile, dependency.supportEquipmentIndex);
}

int16_t findManagedEquipment(
    const EquipmentAutomationProfile& profile,
    uint8_t type,
    uint8_t targetIndex
) {
    for (uint8_t index = 0U; index < EquipmentModel::MAX_EQUIPMENTS; ++index) {
        const ManagedEquipmentConfig& equipment = profile.equipments[index];
        if (equipment.enabled && equipment.type == type &&
            equipment.targetIndex == targetIndex) {
            return static_cast<int16_t>(index);
        }
    }
    return -1;
}

bool buildEquipmentModelFromProfile(
    const EquipmentAutomationProfile& profile,
    uint8_t nbZones,
    EquipmentModel::EquipmentConfigSet& model
) {
    if (profile.schemaVersion != EQUIPMENT_AUTOMATION_PROFILE_SCHEMA ||
        nbZones == 0U || nbZones > MAX_ZONES) {
        return false;
    }

    EquipmentModel::clear(model);

    for (uint8_t index = 0U; index < EquipmentModel::MAX_EQUIPMENTS; ++index) {
        const ManagedEquipmentConfig& source = profile.equipments[index];
        if (!source.enabled) continue;
        if (!validateManagedEquipment(profile, index)) return false;

        EquipmentModel::EquipmentConfig& target = model.equipments[index];
        target.enabled = source.mode != EquipmentExecutionMode::MODE_DISABLED;
        target.type = source.type;
        target.targetIndex = source.targetIndex;
        target.relayAssignmentIndex = source.relayAssignmentIndex;
        target.startupDelayMs = source.startupDelayMs;
        target.shutdownDelayMs = source.shutdownDelayMs;
        target.minOnSec = source.minOnSec;
        target.minOffSec = source.minOffSec;
        strlcpy(target.name, source.name, sizeof(target.name));
    }

    for (uint8_t zone = 0U; zone < nbZones; ++zone) {
        if (!validateZoneDependencies(profile, zone, nbZones)) return false;

        const ZoneEquipmentDependencies& source = profile.zoneDependencies[zone];
        EquipmentModel::ZoneEquipmentLink& target = model.zoneLinks[zone];
        target.enabled = true;
        target.zoneIndex = source.zoneIndex;
        target.valveEquipmentIndex = source.valveEquipmentIndex;
        target.pumpEquipmentIndex = source.supportEquipmentIndex;
    }

    return true;
}

}} // namespace AquaLook::Runtime
