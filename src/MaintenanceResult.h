#pragma once

#include <Arduino.h>

struct MaintenanceResult {
    bool valid = false;
    bool success = false;
    bool updateAvailable = false;
    bool notificationPending = false;
    // Canal ressources Web, distinct du firmware ci-dessus : verifie par
    // WebAssetsUpdater::checkForUpdate() pendant le meme redemarrage de
    // maintenance CHECK_VERSION, sans rien telecharger ni deployer.
    bool webAssetsUpdateAvailable = false;
    bool webAssetsNotificationPending = false;
    uint32_t tlsDurationMs = 0U;
    uint32_t recordedUptimeMs = 0U;
    // Date reelle de l'enregistrement (epoch UTC), 0 si l'horloge n'etait pas
    // encore reglee. recordedUptimeMs ne dit que "combien de temps apres le
    // demarrage", ce qui ne repond pas a la seule question utile devant un
    // echec : depuis QUAND. L'horloge survit au redemarrage logiciel qui fait
    // entrer en mode maintenance, donc elle est generalement valide ici.
    uint32_t recordedEpoch = 0U;
    uint32_t minFreeHeap = 0U;
    uint32_t manifestSize = 0U;
    uint32_t firmwareSize = 0U;
    uint32_t downloadedSize = 0U;
    uint32_t downloadDurationMs = 0U;
    char command[24] = "";
    char httpLine[96] = "";
    char detail[128] = "";
    char installedVersion[24] = "";
    char availableVersion[24] = "";
    char webAssetsInstalledVersion[24] = "";
    char webAssetsAvailableVersion[24] = "";
    char channel[16] = "";
    char target[16] = "";
    char environment[40] = "";
    char board[32] = "";
    char firmwareUrl[192] = "";
    char sha256[65] = "";
    char calculatedSha256[65] = "";
};

class MaintenanceResultStore {
public:
    static MaintenanceResult load();
    static bool save(const MaintenanceResult& result);
    static bool clear();
};
