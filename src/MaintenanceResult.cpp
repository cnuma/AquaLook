#include "MaintenanceResult.h"

#include <Preferences.h>
#include <cstring>

namespace {
constexpr char NVS_NAMESPACE[] = "aq_maint_res";
constexpr char NVS_KEY[] = "blob";
constexpr uint32_t NVS_MAGIC = 0x53455252UL; // "RRES" lu petit-boutiste
constexpr uint16_t NVS_SCHEMA = 1U;

// Bloc unique, a l'image de PersistedConfig dans ConfigManager : une seule
// ecriture NVS au lieu d'une quinzaine de cles separees. Chaque cle NVS a un
// cout fixe minimal independant de sa taille ; regrouper les champs en un
// seul blob reduit fortement l'empreinte totale et le nombre d'ecritures
// flash par sauvegarde.
struct PersistedMaintenanceResult {
    uint32_t magic;
    uint16_t schema;
    uint16_t payloadSize;
    MaintenanceResult data;
    uint32_t crc32;
};

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
    return ~crc;
}

void copyText(char* destination, size_t destinationSize, const char* source) {
    if (destinationSize == 0U) return;
    std::strncpy(destination, source ? source : "", destinationSize - 1U);
    destination[destinationSize - 1U] = '\0';
}

// Charge le bloc precedent sans passer par l'API publique load(), afin de
// distinguer explicitement "aucun bloc NVS" de "bloc invalide" au besoin.
MaintenanceResult loadRaw(Preferences& preferences) {
    MaintenanceResult empty;
    const size_t len = preferences.getBytesLength(NVS_KEY);
    if (len != sizeof(PersistedMaintenanceResult)) return empty;

    // Alloue sur le tas et non sur la pile : PersistedMaintenanceResult pese
    // environ 760 octets, et ce code est appele depuis des taches a pile
    // etroite. Le 17 aout 2026, l'enchainement load() puis save() depuis la
    // tache de notification (4 Ko) a fait deborder le canari de pile. Meme
    // precaution que ConfigManager::save(), pour la meme raison.
    PersistedMaintenanceResult* blob =
        static_cast<PersistedMaintenanceResult*>(malloc(sizeof(PersistedMaintenanceResult)));
    if (blob == nullptr) return empty;

    const size_t read = preferences.getBytes(NVS_KEY, blob, sizeof(*blob));
    if (read != sizeof(*blob) ||
        blob->magic != NVS_MAGIC ||
        blob->schema != NVS_SCHEMA ||
        blob->payloadSize != sizeof(*blob)) {
        free(blob);
        return empty;
    }
    if (crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                    offsetof(PersistedMaintenanceResult, crc32)) != blob->crc32) {
        free(blob);
        return empty;
    }
    const MaintenanceResult data = blob->data;
    free(blob);
    return data;
}
}

MaintenanceResult MaintenanceResultStore::load() {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, true)) return MaintenanceResult{};
    const MaintenanceResult result = loadRaw(preferences);
    preferences.end();
    return result;
}

bool MaintenanceResultStore::save(const MaintenanceResult& result) {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return false;

    const MaintenanceResult previous = loadRaw(preferences);

    const bool isVersionCheck = strcmp(result.command, "check_version") == 0;
    const bool isDownloadTest = strcmp(result.command, "download_update_test") == 0;
    const bool isStageTest = strcmp(result.command, "stage_update_test") == 0;
    const bool successfulVersionCheck = isVersionCheck && result.success;
    const bool successfulInstall = strcmp(result.command, "install_update") == 0 && result.success;

    const bool previousUpdateAvailable = previous.updateAvailable;
    const bool previousNotificationPending = previous.notificationPending;
    const bool explicitNotificationAck = previousUpdateAvailable && previousNotificationPending &&
        result.updateAvailable && !result.notificationPending && result.availableVersion[0] != '\0' &&
        strcmp(previous.availableVersion, result.availableVersion) == 0;

    MaintenanceResult merged = result;

    if (!successfulVersionCheck) {
        // Un INSTALL_UPDATE reussi consomme la mise a jour en attente : la
        // notification et le drapeau "mise a jour disponible" ne doivent pas
        // survivre a la bascule, sinon le nouveau firmware demarre en
        // pretendant a tort qu'une mise a jour vers lui-meme reste a faire.
        merged.updateAvailable = successfulInstall ? false : previousUpdateAvailable;
        merged.notificationPending = successfulInstall
            ? false
            : (explicitNotificationAck ? false : previousNotificationPending);
        merged.manifestSize = previous.manifestSize;
        merged.firmwareSize = previous.firmwareSize;
        copyText(merged.installedVersion, sizeof(merged.installedVersion), previous.installedVersion);
        copyText(merged.availableVersion, sizeof(merged.availableVersion), previous.availableVersion);
        copyText(merged.channel, sizeof(merged.channel), previous.channel);
        copyText(merged.target, sizeof(merged.target), previous.target);
        copyText(merged.environment, sizeof(merged.environment), previous.environment);
        copyText(merged.board, sizeof(merged.board), previous.board);
        copyText(merged.firmwareUrl, sizeof(merged.firmwareUrl), previous.firmwareUrl);
        copyText(merged.sha256, sizeof(merged.sha256), previous.sha256);
        if (!isDownloadTest && !isStageTest) {
            merged.downloadedSize = previous.downloadedSize;
            merged.downloadDurationMs = previous.downloadDurationMs;
            copyText(merged.calculatedSha256, sizeof(merged.calculatedSha256), previous.calculatedSha256);
        }
    } else {
        merged.downloadedSize = 0U;
        merged.downloadDurationMs = 0U;
        merged.calculatedSha256[0] = '\0';
        if (result.updateAvailable && previousUpdateAvailable &&
            strcmp(previous.availableVersion, result.availableVersion) == 0 &&
            !previousNotificationPending) {
            merged.notificationPending = false;
        }
    }

    // Sur le tas, pour la meme raison que dans loadRaw() : ce bloc de ~760
    // octets s'ajoutait a 'previous' et 'merged' deja sur la pile, soit plus
    // de 2 Ko pour la seule fonction save().
    PersistedMaintenanceResult* blob =
        static_cast<PersistedMaintenanceResult*>(malloc(sizeof(PersistedMaintenanceResult)));
    if (blob == nullptr) {
        preferences.end();
        return false;
    }
    memset(blob, 0, sizeof(*blob));
    blob->magic = NVS_MAGIC;
    blob->schema = NVS_SCHEMA;
    blob->payloadSize = sizeof(*blob);
    blob->data = merged;
    blob->crc32 = crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                             offsetof(PersistedMaintenanceResult, crc32));

    const size_t written = preferences.putBytes(NVS_KEY, blob, sizeof(*blob));
    preferences.end();
    free(blob);
    return written == sizeof(PersistedMaintenanceResult);
}

bool MaintenanceResultStore::clear() {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return false;
    const bool ok = preferences.clear();
    preferences.end();
    return ok;
}
