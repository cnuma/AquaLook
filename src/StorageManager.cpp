#include "StorageManager.h"

#include "EventLog.h"
#include "FaultManager.h"
#include "IncidentManager.h"
#include <cstring>

namespace {
constexpr uint32_t SD_HEALTH_CHECK_INTERVAL_MS = 2000U;
constexpr uint8_t SD_HEALTH_FAILURE_CONFIRMATIONS = 2U;
constexpr uint8_t SD_RECOVERY_MAX_ATTEMPTS = 5U;
constexpr uint32_t SD_SLOW_RECOVERY_INTERVAL_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t SD_RECOVERY_TASK_STACK = 4096U;
constexpr UBaseType_t SD_RECOVERY_TASK_PRIORITY = 1U;
constexpr BaseType_t SD_RECOVERY_TASK_CORE = 0;

const uint32_t SD_RECOVERY_DELAYS_MS[SD_RECOVERY_MAX_ATTEMPTS] = {
    2000U,
    5000U,
    10000U,
    30000U,
    60000U
};

StorageManager* g_registeredStorage = nullptr;

bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}
}

bool StorageManager::lockSd(uint32_t timeoutMs) {
    if (!_sdMutex) return false;
    return xSemaphoreTake(_sdMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void StorageManager::unlockSd() {
    if (_sdMutex) xSemaphoreGive(_sdMutex);
}

void storageHealthUpdate() {
    if (g_registeredStorage) g_registeredStorage->update();
}

void StorageManager::begin() {
    g_registeredStorage = this;
    IncidentManager::begin();

    if (!_sdMutex) _sdMutex = xSemaphoreCreateMutex();

    _sd.end();
    resetCardMetadata();

    _status = StorageStatus::NOT_INITIALIZED;
    _recoveryState = StorageRecoveryState::IDLE;
    _lastHealthCheckMs = 0;
    _healthFailureCount = 0;
    _recoveryAttempt = 0;
    _nextRecoveryAttemptMs = 0;
    _unavailableSinceMs = 0;
    _restartRecommended = false;
    _slowRecoveryMode = false;
    _slowRecoveryCount = 0;
    _recoveryTaskResult = RecoveryTaskResult::NONE;
    _recoveryTaskHandle = nullptr;
    _lastMountFailureReason = "not_attempted";

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    if (mountSd(true)) {
        FaultManager::setActive(FaultId::STORAGE_SD, false);

        const PersistentIncidentSnapshot incident = IncidentManager::storageSd();
        if (incident.state == IncidentState::ACTIVE) {
            IncidentManager::recoverStorageSd("available_after_reboot");
        }

        logMounted(false, 0);
        // Auto-test d'ecriture retire du demarrage : il ecrivait, renommait
        // puis supprimait un fichier DANS /www a chaque boot, creant une
        // fenetre de corruption du repertoire qui contient toutes les
        // ressources Web — au moment ou l'alimentation est la moins stable.
        // Perte reelle de /www constatee le 17 aout 2026 apres 28 boots.
        // Desormais uniquement a la demande, via /api/debug/sd-selftest.
        return;
    }

    FaultManager::setActive(FaultId::STORAGE_SD, true);
    IncidentManager::activateStorageSd(_lastMountFailureReason);

    EventLog::log(
        LOG_WARN,
        "Stockage: montage SD initial echoue raison=%s",
        _lastMountFailureReason
    );

    _unavailableSinceMs = millis();
    scheduleRecovery(_unavailableSinceMs);
}

void StorageManager::end() {
    if (_recoveryTaskResult == RecoveryTaskResult::RUNNING) {
        EventLog::log(
            LOG_WARN,
            "Stockage: fermeture ignoree pendant une tentative de remontage"
        );
        return;
    }

    if (lockSd()) {
        _sd.end();
        unlockSd();
    }
    resetCardMetadata();

    _status = StorageStatus::NOT_INITIALIZED;
    _recoveryState = StorageRecoveryState::IDLE;
    _lastHealthCheckMs = 0;
    _healthFailureCount = 0;
    _recoveryAttempt = 0;
    _nextRecoveryAttemptMs = 0;
    _unavailableSinceMs = 0;
    _restartRecommended = false;
    _slowRecoveryMode = false;
    _slowRecoveryCount = 0;
    _recoveryTaskResult = RecoveryTaskResult::NONE;
    _recoveryTaskHandle = nullptr;
    _lastMountFailureReason = "not_attempted";

    if (g_registeredStorage == this) {
        g_registeredStorage = nullptr;
    }
}

void StorageManager::update() {
    const uint32_t nowMs = millis();

    if (_recoveryState == StorageRecoveryState::WAITING_RETRY) {
        if (deadlineReached(nowMs, _nextRecoveryAttemptMs)) {
            startRecoveryTask(nowMs);
        }
        return;
    }

    if (_recoveryState == StorageRecoveryState::RETRYING) {
        processRecoveryTaskResult(nowMs);
        return;
    }

    if (_recoveryState == StorageRecoveryState::FAILED) {
        if (deadlineReached(nowMs, _nextRecoveryAttemptMs)) {
            startRecoveryTask(nowMs);
        }
        return;
    }

    if (!_sdAvailable || _status != StorageStatus::READY) return;

    if (nowMs - _lastHealthCheckMs < SD_HEALTH_CHECK_INTERVAL_MS) return;
    _lastHealthCheckMs = nowMs;

    // La tache Web asynchrone peut etre en train de lire un fichier au meme
    // instant : ne pas bloquer la boucle principale si le bus est occupe,
    // ce controle sera simplement retente au prochain passage.
    if (!lockSd(50U)) return;
    const bool indexPresent = _sd.exists("/www/index.html");
    unlockSd();

    if (indexPresent) {
        _healthFailureCount = 0;
        return;
    }

    if (_healthFailureCount < 0xFFU) {
        _healthFailureCount++;
    }

    if (_healthFailureCount < SD_HEALTH_FAILURE_CONFIRMATIONS) {
        EventLog::log(
            LOG_WARN,
            "Stockage: controle SD echoue confirmation=%u/%u",
            static_cast<unsigned>(_healthFailureCount),
            static_cast<unsigned>(SD_HEALTH_FAILURE_CONFIRMATIONS)
        );
        return;
    }

    markUnavailable(
        StorageStatus::READ_ERROR,
        "health_check_failed",
        "/www/index.html"
    );
}

bool StorageManager::existsOnSd(const char* path) {
    if (!_sdAvailable || _status != StorageStatus::READY ||
        !path || path[0] != '/') {
        return false;
    }
    if (!lockSd()) return false;
    const bool found = _sd.exists(path);
    unlockSd();
    return found;
}

bool StorageManager::openRead(const char* path, FsFile& file) {
    if (!_sdAvailable ||
        _status != StorageStatus::READY ||
        !path ||
        path[0] != '/') {
        return false;
    }

    if (!lockSd()) return false;
    if (file.isOpen()) file.close();
    file = _sd.open(path, O_RDONLY);
    unlockSd();
    return file.isOpen();
}

int32_t StorageManager::readChunk(FsFile& file, uint8_t* buffer, size_t maxLen) {
    if (!file.isOpen() || !buffer || maxLen == 0U) return -1;
    if (!lockSd()) return -1;
    const int32_t count = file.read(buffer, maxLen);
    unlockSd();
    return count;
}

// Variante non bloquante de readChunk(), destinee aux rappels AsyncTCP.
// Retourne READ_CHUNK_BUSY (et ne lit rien) si le bus SD est occupe, au lieu
// d'attendre le verrou.
//
// Raison d'etre : AsyncTCP execute TOUS les rappels de TOUTES les connexions
// sur une seule tache. Un readChunk() classique y attend le mutex jusqu'a
// 200 ms ; avec plusieurs fichiers SD demandes en parallele — ce que fait
// tout navigateur — les rappels se disputent le verrou et bloquent cette
// tache unique, donc l'integralite du serveur Web, y compris les routes API
// qui ne touchent pas a la SD. Mesure du 16 aout 2026 : 6 requetes
// simultanees suffisaient a rendre le module totalement muet (page non
// chargee, /app.js et /style.css en echec, /index.html a 20 s).
int32_t StorageManager::readChunkNonBlocking(FsFile& file, uint8_t* buffer, size_t maxLen) {
    if (!file.isOpen() || !buffer || maxLen == 0U) return -1;
    if (!lockSd(0U)) return READ_CHUNK_BUSY;
    const int32_t count = file.read(buffer, maxLen);
    unlockSd();
    return count;
}

void StorageManager::closeFile(FsFile& file) {
    if (!file.isOpen()) return;
    if (!lockSd()) return;
    file.close();
    unlockSd();
}

bool StorageManager::openWrite(const char* path, FsFile& file) {
    // Ecriture autorisee des que la carte est montee, meme si les ressources
    // Web manquent : c'est precisement dans cet etat qu'il faut pouvoir ecrire
    // pour reparer (voir la note sur _cardMounted dans StorageManager.h).
    if (!_cardMounted ||
        !path ||
        path[0] != '/') {
        return false;
    }

    if (!lockSd()) return false;
    if (file.isOpen()) file.close();

    // Creer le repertoire parent s'il manque. Indispensable a la reparation a
    // distance : quand /www a disparu, il faut pouvoir le recreer avant d'y
    // reecrire index.html, sans quoi le module reste inutilisable pour une
    // simple absence de repertoire (constate le 17 aout 2026).
    const char* lastSlash = strrchr(path, '/');
    if (lastSlash && lastSlash != path) {
        char parent[96];
        const size_t len = static_cast<size_t>(lastSlash - path);
        if (len < sizeof(parent)) {
            memcpy(parent, path, len);
            parent[len] = '\0';
            if (!_sd.exists(parent)) _sd.mkdir(parent);
        }
    }

    // O_CREAT | O_TRUNC : remplacement complet du contenu existant, jamais
    // une ecriture partielle superposee a un ancien fichier plus long.
    file = _sd.open(path, O_WRONLY | O_CREAT | O_TRUNC);
    unlockSd();
    return file.isOpen();
}

int32_t StorageManager::writeChunk(FsFile& file, const uint8_t* buffer, size_t len) {
    if (!file.isOpen() || !buffer || len == 0U) return -1;
    if (!lockSd()) return -1;
    const int32_t written = file.write(buffer, len);
    unlockSd();
    return written;
}

bool StorageManager::deleteOnSd(const char* path) {
    if (!_cardMounted ||
        !path ||
        path[0] != '/') {
        return false;
    }
    if (!lockSd()) return false;
    const bool ok = !_sd.exists(path) || _sd.remove(path);
    unlockSd();
    return ok;
}


// ── Deploiement transactionnel des ressources Web ─────────────────────────
// Voir la note detaillee dans StorageManager.h. Regle : ne jamais ecrire
// directement dans /www.

bool StorageManager::removeDirectoryContents(const char* dirPath) {
    if (!_cardMounted || !dirPath) return false;
    if (!lockSd(1000U)) return false;

    FsFile dir;
    if (!dir.open(dirPath, O_RDONLY)) {
        unlockSd();
        return false;   // absent : rien a supprimer, l'appelant decide
    }

    bool allRemoved = true;
    char name[64];
    FsFile entry;
    while (entry.openNext(&dir, O_RDONLY)) {
        const bool isDir = entry.isDir();
        entry.getName(name, sizeof(name));
        entry.close();
        if (isDir) {
            // /www est plat par construction. Un sous-repertoire inattendu est
            // signale plutot qu'ignore en silence.
            allRemoved = false;
            continue;
        }
        char full[128];
        snprintf(full, sizeof(full), "%s/%s", dirPath, name);
        if (!_sd.remove(full)) allRemoved = false;
    }
    dir.close();
    unlockSd();
    return allRemoved;
}

bool StorageManager::beginAssetStaging() {
    if (!_cardMounted) return false;

    // Repartir d'un transit propre : un reliquat d'un essai precedent
    // interrompu melangerait d'anciens fichiers aux nouveaux.
    removeDirectoryContents(ASSETS_STAGING);

    if (!lockSd(1000U)) return false;
    _sd.rmdir(ASSETS_STAGING);
    const bool created = _sd.mkdir(ASSETS_STAGING);
    unlockSd();

    EventLog::log(created ? LOG_INFO : LOG_ERROR,
                  "Deploiement: preparation du transit %s %s",
                  ASSETS_STAGING, created ? "OK" : "ECHEC");
    return created;
}

bool StorageManager::commitAssetStaging() {
    if (!_cardMounted) return false;

    // Bascule fichier par fichier, et non par renommage de repertoire.
    //
    // SdFat ouvre le chemin en lecture seule dans rename() (FatVolume.h :
    // open(vwd(), oldPath, O_RDONLY) puis file.rename(...)), ce qui ne permet
    // pas de mettre a jour l'entree ".." d'un sous-repertoire : renommer /www
    // echouait systematiquement (etape 1/2), constate le 17 aout 2026 alors
    // que les 10 fichiers etaient tous telecharges et verifies.
    //
    // Compromis assume : la bascule n'est plus atomique. Une coupure pendant
    // cette phase laisse un melange de fichiers ANCIENS et NOUVEAUX, tous
    // complets et valides individuellement — jamais de fichier tronque. C'est
    // tres different de l'incident du matin, ou l'ecriture longue (le
    // telechargement) se faisait dans /www ; ici cette phase reste isolee dans
    // le transit, et seule la phase de deplacement, de l'ordre de la seconde,
    // touche /www. recoverInterruptedStaging() reprend les deplacements
    // restants au demarrage suivant.
    if (!lockSd(2000U)) return false;

    FsFile dir;
    if (!dir.open(ASSETS_STAGING, O_RDONLY)) {
        unlockSd();
        EventLog::log(LOG_ERROR, "Deploiement: transit absent, bascule annulee");
        return false;
    }

    uint8_t moved = 0U;
    bool allMoved = true;
    char name[64];
    FsFile entry;
    while (entry.openNext(&dir, O_RDONLY)) {
        const bool isDir = entry.isDir();
        entry.getName(name, sizeof(name));
        entry.close();
        if (isDir) continue;   // le transit est plat par construction

        char from[128];
        char to[128];
        snprintf(from, sizeof(from), "%s/%s", ASSETS_STAGING, name);
        snprintf(to, sizeof(to), "%s/%s", ASSETS_DIR, name);

        // Un renommage FAT echoue si la destination existe : retirer d'abord.
        if (_sd.exists(to)) _sd.remove(to);
        if (_sd.rename(from, to)) {
            moved++;
        } else {
            allMoved = false;
            EventLog::log(LOG_ERROR, "Deploiement: deplacement echoue pour %s", name);
        }
    }
    dir.close();
    _sd.rmdir(ASSETS_STAGING);
    unlockSd();

    EventLog::log(allMoved ? LOG_INFO : LOG_ERROR,
                  "Deploiement: bascule %s, %u fichier(s) deplace(s)",
                  allMoved ? "effectuee" : "INCOMPLETE",
                  static_cast<unsigned>(moved));
    return allMoved && moved > 0U;
}

void StorageManager::recoverInterruptedStaging() {
    if (!lockSd(1000U)) return;

    const bool hasAssets  = _sd.exists(ASSETS_DIR);
    const bool hasStaging = _sd.exists(ASSETS_STAGING);
    const bool hasOld     = _sd.exists(ASSETS_OLD);

    // Transit encore present : une bascule a ete interrompue en cours de
    // deplacement. La reprendre est sans risque, l'operation etant idempotente
    // (chaque fichier restant remplace sa version ancienne).
    if (hasStaging) {
        unlockSd();
        EventLog::log(LOG_WARN, "Deploiement: transit restant, reprise de la bascule");
        commitAssetStaging();
        return;
    }

    // Coupure juste avant le second renommage : l'ancien est encore la, le
    // transit n'a pas pris sa place. Restaurer l'ancien, qui est sain.
    if (!hasAssets && hasOld) {
        const bool done = _sd.rename(ASSETS_OLD, ASSETS_DIR);
        unlockSd();
        EventLog::log(done ? LOG_WARN : LOG_ERROR,
                      "Deploiement: ancienne version %s apres interruption",
                      done ? "restauree" : "IRRECUPERABLE");
        return;
    }

    unlockSd();

    // Cas nominal avec residus : la bascule a reussi, le menage n'a pas fini.
    if (hasAssets && hasOld) {
        removeDirectoryContents(ASSETS_OLD);
        if (lockSd(1000U)) { _sd.rmdir(ASSETS_OLD); unlockSd(); }
        EventLog::log(LOG_INFO, "Deploiement: residus de bascule nettoyes");
    }
}

void StorageManager::selfTestSdWrite() {
    // Hors de /www : une corruption pendant ce test ne doit jamais pouvoir
    // emporter le repertoire des ressources Web.
    static constexpr const char* TMP_PATH  = "/diag/.write_selftest.tmp";
    static constexpr const char* FINAL_PATH = "/diag/.write_selftest";
    static constexpr const char* CONTENT = "AquaLook SD write self-test";
    const size_t len = strlen(CONTENT);

    FsFile f;
    if (!openWrite(TMP_PATH, f)) {
        EventLog::log(LOG_WARN, "Stockage: auto-test ecriture SD echoue (ouverture)");
        return;
    }
    const int32_t written = writeChunk(f, reinterpret_cast<const uint8_t*>(CONTENT), len);
    closeFile(f);

    if (written != static_cast<int32_t>(len)) {
        EventLog::log(
            LOG_WARN,
            "Stockage: auto-test ecriture SD echoue (ecrit %ld/%u octets)",
            static_cast<long>(written),
            static_cast<unsigned>(len)
        );
        deleteOnSd(TMP_PATH);
        return;
    }

    if (!renameOnSd(TMP_PATH, FINAL_PATH)) {
        EventLog::log(LOG_WARN, "Stockage: auto-test ecriture SD echoue (renommage)");
        deleteOnSd(TMP_PATH);
        return;
    }

    char buf[64] = {0};
    bool readOk = false;
    FsFile rf;
    if (openRead(FINAL_PATH, rf)) {
        const int32_t n = readChunk(rf, reinterpret_cast<uint8_t*>(buf), sizeof(buf) - 1U);
        closeFile(rf);
        readOk = (n == static_cast<int32_t>(len)) && (memcmp(buf, CONTENT, len) == 0);
    }
    deleteOnSd(FINAL_PATH);

    if (readOk) {
        EventLog::log(LOG_INFO, "Stockage: auto-test ecriture/lecture SD OK");
    } else {
        EventLog::log(LOG_WARN, "Stockage: auto-test ecriture SD : relecture incoherente");
    }
}

bool StorageManager::renameOnSd(const char* fromPath, const char* toPath) {
    if (!_cardMounted ||
        !fromPath || fromPath[0] != '/' ||
        !toPath || toPath[0] != '/') {
        return false;
    }
    if (!lockSd()) return false;
    // rename() sur SdFat echoue si la cible existe deja : la supprimer
    // d'abord rend l'appel idempotent (utile pour "remplacer le fichier
    // final par la version fraichement telechargee").
    if (_sd.exists(toPath)) _sd.remove(toPath);
    const bool ok = _sd.rename(fromPath, toPath);
    unlockSd();
    return ok;
}

void StorageManager::reportReadError(const char* path) {
    if (_recoveryState != StorageRecoveryState::IDLE ||
        !_sdAvailable ||
        _status != StorageStatus::READY) {
        return;
    }

    markUnavailable(
        StorageStatus::READ_ERROR,
        "read_error",
        path
    );
}

bool StorageManager::mountSd(bool publishAvailability) {
    // Appelee depuis begin() (tache principale) et depuis la tache dediee de
    // remontage : un verrou plus long est acceptable ici, le (re)montage
    // n'est pas une operation frequente comme readChunk()/existsOnSd().
    if (!lockSd(2000U)) {
        _status = StorageStatus::SD_UNAVAILABLE;
        _lastMountFailureReason = "sd_busy";
        return false;
    }

    _sd.end();
    resetCardMetadata();

    const SdSpiConfig sdConfig(
        SD_CS_PIN,
        SHARED_SPI,
        SD_SCK_MHZ(0),
        &_softSpi
    );

    if (!_sd.begin(sdConfig)) {
        _status = StorageStatus::SD_UNAVAILABLE;
        _lastMountFailureReason = "sd_begin_failed";
        unlockSd();
        return false;
    }

    if (!_sd.card() || !_sd.vol()) {
        _status = StorageStatus::SD_UNAVAILABLE;
        _lastMountFailureReason = "volume_unavailable";
        _sd.end();
        unlockSd();
        return false;
    }

    _cardType = _sd.card()->type();
    _cardSizeBytes =
        static_cast<uint64_t>(_sd.card()->sectorCount()) * 512ULL;

    const uint64_t bytesPerCluster = _sd.vol()->bytesPerCluster();
    const uint64_t clusterCount = _sd.vol()->clusterCount();
    _totalBytes = clusterCount * bytesPerCluster;
    _usedBytes = 0;

    // Rattraper un deploiement interrompu AVANT de juger de la presence des
    // ressources : sans cela, une coupure pendant la bascule ferait conclure a
    // tort a des ressources manquantes alors qu'elles sont dans le transit.
    unlockSd();
    recoverInterruptedStaging();
    if (!lockSd(1000U)) return false;

    if (!_sd.exists("/www") || !_sd.exists("/www/index.html")) {
        _status = StorageStatus::WEB_ASSETS_MISSING;
        _lastMountFailureReason = "web_assets_missing";
        // La carte reste MONTEE : elle fonctionne, seuls les fichiers manquent.
        // C'est ce qui permet de les reecrire a distance et de retrouver un
        // module sain sans intervention physique. Les ressources Web restent
        // annoncees indisponibles (_sdAvailable faux), donc le serveur bascule
        // sur le repli LittleFS — mais les ecritures, elles, sont autorisees.
        // Les metadonnees ne sont plus effacees : elles etaient lues avec
        // succes, les remettre a zero affichait "carte inconnue, 0 octet" et
        // laissait croire a une carte morte alors qu'elle repond.
        _cardMounted = true;
        _sdAvailable = false;
        unlockSd();
        return false;
    }

    _status = StorageStatus::READY;
    _cardMounted = true;
    _lastMountFailureReason = "none";
    _lastHealthCheckMs = millis();
    _healthFailureCount = 0;
    _sdAvailable = publishAvailability;
    unlockSd();
    return true;
}

void StorageManager::resetCardMetadata() {
    _sdAvailable = false;
    _cardType = 0;
    _cardSizeBytes = 0;
    _totalBytes = 0;
    _usedBytes = 0;
}

void StorageManager::markUnavailable(StorageStatus status,
                                     const char* reason,
                                     const char* path) {
    if (_recoveryState != StorageRecoveryState::IDLE) return;

    _status = status;
    _sdAvailable = false;
    _healthFailureCount = 0;
    _restartRecommended = false;
    _slowRecoveryMode = false;
    _slowRecoveryCount = 0;
    _lastMountFailureReason = reason ? reason : "unknown";

    FaultManager::setActive(FaultId::STORAGE_SD, true);
    IncidentManager::activateStorageSd(_lastMountFailureReason);

    EventLog::log(
        LOG_ERROR,
        "Stockage: SD indisponible raison=%s chemin=%s",
        _lastMountFailureReason,
        path ? path : "inconnu"
    );

    if (lockSd()) {
        _sd.end();
        unlockSd();
    }
    resetCardMetadata();

    _unavailableSinceMs = millis();
    _recoveryAttempt = 0;
    _recoveryTaskResult = RecoveryTaskResult::NONE;
    scheduleRecovery(_unavailableSinceMs);
}

void StorageManager::scheduleRecovery(uint32_t nowMs) {
    if (_recoveryAttempt >= SD_RECOVERY_MAX_ATTEMPTS) {
        _restartRecommended = true;
        _slowRecoveryMode = true;

        IncidentManager::escalateStorageSdRecoveryFailure();

        EventLog::log(
            LOG_ERROR,
            "Stockage: echec apres %u essais, reprise lente toutes les 10min",
            static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS)
        );

        scheduleSlowRecovery(nowMs);
        return;
    }

    const uint32_t delayMs = SD_RECOVERY_DELAYS_MS[_recoveryAttempt];
    _nextRecoveryAttemptMs = nowMs + delayMs;
    _recoveryState = StorageRecoveryState::WAITING_RETRY;

    EventLog::log(
        LOG_INFO,
        "Stockage: remontage programme essai=%u/%u dans=%lus",
        static_cast<unsigned>(_recoveryAttempt + 1U),
        static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS),
        static_cast<unsigned long>(delayMs / 1000U)
    );
}

