#pragma once

#include <Arduino.h>
#include <SdFat.h>
#include <freertos/semphr.h>
#include "config.h"

enum class StorageStatus : uint8_t {
    NOT_INITIALIZED = 0,
    READY,
    SD_UNAVAILABLE,
    WEB_ASSETS_MISSING,
    READ_ERROR
};

enum class StorageRecoveryState : uint8_t {
    IDLE = 0,
    WAITING_RETRY,
    RETRYING,
    FAILED
};

class StorageManager {
public:
    void begin();
    void end();
    void update();

    bool isSdAvailable() const { return _sdAvailable; }
    bool areWebAssetsAvailable() const {
        return _status == StorageStatus::READY && _sdAvailable;
    }

    StorageStatus status() const { return _status; }
    const char* statusCode() const;
    const char* statusMessage() const;

    StorageRecoveryState recoveryState() const { return _recoveryState; }
    const char* recoveryStateCode() const;
    uint8_t recoveryAttempt() const { return _recoveryAttempt; }
    uint8_t recoveryMaxAttempts() const;
    bool isRestartRecommended() const { return _restartRecommended; }
    bool isSlowRecoveryMode() const { return _slowRecoveryMode; }
    uint32_t unavailableSinceMs() const { return _unavailableSinceMs; }

    uint8_t cardType() const { return _cardType; }
    uint64_t cardSizeBytes() const { return _cardSizeBytes; }
    uint64_t totalBytes() const { return _totalBytes; }
    uint64_t usedBytes() const { return _usedBytes; }

    bool existsOnSd(const char* path);
    bool openRead(const char* path, FsFile& file);
    int32_t readChunk(FsFile& file, uint8_t* buffer, size_t maxLen);

    // Valeur rendue par readChunkNonBlocking() quand le bus SD est occupe :
    // rien n'a ete lu, le fichier est intact, l'appelant doit reessayer.
    // Distincte de -1 (erreur reelle) et de 0 (fin de fichier).
    static constexpr int32_t READ_CHUNK_BUSY = -2;
    int32_t readChunkNonBlocking(FsFile& file, uint8_t* buffer, size_t maxLen);
    void closeFile(FsFile& file);
    void reportReadError(const char* path);
    const char* cardTypeName() const;

    // ── Ecriture (deploiement distant des ressources Web — voir ROADMAP.md,
    //    "Mise a jour distante des ressources Web") ──────────────────────
    // Meme protection mutex que la lecture ci-dessus. openWrite() tronque le
    // fichier s'il existe deja (remplacement complet, pas d'ecriture
    // partielle superposee a un ancien contenu). Le fichier ecrit avec un
    // chemin temporaire puis renomme (commitWrite) est a la charge de
    // l'appelant : cette classe ne fait qu'exposer les primitives, pas la
    // strategie de coherence en cas d'interruption (a definir avec le reste
    // du flux de deploiement).
    bool openWrite(const char* path, FsFile& file);
    int32_t writeChunk(FsFile& file, const uint8_t* buffer, size_t len);
    bool deleteOnSd(const char* path);
    bool renameOnSd(const char* fromPath, const char* toPath);

private:
    enum class RecoveryTaskResult : uint8_t {
        NONE = 0,
        RUNNING,
        SUCCESS,
        FAILED,
        START_FAILED
    };

    bool mountSd(bool publishAvailability);
    void resetCardMetadata();
    void markUnavailable(StorageStatus status,
                         const char* reason,
                         const char* path);
    void scheduleRecovery(uint32_t nowMs);
    void scheduleSlowRecovery(uint32_t nowMs);
    void startRecoveryTask(uint32_t nowMs);
    void processRecoveryTaskResult(uint32_t nowMs);
    void logMounted(bool recovered, uint32_t downtimeMs);

    // Valide une fois au montage que le chemin d'ecriture (openWrite/
    // writeChunk/renameOnSd) fonctionne reellement sur ce matériel — voir
    // ROADMAP.md, "Mise a jour distante des ressources Web". Ecrit puis
    // relit un petit fichier cache (prefixe '.') sous /www, verifie le
    // contenu, nettoie derriere elle. N'expose aucune route HTTP : ne
    // depend d'aucune entree exterieure, rien a valider/filtrer.
    void selfTestSdWrite();

    static void recoveryTaskEntry(void* parameter);

    SoftSpiDriver<SD_MISO_PIN, SD_MOSI_PIN, SD_SCLK_PIN> _softSpi;
    SdFs _sd;

    // Le bus SPI logiciel de la carte SD est touche a la fois par la boucle
    // principale (controle de sante periodique) et par la tache Web
    // asynchrone (lecture des fichiers statiques). Ce mutex serialise tout
    // acces a _sd/_softSpi entre ces deux contextes, sans quoi deux accès
    // concurrents peuvent corrompre une lecture ou bloquer durablement le bus.
    SemaphoreHandle_t _sdMutex = nullptr;
    bool lockSd(uint32_t timeoutMs = 200U);
    void unlockSd();

    volatile bool _sdAvailable = false;
    volatile StorageStatus _status = StorageStatus::NOT_INITIALIZED;
    volatile StorageRecoveryState _recoveryState = StorageRecoveryState::IDLE;

    uint8_t _cardType = 0;
    uint64_t _cardSizeBytes = 0;
    uint64_t _totalBytes = 0;
    uint64_t _usedBytes = 0;

    uint32_t _lastHealthCheckMs = 0;
    uint8_t _healthFailureCount = 0;

    uint8_t _recoveryAttempt = 0;
    uint32_t _nextRecoveryAttemptMs = 0;
    uint32_t _unavailableSinceMs = 0;
    bool _restartRecommended = false;
    bool _slowRecoveryMode = false;
    uint32_t _slowRecoveryCount = 0;

    volatile RecoveryTaskResult _recoveryTaskResult = RecoveryTaskResult::NONE;
    TaskHandle_t _recoveryTaskHandle = nullptr;

    const char* _lastMountFailureReason = "not_attempted";
};