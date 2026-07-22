#include "MaintenanceRequest.h"

#include <Preferences.h>

namespace {
constexpr char NVS_NAMESPACE[] = "aq_maint";
constexpr char NVS_REQUEST_KEY[] = "request";
}

bool MaintenanceRequestStore::isValid(uint8_t rawValue) {
    return rawValue <= static_cast<uint8_t>(MaintenanceRequest::FACTORY_RESET);
}

MaintenanceRequest MaintenanceRequestStore::load() {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, true)) {
        return MaintenanceRequest::NONE;
    }

    const uint8_t rawValue = preferences.getUChar(
        NVS_REQUEST_KEY,
        static_cast<uint8_t>(MaintenanceRequest::NONE)
    );
    preferences.end();

    if (!isValid(rawValue)) {
        return MaintenanceRequest::NONE;
    }

    return static_cast<MaintenanceRequest>(rawValue);
}

bool MaintenanceRequestStore::save(MaintenanceRequest request) {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        return false;
    }

    const size_t written = preferences.putUChar(
        NVS_REQUEST_KEY,
        static_cast<uint8_t>(request)
    );
    preferences.end();
    return written == sizeof(uint8_t);
}

bool MaintenanceRequestStore::clear() {
    return save(MaintenanceRequest::NONE);
}

const char* MaintenanceRequestStore::name(MaintenanceRequest request) {
    switch (request) {
        case MaintenanceRequest::NONE: return "none";
        case MaintenanceRequest::PROBE_GITHUB: return "probe_github";
        case MaintenanceRequest::CHECK_VERSION: return "check_version";
        case MaintenanceRequest::INSTALL_UPDATE: return "install_update";
        case MaintenanceRequest::RECOVERY: return "recovery";
        case MaintenanceRequest::FACTORY_RESET: return "factory_reset";
    }
    return "invalid";
}