void StorageManager::scheduleSlowRecovery(uint32_t nowMs) {
    _slowRecoveryMode = true;
    _nextRecoveryAttemptMs = nowMs + SD_SLOW_RECOVERY_INTERVAL_MS;
    _recoveryState = StorageRecoveryState::FAILED;

    EventLog::log(
        LOG_INFO,
        "Stockage: prochaine tentative lente dans=10min"
    );
}

void StorageManager::startRecoveryTask(uint32_t nowMs) {
    (void)nowMs;

    if (_recoveryTaskResult == RecoveryTaskResult::RUNNING) return;

    _recoveryState = StorageRecoveryState::RETRYING;

    if (_slowRecoveryMode) {
        _slowRecoveryCount++;
    } else {
        _recoveryAttempt++;
    }

    _recoveryTaskResult = RecoveryTaskResult::RUNNING;

    if (_slowRecoveryMode) {
        EventLog::log(
            LOG_INFO,
            "Stockage: tentative lente=%lu tache=core0",
            static_cast<unsigned long>(_slowRecoveryCount)
        );
    } else {
        EventLog::log(
            LOG_INFO,
            "Stockage: tentative remontage=%u/%u tache=core0",
            static_cast<unsigned>(_recoveryAttempt),
            static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS)
        );
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        recoveryTaskEntry,
        "sd-recovery",
        SD_RECOVERY_TASK_STACK,
        this,
        SD_RECOVERY_TASK_PRIORITY,
        &_recoveryTaskHandle,
        SD_RECOVERY_TASK_CORE
    );

    if (created != pdPASS) {
        _recoveryTaskHandle = nullptr;
        _recoveryTaskResult = RecoveryTaskResult::START_FAILED;
    }
}

