#include "MaintenanceResult.h"

#include <Preferences.h>
#include <cstring>

namespace {
constexpr char NVS_NAMESPACE[] = "aq_maint_res";

void copyText(char* destination, size_t destinationSize, const String& source) {
    if (destinationSize == 0U) return;
    std::strncpy(destination, source.c_str(), destinationSize - 1U);
    destination[destinationSize - 1U] = '\0';
}
}

MaintenanceResult MaintenanceResultStore::load() {
    MaintenanceResult result;
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, true)) {
        return result;
    }

    result.valid = preferences.getBool("valid", false);
    if (result.valid) {
        result.success = preferences.getBool("success", false);
        result.updateAvailable = preferences.getBool("upd_avail", false);
        result.notificationPending = preferences.getBool("notify", false);
        result.tlsDurationMs = preferences.getULong("tls_ms", 0U);
        result.recordedUptimeMs = preferences.getULong("uptime_ms", 0U);
        result.minFreeHeap = preferences.getULong("heap_min", 0U);
        result.manifestSize = preferences.getULong("manifest_sz", 0U);
        result.firmwareSize = preferences.getULong("firmware_sz", 0U);
        copyText(result.command, sizeof(result.command), preferences.getString("command", ""));
        copyText(result.httpLine, sizeof(result.httpLine), preferences.getString("http", ""));
        copyText(result.detail, sizeof(result.detail), preferences.getString("detail", ""));
        copyText(result.installedVersion, sizeof(result.installedVersion), preferences.getString("installed", ""));
        copyText(result.availableVersion, sizeof(result.availableVersion), preferences.getString("available", ""));
        copyText(result.channel, sizeof(result.channel), preferences.getString("channel", ""));
        copyText(result.target, sizeof(result.target), preferences.getString("target", ""));
        copyText(result.environment, sizeof(result.environment), preferences.getString("env", ""));
        copyText(result.board, sizeof(result.board), preferences.getString("board", ""));
        copyText(result.firmwareUrl, sizeof(result.firmwareUrl), preferences.getString("fw_url", ""));
        copyText(result.sha256, sizeof(result.sha256), preferences.getString("sha256", ""));
    }

    preferences.end();
    return result;
}

bool MaintenanceResultStore::save(const MaintenanceResult& result) {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        return false;
    }

    bool ok = true;
    ok = preferences.putBool("valid", result.valid) == 1U && ok;
    ok = preferences.putBool("success", result.success) == 1U && ok;
    ok = preferences.putBool("upd_avail", result.updateAvailable) == 1U && ok;
    ok = preferences.putBool("notify", result.notificationPending) == 1U && ok;
    ok = preferences.putULong("tls_ms", result.tlsDurationMs) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("uptime_ms", result.recordedUptimeMs) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("heap_min", result.minFreeHeap) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("manifest_sz", result.manifestSize) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("firmware_sz", result.firmwareSize) == sizeof(uint32_t) && ok;

    preferences.putString("command", result.command);
    preferences.putString("http", result.httpLine);
    preferences.putString("detail", result.detail);
    preferences.putString("installed", result.installedVersion);
    preferences.putString("available", result.availableVersion);
    preferences.putString("channel", result.channel);
    preferences.putString("target", result.target);
    preferences.putString("env", result.environment);
    preferences.putString("board", result.board);
    preferences.putString("fw_url", result.firmwareUrl);
    preferences.putString("sha256", result.sha256);

    preferences.end();
    return ok;
}

bool MaintenanceResultStore::clear() {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        return false;
    }
    const bool ok = preferences.clear();
    preferences.end();
    return ok;
}
