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

String readOptionalString(Preferences& preferences, const char* key) {
    return preferences.isKey(key) ? preferences.getString(key) : String();
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
        copyText(result.command, sizeof(result.command), readOptionalString(preferences, "command"));
        copyText(result.httpLine, sizeof(result.httpLine), readOptionalString(preferences, "http"));
        copyText(result.detail, sizeof(result.detail), readOptionalString(preferences, "detail"));
        copyText(result.installedVersion, sizeof(result.installedVersion), readOptionalString(preferences, "installed"));
        copyText(result.availableVersion, sizeof(result.availableVersion), readOptionalString(preferences, "available"));
        copyText(result.channel, sizeof(result.channel), readOptionalString(preferences, "channel"));
        copyText(result.target, sizeof(result.target), readOptionalString(preferences, "target"));
        copyText(result.environment, sizeof(result.environment), readOptionalString(preferences, "env"));
        copyText(result.board, sizeof(result.board), readOptionalString(preferences, "board"));
        copyText(result.firmwareUrl, sizeof(result.firmwareUrl), readOptionalString(preferences, "fw_url"));
        copyText(result.sha256, sizeof(result.sha256), readOptionalString(preferences, "sha256"));
    }

    preferences.end();
    return result;
}

bool MaintenanceResultStore::save(const MaintenanceResult& result) {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        return false;
    }

    const bool isVersionCheck = strcmp(result.command, "check_version") == 0;
    const bool successfulVersionCheck = isVersionCheck && result.success;

    const bool previousUpdateAvailable = preferences.getBool("upd_avail", false);
    const bool previousNotificationPending = preferences.getBool("notify", false);
    const String previousAvailableVersion = readOptionalString(preferences, "available");

    const bool explicitNotificationAck =
        previousUpdateAvailable &&
        previousNotificationPending &&
        result.updateAvailable &&
        !result.notificationPending &&
        result.availableVersion[0] != '\0' &&
        previousAvailableVersion == result.availableVersion;

    bool updateAvailable = result.updateAvailable;
    bool notificationPending = result.notificationPending;
    uint32_t manifestSize = result.manifestSize;
    uint32_t firmwareSize = result.firmwareSize;
    String installedVersion = result.installedVersion;
    String availableVersion = result.availableVersion;
    String channel = result.channel;
    String target = result.target;
    String environment = result.environment;
    String board = result.board;
    String firmwareUrl = result.firmwareUrl;
    String sha256 = result.sha256;

    if (!successfulVersionCheck) {
        // Un probe GitHub ou un CHECK_VERSION en échec ne doit pas effacer une
        // disponibilité déjà validée. La signalisation locale et mobile reste
        // fondée sur le dernier manifeste entièrement validé.
        updateAvailable = previousUpdateAvailable;
        notificationPending = explicitNotificationAck
            ? false
            : previousNotificationPending;
        manifestSize = preferences.getULong("manifest_sz", 0U);
        firmwareSize = preferences.getULong("firmware_sz", 0U);
        installedVersion = readOptionalString(preferences, "installed");
        availableVersion = previousAvailableVersion;
        channel = readOptionalString(preferences, "channel");
        target = readOptionalString(preferences, "target");
        environment = readOptionalString(preferences, "env");
        board = readOptionalString(preferences, "board");
        firmwareUrl = readOptionalString(preferences, "fw_url");
        sha256 = readOptionalString(preferences, "sha256");
    } else if (result.updateAvailable &&
               previousUpdateAvailable &&
               previousAvailableVersion == result.availableVersion &&
               !previousNotificationPending) {
        // La même version a déjà été livrée au téléphone. Une nouvelle
        // vérification manuelle ne doit pas recréer la notification.
        notificationPending = false;
    }

    bool ok = true;
    ok = preferences.putBool("valid", result.valid) == 1U && ok;
    ok = preferences.putBool("success", result.success) == 1U && ok;
    ok = preferences.putBool("upd_avail", updateAvailable) == 1U && ok;
    ok = preferences.putBool("notify", notificationPending) == 1U && ok;
    ok = preferences.putULong("tls_ms", result.tlsDurationMs) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("uptime_ms", result.recordedUptimeMs) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("heap_min", result.minFreeHeap) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("manifest_sz", manifestSize) == sizeof(uint32_t) && ok;
    ok = preferences.putULong("firmware_sz", firmwareSize) == sizeof(uint32_t) && ok;

    preferences.putString("command", result.command);
    preferences.putString("http", result.httpLine);
    preferences.putString("detail", result.detail);
    preferences.putString("installed", installedVersion);
    preferences.putString("available", availableVersion);
    preferences.putString("channel", channel);
    preferences.putString("target", target);
    preferences.putString("env", environment);
    preferences.putString("board", board);
    preferences.putString("fw_url", firmwareUrl);
    preferences.putString("sha256", sha256);

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
