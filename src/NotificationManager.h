#pragma once

#include <Arduino.h>
#include "IncidentManager.h"

class ConfigManager;

// Arduino-ESP32 2.0.17 ne fournit pas WiFiClientSecure::setBufferSizes().
// NotificationManager.cpp est le seul module qui appelle cette API optionnelle.
// Sur ce core, l'appel est remappe vers setTimeout() afin de conserver la
// compatibilite de compilation sans desactiver la validation TLS.
#if defined(ARDUINO_ARCH_ESP32) && (__INCLUDE_LEVEL__ == 1)
#define setBufferSizes(rxSize, txSize) setTimeout(8)
#endif

struct NotificationConfig {
    bool enabled = false;
    char server[96] = "https://ntfy.sh";
    char topic[96] = "";
    char token[160] = "";
};

struct NotificationStatus {
    bool configured = false;
    bool enabled = false;
    bool workerRunning = false;
    bool testPending = false;
    bool updatePending = false;
    bool webAssetsUpdatePending = false;
    // Persistant jusqu'au deploiement reel, contrairement aux deux champs
    // ci-dessus qui ne durent que jusqu'a l'envoi de LA notification.
    // C'est ce que doivent lire les indicateurs visuels permanents (LED,
    // icone LCD, pastille Web) : une notification deja livree ne signifie
    // pas qu'il n'y a plus rien a deployer.
    bool updateAvailable = false;
    bool webAssetsUpdateAvailable = false;
    uint8_t pendingMask = 0;
    uint8_t pendingZoneEvents = 0;
    uint32_t attempts = 0;
    uint32_t nextAttemptInSec = 0;
    int lastHttpCode = 0;
    char lastResult[48] = "not-started";
};

class NotificationManager {
public:
    enum class WorkType : uint8_t {
        NONE = 0,
        INCIDENT_INITIAL,
        INCIDENT_ESCALATION,
        INCIDENT_RECOVERY,
        MANUAL_TEST,
        ZONE_EVENT,
        UPDATE_AVAILABLE,
        WEB_ASSETS_UPDATE_AVAILABLE,
        // Reglages recus du serveur et appliques -- ou refuses. L'utilisateur
        // doit savoir qu'un arrosage a change sans qu'il touche au module :
        // c'est le seul evenement ou la configuration bouge a distance.
        REMOTE_CONFIG,
        // Message emis par un script de l'utilisateur. Distinct des autres :
        // c'est le seul dont le module ne connait pas le sens -- il transporte
        // un code choisi par l'auteur du script, pas un evenement qu'il aurait
        // lui-meme constate.
        SCRIPT_MESSAGE,
        // Le garde anti-boucle est sorti seul du mode degrade, apres avoir mis
        // a l'epreuve les fonctions qu'il avait suspendues (voir
        // BootLoopGuard.h). Volontairement le SEUL evenement de ce garde qui
        // notifie : il ne peut le faire qu'une fois l'essai confirme, jamais
        // pendant -- sans quoi la notification elle-meme pourrait relancer la
        // boucle qu'elle raconte.
        BOOT_LOOP_RECOVERED
    };

    enum class WorkerResult : uint8_t {
        IDLE = 0,
        RUNNING,
        SUCCESS,
        FAILED,
        START_FAILED
    };

    static void begin();
    static void bindConfig(ConfigManager* config);
    static void update();
    static NotificationConfig config();
    static NotificationStatus status();
    // Raccourci vers les deux champs homonymes de NotificationStatus, sans
    // construire la structure complete (~100 octets et une copie de chaine).
    // Destine aux indicateurs redessines souvent -- le bandeau LCD est
    // repeint a chaque rafraichissement d'ecran, status() y serait paye
    // plusieurs fois par seconde pour deux booleens.
    static bool updateAvailable();
    static bool saveConfig(bool enabled,
                           const char* server,
                           const char* topic,
                           const char* token,
                           bool preserveTokenWhenEmpty);
    static bool requestTest();
    static bool enqueueZoneEvent(uint8_t zone, bool active);
    /** Signale l'arrivee de reglages venus du serveur. detail est repris tel
     *  quel dans le message : c'est deja la phrase que le module renvoie au
     *  serveur en accuse. */
    static bool enqueueRemoteConfig(bool applied, uint8_t champs,
                                    uint32_t revision, const char* detail);
    /** Message demande par un script. Retourne false si les notifications ne
     *  sont pas configurees : le script doit pouvoir savoir que son alerte
     *  n'est PAS partie, plutot que de croire avoir prevenu quelqu'un. */
    static bool enqueueScriptMessage(uint16_t code, const char* scriptName,
                                     const char* extra = nullptr);
    /** Le garde anti-boucle vient de confirmer un essai : demarrages sans
     *  stabilite qui avaient declenche l'episode, repris dans le message
     *  pour orienter l'utilisateur vers une cause probable. A appeler
     *  uniquement une fois l'essai reussi (module deja en fonctionnement
     *  normal). */
    static bool enqueueAutoHeal(uint8_t triggeringSuspectBoots);
    /** true si un serveur de notification est configure et actif. */
    static bool notificationsReady();

private:
    static void loadConfig();
    static bool persistConfig();
    static void supervisorTask(void* parameter);
    static void senderTask(void* parameter);
    static void startSender(WorkType type);
    static void processWorkerResult(uint32_t nowMs);
    static void scheduleNextAttempt(uint32_t nowMs);
    static WorkType nextWork();
    static bool sendCurrentWork();
    static bool validServer(const char* server);
    static bool validTopic(const char* topic);
    static const char* workCode(WorkType type);
    static IncidentNotification incidentNotificationFor(WorkType type);
};
