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
        result.tlsDurationMs = preferences.getULong("tls_ms", 0U);
        result.recordedUptimeMs = preferences.getULong("uptime_ms", 0U);
        result.minFreeHeap = preferences.getULong("heap_min", 0U);
        copyText(result.command, sizeof(result.command), preferences.getString("command", ""));
        copyText(result.httpLine, sizeof(result.httpLine), preferences.getString("http", ""));
        copyText(result.detail, sizeof(result.detail), preferences.getString("detail", ""));
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
    ok = preferences.putULong("tls_ms", result.tlsDurationMs) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("uptime_ms", result.recordedUptimeMs) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("heap_min", result.minFreeHeap) == sizeof(uint32_t) && ok;
    ok = preferences.putString("command", result.command) > 0U && ok;
    ok = preferences.putString("http", result.httpLine) > 0U && ok;
    ok = preferences.putString("detail", result.detail) > 0U && ok;

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
