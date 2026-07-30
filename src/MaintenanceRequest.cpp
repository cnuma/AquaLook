#include "MaintenanceRequest.h"

#include <Preferences.h>

namespace {
constexpr char NVS_NAMESPACE[] = "aq_maint";
constexpr char NVS_REQUEST_KEY[] = "request";
}

bool MaintenanceRequestStore::isValid(uint8_t rawValue) {
    return rawValue <= static_cast<uint8_t>(MaintenanceRequest::STAGE_UPDATE_TEST);
}

MaintenanceRequest MaintenanceRequestStore::load() {
    Preferences preferences;

    // Ouvrir en lecture/ecriture afin de creer silencieusement le namespace
    // lors du tout premier boot. Une ouverture read-only sur un namespace
    // encore absent produit un log NVS NOT_FOUND bien que l'absence signifie
    // simplement qu'aucune demande de maintenance n'a encore ete enregistree.
    if (!preferences.begin(NVS_NAMESPACE, false)) {
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
        case MaintenanceRequest::DOWNLOAD_UPDATE_TEST: return "download_update_test";
        case MaintenanceRequest::STAGE_UPDATE_TEST: return "stage_update_test";
    }
    return "invalid";
}
