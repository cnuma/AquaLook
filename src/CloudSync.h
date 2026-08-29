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
//  Perimetre actuel : remonte la configuration effective (creneaux et
//  reglages systeme) pour consultation cote serveur, et accuse reception
//  d'une commande en attente sans encore l'appliquer.
//
//  Sens d'autorite (invariants Sec.9 #1, #2, #3 et #6) : le module reste
//  l'autorite. Le serveur n'est qu'un miroir de la configuration
//  effective ; il pourra proposer une modification sous forme de commande,
//  que le module validera avant application. Le serveur ne detient jamais
//  la verite, sinon une modification faite localement (ecran, portail web
//  du module) serait ecrasee en silence -- ce qu'interdit l'invariant #3.
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

// Accuse a emettre pour une commande deja traitee par la boucle principale.
// La tache ne decide de rien : elle transporte.
struct CloudSyncPendingAck {
    char correlationId[40] = "";
    char state[16]         = "";   // "accepted" ou "refused"
    char detail[96]        = "";
};

struct CloudSyncResult {
    bool     valid           = false;
    bool     reportSuccess   = false;
    bool     configSuccess   = false;
    bool     commandReceived = false;
    char     correlationId[40] = "";
    bool     ackSuccess      = false;
    char     detail[64]      = "";

    // Commande recue et NON encore traitee. Tampon alloue par la tache de
    // synchronisation, dont la boucle principale prend la propriete et
    // qu'elle libere apres traitement.
    //
    // Un pointeur et non un tableau : _pendingResult est copie sous section
    // critique, ou recopier plusieurs kilo-octets bloquerait les
    // interruptions bien trop longtemps. Un pointeur se copie en un mot.
    char*    commandJson     = nullptr;
};

class CloudSync {
public:
    // Charge la configuration depuis NVS. Fonction partagee avec
    // CloudSyncScheduler::load() (meme cles), utilisable directement depuis
    // le mode maintenance sans instancier tout le planificateur -- son etat
    // de planification (dernier passage, stabilite WiFi) n'a pas de sens
    // hors de la boucle principale.
    static CloudSyncConfig loadConfig();

    // Serialise la configuration effective en corps de requete pret a
    // emettre. A appeler depuis la boucle principale, jamais depuis la tache
    // de synchronisation : ConfigManager peut etre modifie a tout instant par
    // l'interface web du module, et une lecture concurrente donnerait un
    // instantane incoherent. Meme discipline que WeatherManager, qui copie sa
    // requete avant de lancer sa tache.
    static String buildConfigBody(const ConfigManager& configManager);

    // Envoie la telemetrie puis la configuration effective, sonde une
    // commande en attente, l'accuse sans encore l'appliquer. Bloque plusieurs
    // secondes : a executer dans une tache dediee (CloudSyncScheduler) ou en
    // mode maintenance, jamais dans la boucle principale.
    static CloudSyncResult run(const CloudSyncConfig& cfg,
                               const String& configBody,
                               const CloudSyncPendingAck& pendingAck);
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
    // Cible des modifications de configuration recues. Fournie separement de
    // update() parce qu'elle doit etre MODIFIABLE, la ou update() ne recoit
    // qu'une reference constante pour construire son rapport.
    void setConfigTarget(ConfigManager* configManager) { _configTarget = configManager; }

    bool set(bool enabled, const char* host, uint16_t port, bool useHttps,
             const char* moduleId, const char* token, uint16_t intervalMinutes);

    static constexpr uint32_t WIFI_STABLE_MS = 300000UL;   // 5 min, meme seuil qu'UpdateCheckScheduler

    // ── Garde memoire, calquee sur WeatherManager ────────────────────────
    //
    // Le seuil porte sur le plus GROS BLOC autant que sur le total libre :
    // le tas de ce module est fragmente par les tampons d'affichage
    // permanents (~95 Ko), et un total confortable en blocs minuscules ne
    // permet toujours pas d'allouer. Voir WeatherManager::startFetch(), ou
    // l'absence de cette garde avait provoque une boucle de redemarrages
    // (bad_alloc non rattrape dans AsyncServer::_accepted).
    //
    // Seuils plus bas que ceux de la meteo (45000/25000) : l'echange cloud
    // porte ~1,3 Ko de corps et des reponses courtes, la ou la meteo
    // telecharge ~17 Ko.
    static constexpr uint32_t MIN_FREE_FOR_SYNC  = 35000UL;
    static constexpr uint32_t MIN_BLOCK_FOR_SYNC = 15000UL;
    static constexpr uint32_t RETRY_ON_LOW_MEMORY_MS = 120000UL;

    static constexpr uint32_t   SYNC_TASK_STACK_BYTES = 8192;
    static constexpr UBaseType_t SYNC_TASK_PRIORITY   = 1;

private:
    void load();
    void save();
    void saveLastSync(uint32_t epochSec);
    void logBlocked(const char* reason);

    bool startSync(const ConfigManager& configManager);
    static void syncTaskEntry(void* context);
    void performSync();
    void applyPendingResult();
    // Applique une commande de configuration. Appelee UNIQUEMENT depuis la
    // boucle principale : elle ecrit en NVS et touche l'etat partage avec
    // l'affichage, ce que la tache de synchronisation n'a pas le droit de
    // faire (voir la note sur applyPendingResult dans CloudSync.cpp).
    void applyCommand(const char* json, const char* correlationId);

    CloudSyncConfig _cfg;
    uint32_t _lastSyncEpochSec = 0U;
    uint32_t _wifiConnectedSinceMs = 0U;
    uint32_t _blockedLogAtMs = 0U;
    uint32_t _deferUntilMs = 0U;
    bool     _triggered = false;
    bool     _loaded = false;

    // Etat partage avec la tache de synchronisation.
    CloudSyncConfig  _taskCfg;
    String           _taskConfigBody;
    CloudSyncResult  _pendingResult;
    volatile bool    _syncInProgress = false;
    volatile bool    _resultReady    = false;

    ConfigManager*   _configTarget = nullptr;

    // Accuse en attente d'emission, produit par applyCommand() et transmis
    // au cycle suivant. Le serveur representera la meme commande tant
    // qu'elle n'est pas reglee : la comparaison des correlationId evite de
    // l'appliquer deux fois.
    CloudSyncPendingAck _pendingAck;
    // Reveille le cycle suivant sans attendre l'intervalle complet, pour que
    // l'accuse parte en quelques secondes plutot qu'au bout de 15 minutes.
    bool             _ackSyncSoon = false;
};