void StorageManager::processRecoveryTaskResult(uint32_t nowMs) {
    const RecoveryTaskResult result = _recoveryTaskResult;

    if (result == RecoveryTaskResult::NONE ||
        result == RecoveryTaskResult::RUNNING) {
        return;
    }

    _recoveryTaskResult = RecoveryTaskResult::NONE;
    _recoveryTaskHandle = nullptr;

    if (result == RecoveryTaskResult::SUCCESS) {
        const uint32_t downtimeMs = nowMs - _unavailableSinceMs;

        _sdAvailable = true;
        _recoveryState = StorageRecoveryState::IDLE;
        _nextRecoveryAttemptMs = 0;
        _restartRecommended = false;

        FaultManager::setActive(FaultId::STORAGE_SD, false);
        IncidentManager::recoverStorageSd(
            _slowRecoveryMode ? "slow_recovery_success" : "recovery_success"
        );

        logMounted(true, downtimeMs);
        _slowRecoveryMode = false;
        return;
    }

    if (result == RecoveryTaskResult::START_FAILED) {
        _lastMountFailureReason = "task_start_failed";
    }

    if (_slowRecoveryMode) {
        EventLog::log(
            LOG_WARN,
            "Stockage: tentative lente=%lu echouee raison=%s",
            static_cast<unsigned long>(_slowRecoveryCount),
            _lastMountFailureReason
        );
        scheduleSlowRecovery(nowMs);
        return;
    }

    EventLog::log(
        LOG_WARN,
        "Stockage: remontage %u/%u echoue raison=%s",
        static_cast<unsigned>(_recoveryAttempt),
        static_cast<unsigned>(SD_RECOVERY_MAX_ATTEMPTS),
        _lastMountFailureReason
    );

    scheduleRecovery(nowMs);
}

