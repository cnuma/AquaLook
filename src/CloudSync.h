#pragma once

#include <Arduino.h>

class WiFiManager;
class RelaisManager;
class ConfigManager;
class ScheduleManager;
class StorageManager;

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

// Poignee de securite partagee entre la tache de synchronisation et la boucle
// principale, pour permettre a celle-ci de debloquer un appel reseau qui
// degenere bien au-dela de tous les timeouts configures cote client -- voir
// la note detaillee dans CloudSync.cpp (recherche "plafond de securite") et
// la memoire checkpoint-2026-09-28-nuit2-cloudsync-hang-ssl80.md : un
// handshake TLS peut echouer directement (hors chemin WANT_READ/WANT_WRITE)
// bien au-dela de setHandshakeTimeout(), constate le 28 septembre 2026
// (~50-60s au lieu des 4s configures).
//
// Ne transporte JAMAIS de structure mbedTLS ni n'appelle jamais stop() depuis
// l'exterieur : seul le descripteur de socket brut (WiFiClient::fd(), virtuel,
// partage par WiFiClient et WiFiClientSecure) est ferme depuis la boucle
// principale via lwip_shutdown(), ce qui fait echouer proprement l'appel
// bloquant en cours dans LA TACHE elle-meme, sans jamais toucher a l'etat
// partage (ssl_ctx, ssl_conf...) que seule cette tache a le droit de liberer.
struct CloudSyncWatchdog {
    void*    volatile activeClient = nullptr;  // WiFiClient* actif ; nullptr hors synchro
    uint32_t volatile phaseStartMs = 0;        // instant de depart de la PHASE reseau en cours
    bool     volatile fired        = false;    // latch : n'agit qu'une fois par phase
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
    // relais et storage alimentent le bloc "scripts" (entrees, sources,
    // phrases) ; absents, le bloc ne porte que les entetes des scripts.
    static String buildConfigBody(const ConfigManager& configManager,
                                  const RelaisManager* relais = nullptr,
                                  StorageManager* storage = nullptr);

    // Envoie la telemetrie puis, si sendConfig, la configuration effective ;
    // sonde une commande en attente, l'accuse sans encore l'appliquer. Bloque
    // plusieurs secondes : a executer dans une tache dediee (CloudSyncScheduler)
    // ou en mode maintenance, jamais dans la boucle principale.
    //
    // sendConfig est faux quand la revision de configuration n'a pas change
    // depuis le dernier envoi reussi (CloudSyncScheduler::_lastSyncedRevision) :
    // configBody est alors vide et l'etape config est sautee sans connexion.
    // watchdog non nul en usage normal (fourni par CloudSyncScheduler::
    // performSync()) ; nul uniquement pour un appelant qui ne peut pas se
    // faire debloquer de l'exterieur (aucun cas actuel).
    static CloudSyncResult run(const CloudSyncConfig& cfg,
                               const String& configBody,
                               bool sendConfig,
                               const CloudSyncPendingAck& pendingAck,
                               CloudSyncWatchdog* watchdog);
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

    // Un creneau doit etre ecrit AUX DEUX endroits : ScheduleManager pour
    // l'execution, ConfigManager pour la persistance. En oublier un ferait
    // diverger le planning actif de celui enregistre - c'est d'ailleurs ce
    // que fait deja WebManager::handleSetDaySlot pour la voie locale.
    void setScheduleTarget(ScheduleManager* schedule) { _scheduleTarget = schedule; }

    // Sources du bloc "scripts" du miroir (decision D014) : noms des
    // entrees (topologie) et fichiers SD (textes sources, phrases).
    void setReportSources(const RelaisManager* relais, StorageManager* storage) {
        _reportRelais = relais;
        _reportStorage = storage;
    }

    bool set(bool enabled, const char* host, uint16_t port, bool useHttps,
             const char* moduleId, const char* token, uint16_t intervalMinutes);

    // ── Diagnostic du dernier cycle, pour /api/adminStatus ───────────────
    //
    // Lus sans verrou depuis le contexte AsyncTCP (WebManager) : memes
    // scalaires simples que _cfg/_lastSyncedRevision deja aujourd'hui, pas
    // le CloudSyncResult qui porte un pointeur brut (celui-la reste
    // protege par g_cloudSyncMux). Un instantane momentanement perime
    // s'auto-corrige au sondage suivant.
    bool     lastSyncOk() const { return _lastSyncOk; }
    uint32_t lastSyncedRevision() const { return _lastSyncedRevision; }
    // Instant de LANCEMENT de la derniere tentative, pas de sa reussite --
    // voir saveLastSync(). Distinct de lastSuccessEpochSec().
    uint32_t lastAttemptEpochSec() const { return _lastSyncEpochSec; }
    uint32_t lastSuccessEpochSec() const { return _lastSuccessEpochSec; }
    uint8_t  consecutiveFailures() const { return _consecutiveFailures; }

