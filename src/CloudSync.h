#pragma once

#include <Arduino.h>

class WiFiManager;
class RelaisManager;
class ConfigManager;

// ═══════════════════════════════════════════════════════════════
//  CloudSync — telemetrie et sondage de commande vers un service
//  AquaLook a soi (docs/architecture/SYSTEM_ARCHITECTURE.md Sec.5.0,
//  docs/architecture/CLOUD_ENVIRONMENT_EVALUATION.md Sec.7).
//
//  Contrat d'API : POST /v1/report, GET /v1/pending-command,
//  POST /v1/command/ack -- identique cote cloud/api (FastAPI) et
//  cloud/php-mutualized (PHP), le module n'a pas a savoir lequel repond.
//
//  Meme raisonnement que UpdateCheckScheduler pour le mode maintenance :
//  une connexion HTTPS exige du tas contigu (~16 Ko de tampons mbedTLS
//  rien que pour l'E/S, voir docs/engineering/38_MEMORY_MANAGEMENT.md)
//  que le fonctionnement normal ne peut pas garantir. Le mode
//  maintenance offre ~245 Ko de tas libre. utile meme en HTTP simple
//  (tests locaux) pour rester coherent une fois le HTTPS active.
//
//  Perimetre actuel : accuse reception d'une commande en attente, ne
//  l'applique pas encore. Prouver le tuyau complet avant d'y brancher
//  une logique metier (creneaux, reglages).
// ═══════════════════════════════════════════════════════════════

struct CloudSyncConfig {
    bool     enabled        = false;   // desactive tant que non configure
    char     host[64]       = "";
    uint16_t port           = 80U;
    bool     useHttps       = false;   // false en local ; true obligatoire hors LAN
    char     moduleId[32]   = "";
    char     token[80]      = "";
    uint16_t intervalMinutes = 15U;
};

struct CloudSyncResult {
    bool     valid           = false;
    bool     reportSuccess   = false;
    bool     commandReceived = false;
    char     correlationId[40] = "";
    bool     ackSuccess      = false;
    char     detail[64]      = "";
};

class CloudSync {
public:
    // Charge la configuration depuis NVS. Fonction partagee avec
    // CloudSyncScheduler::load() (meme cles), utilisable directement depuis
    // le mode maintenance sans instancier tout le planificateur -- son etat
    // de planification (dernier passage, stabilite WiFi) n'a pas de sens
    // hors de la boucle principale.
    static CloudSyncConfig loadConfig();

    // Envoie la telemetrie, sonde une commande en attente, l'accuse sans
    // encore l'appliquer. A appeler uniquement en mode maintenance (voir
    // note ci-dessus).
    static CloudSyncResult run(const CloudSyncConfig& cfg);
};

class CloudSyncScheduler {
public:
    void begin();

    // Appelee a chaque tour de boucle. Ne fait rien tant que toutes les
    // conditions ne sont pas reunies ; peut declencher un redemarrage.
    void update(bool ntpSynced,
                uint32_t epochSec,
                const WiFiManager* wifi,
                const RelaisManager* relais,
                const ConfigManager* config);

    const CloudSyncConfig& config() const { return _cfg; }
    bool set(bool enabled, const char* host, uint16_t port, bool useHttps,
             const char* moduleId, const char* token, uint16_t intervalMinutes);

    static constexpr uint32_t WIFI_STABLE_MS = 300000UL;   // 5 min, meme seuil qu'UpdateCheckScheduler

private:
    void load();
    void save();
    void saveLastSync(uint32_t epochSec);
    void logBlocked(const char* reason);

    CloudSyncConfig _cfg;
    uint32_t _lastSyncEpochSec = 0U;
    uint32_t _wifiConnectedSinceMs = 0U;
    uint32_t _blockedLogAtMs = 0U;
    bool     _triggered = false;
    bool     _loaded = false;
};