void StorageManager::recoveryTaskEntry(void* parameter) {
    StorageManager* storage = static_cast<StorageManager*>(parameter);

    if (!storage) {
        vTaskDelete(nullptr);
        return;
    }

    const bool mounted = storage->mountSd(false);
    storage->_recoveryTaskResult = mounted
        ? RecoveryTaskResult::SUCCESS
        : RecoveryTaskResult::FAILED;

    vTaskDelete(nullptr);
}

void StorageManager::logMounted(bool recovered, uint32_t downtimeMs) {
    EventLog::log(
        LOG_INFO,
        "Stockage: ressources Web SD validees dans /www"
    );

    EventLog::log(
        LOG_INFO,
        "Stockage: SD montee type=%s capacite=%llu Mo total=%llu Mo",
        cardTypeName(),
        static_cast<unsigned long long>(_cardSizeBytes / (1024ULL * 1024ULL)),
        static_cast<unsigned long long>(_totalBytes / (1024ULL * 1024ULL))
    );

    if (recovered) {
        EventLog::log(
            LOG_INFO,
            "Stockage: SD recuperee essai=%u lentes=%lu indisponible=%lus",
            static_cast<unsigned>(_recoveryAttempt),
            static_cast<unsigned long>(_slowRecoveryCount),
            static_cast<unsigned long>(downtimeMs / 1000U)
        );
    }
}