    static constexpr uint32_t WIFI_STABLE_MS = 300000UL;   // 5 min, meme seuil qu'UpdateCheckScheduler

    // Delai reduit quand la configuration a change localement depuis le
    // dernier envoi confirme : ne pas faire attendre au serveur
    // l'intervalle nominal (5-15 min) pour un reglage que l'utilisateur
    // vient de modifier sur l'ecran ou le portail web. Sans effet si la
    // revision n'a pas bouge -- l'intervalle normal s'applique alors,
    // comme avant. Ajoute le 27 septembre 2026.
    static constexpr uint32_t SYNC_SOON_SECONDS = 30UL;
    // Meme discipline que StorageManager::SD_HEALTH_FAILURE_CONFIRMATIONS :
    // un blip reseau isole ne merite pas d'exiger un acquittement humain,
    // seulement une panne qui dure sur plusieurs cycles.
    static constexpr uint8_t CLOUD_SYNC_FAILURE_CONFIRMATIONS = 3U;

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

    // ── Plafond de securite pour un appel reseau bloque ──────────────────
    //
    // Couvre le pire cas LEGITIME (pas en panne) d'une phase : resolution DNS
    // a froid (~15s, plafond reel documente dans WiFiGenericClass::hostByName,
    // WiFiGeneric.cpp) + connexion TCP (CONNECT_TIMEOUT_MS) + handshake TLS
    // (le meme budget), soit ~15+4+4=23s au pire sans rien d'anormal. Fixe
    // avec une marge au-dela de ce plafond legitime, mais tres en-deca des
    // ~50-60s constates le 28 septembre 2026 lors d'un blocage reel -- voir
    // checkSyncWatchdog() dans CloudSync.cpp.
    static constexpr uint32_t PHASE_HARD_DEADLINE_MS = 25000UL;

private:
    void load();
    void save();
    void saveLastSync(uint32_t epochSec);
    void logBlocked(const char* reason);

    bool startSync(const ConfigManager& configManager);
    static void syncTaskEntry(void* context);
    void performSync();
    // Appelee a chaque tour depuis update(), quel que soit l'etat par
    // ailleurs : seule fonction autorisee a agir sur _watchdog depuis la
    // boucle principale.
    void checkSyncWatchdog();
    void applyPendingResult(uint32_t epochSec);
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
    // Revision que _taskConfigBody decrit (ou revision courante si
    // _taskSendConfig est faux) : recopiee dans _lastSyncedRevision par
    // applyPendingResult() si le cycle reussit, pour que le cycle suivant
    // sache si la configuration a bouge entre-temps.
    uint32_t         _taskRevisionAttempted = 0U;
    bool             _taskSendConfig = true;
    CloudSyncResult  _pendingResult;
    volatile bool    _syncInProgress = false;
    volatile bool    _resultReady    = false;
    // Voir PHASE_HARD_DEADLINE_MS et checkSyncWatchdog().
    CloudSyncWatchdog _watchdog;

    // Derniere revision de configuration effectivement remontee avec succes.
    // Sentinelle a la premiere synchronisation (aucune revision reelle ne
    // vaut UINT32_MAX) : le tout premier cycle apres redemarrage envoie donc
    // toujours la configuration, meme si elle n'a pas change depuis l'arret --
    // ce compteur vit en RAM, pas en NVS, et un redemarrage doit pouvoir
    // rafraichir un miroir serveur qui aurait diverge pendant l'absence.
    uint32_t _lastSyncedRevision = 0xFFFFFFFFUL;

    // Issue du dernier cycle complet -- scaffolding pour /api/adminStatus et
    // FaultId::CLOUD_SYNC (plan en pause le 27 septembre 2026, voir
    // memoire cloudsync-wip-compile-bug.md). Declares ici pour que les
    // accesseurs publics ci-dessus compilent ; RIEN dans CloudSync.cpp ne
    // les ecrit encore -- ils restent a leur valeur par defaut
    // (false/0/0) tant que applyPendingResult() n'est pas complete pour
    // les renseigner. Pas dangereux en l'etat : juste inerte.
    bool     _lastSyncOk = false;
    uint32_t _lastSuccessEpochSec = 0U;
    uint8_t  _consecutiveFailures = 0U;

    ConfigManager*   _configTarget = nullptr;
    ScheduleManager* _scheduleTarget = nullptr;
    const RelaisManager* _reportRelais = nullptr;
    StorageManager*      _reportStorage = nullptr;

    // Accuse en attente d'emission, produit par applyCommand() et transmis
    // au cycle suivant. Le serveur representera la meme commande tant
    // qu'elle n'est pas reglee : la comparaison des correlationId evite de
    // l'appliquer deux fois.
    CloudSyncPendingAck _pendingAck;
    // Reveille le cycle suivant sans attendre l'intervalle complet, pour que
    // l'accuse parte en quelques secondes plutot qu'au bout de 15 minutes.
    bool             _ackSyncSoon = false;
};