const char* StorageManager::statusCode() const {
    switch (_status) {
        case StorageStatus::READY:              return "ready";
        case StorageStatus::SD_UNAVAILABLE:     return "sd-unavailable";
        case StorageStatus::WEB_ASSETS_MISSING: return "web-assets-missing";
        case StorageStatus::READ_ERROR:         return "read-error";
        default:                                return "not-initialized";
    }
}

const char* StorageManager::statusMessage() const {
    if (_slowRecoveryMode) {
        return "Carte SD toujours indisponible apres les essais rapides. "
               "Une tentative automatique est effectuee toutes les 10 minutes; "
               "l'interface LittleFS de secours reste active.";
    }

    if (_recoveryState == StorageRecoveryState::WAITING_RETRY ||
        _recoveryState == StorageRecoveryState::RETRYING) {
        return "Carte SD indisponible. Recuperation automatique en cours; "
               "l'interface LittleFS de secours reste active.";
    }

    switch (_status) {
        case StorageStatus::READY:
            return "Carte SD operationnelle, ressources Web disponibles.";
        case StorageStatus::SD_UNAVAILABLE:
            return "Carte SD absente, illisible ou corrompue. "
                   "Interface de secours LittleFS utilisee.";
        case StorageStatus::WEB_ASSETS_MISSING:
            return "Carte SD montee, mais /www/index.html est absent. "
                   "Interface de secours LittleFS utilisee.";
        case StorageStatus::READ_ERROR:
            return "Erreur de lecture sur la carte SD. "
                   "Interface de secours LittleFS utilisee.";
        default:
            return "Stockage SD non initialise.";
    }
}

const char* StorageManager::recoveryStateCode() const {
    switch (_recoveryState) {
        case StorageRecoveryState::IDLE:          return "idle";
        case StorageRecoveryState::WAITING_RETRY: return "waiting-retry";
        case StorageRecoveryState::RETRYING:      return "retrying";
        case StorageRecoveryState::FAILED:        return "failed-slow-retry";
        default:                                  return "unknown";
    }
}

uint8_t StorageManager::recoveryMaxAttempts() const {
    return SD_RECOVERY_MAX_ATTEMPTS;
}

const char* StorageManager::cardTypeName() const {
    switch (_cardType) {
        case SD_CARD_TYPE_SD1:  return "SD1";
        case SD_CARD_TYPE_SD2:  return "SD2";
        case SD_CARD_TYPE_SDHC: return "SDHC/SDXC";
        default:                return "inconnue";
    }
}
