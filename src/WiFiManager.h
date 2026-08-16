#pragma once
#include <Arduino.h>
#include <WiFi.h>

// ═══════════════════════════════════════════════════════════════
//  WiFiManager — machine d'états non bloquante
//
//  États :
//    IDLE              → pas encore initialisé
//    CONNECTING        → tentative connexion STA en cours
//    CONNECTED         → connexion STA établie
//    DISCONNECTED      → STA perdue, tentative de reconnexion
//    CAPTIVE_STARTING  → initialisation AP en plusieurs étapes
//    CAPTIVE_PORTAL    → mode AP actif, attente configuration
//
//  Invariant : aucun delay() dans le chemin runtime.
//  Les temporisations matérielles sont représentées par une
//  action différée et une deadline.
// ═══════════════════════════════════════════════════════════════

class WiFiManager {
public:
    enum class State : uint8_t {
        IDLE,
        CONNECTING,
        CONNECTED,
        DISCONNECTED,
        CAPTIVE_STARTING,
        CAPTIVE_PORTAL
    };

    void begin(const char* ssid, const char* pwd);
    void update();

    void startCaptivePortal();
    void stopCaptivePortal();

    void startScan();

    struct ScanEntry {
        char    ssid[33];
        int8_t  rssi;
        bool    secured;
    };

    int16_t     getScanCount() const;
    ScanEntry   getScanEntry(uint8_t i) const;
    void        clearScan();

    State       getState()        const { return _state; }
    bool        isConnected()     const { return _state == State::CONNECTED; }
    bool        isCaptivePortal() const { return _state == State::CAPTIVE_PORTAL; }
    IPAddress   getIP()           const { return WiFi.localIP(); }
    IPAddress   getApIP()         const { return WiFi.softAPIP(); }
    const char* getSsid()         const { return _ssid; }
    int8_t      getRssi()         const;
    const char* stateStr()        const;

    // Cible de la sonde de keepalive (voir checkKeepaliveReachable()).
    // Remplie automatiquement avec la passerelle a la premiere connexion
    // reussie si aucune valeur n'est deja enregistree ; modifiable ensuite
    // (IP de test pour valider la detection, ou plus tard un hote cloud)
    // sans jamais etre ecrasee automatiquement une fois definie.
    const char* keepaliveHost() const { return _keepaliveHost; }
    bool setKeepaliveHost(const char* host);

private:
    enum class PendingAction : uint8_t {
        NONE,
        STA_SET_MODE,
        STA_BEGIN,
        AP_SET_MODE,
        AP_FINALIZE,
        RESTART
    };

    char    _ssid[64]  = "";
    char    _pwd[64]   = "";
    State   _state     = State::IDLE;

    uint32_t _lastActionMs = 0;
    uint8_t  _retryCount = 0;
    bool     _scanPending = false;

    // Cible de la sonde de keepalive — voir checkKeepaliveReachable().
    // Remplie automatiquement avec la passerelle a la premiere connexion
    // reussie (voir handleConnecting()) si aucune valeur n'a deja ete
    // enregistree en NVS ; l'utilisateur peut ensuite la modifier (IP de
    // test, ou plus tard un hote/nom cloud) sans jamais etre ecrasee
    // automatiquement une fois definie.
    char     _keepaliveHost[64] = "";

    // Sonde de keepalive — detecte une association "zombie" (wl_status
    // toujours WL_CONNECTED alors que le reseau ne repond plus), qu'un
    // simple sondage de WiFi.status() ne peut pas voir puisque le pilote
    // lui-meme se trompe.
    uint32_t _lastKeepaliveCheckMs = 0;
    uint8_t  _consecutiveKeepaliveFailures = 0;

    PendingAction _pendingAction = PendingAction::NONE;
    uint32_t _pendingDeadlineMs = 0;

    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    static constexpr uint32_t RETRY_INTERVAL_MS = 30000;
    static constexpr uint8_t  MAX_RETRIES = 5;

    static constexpr uint32_t WIFI_DISCONNECT_SETTLE_MS = 100;
    static constexpr uint32_t WIFI_MODE_SETTLE_MS = 50;
    static constexpr uint32_t WIFI_AP_SETTLE_MS = 200;
    static constexpr uint32_t WIFI_RESTART_SETTLE_MS = 200;

    // Sonde de keepalive : voir _lastKeepaliveCheckMs plus haut. Le port 80
    // est le pari le plus sur sur une box/routeur domestique (interface
    // d'administration presque toujours presente dessus) ; a revoir le jour
    // ou la cible est un service cloud (ex. port 443).
    //
    // Intervalle et timeout resserres (etaient 180000/1500) : le cout reel
    // d'une verification plus frequente est quasi nul en fonctionnement
    // normal (connect() local reussit en quelques ms), le blocage de la
    // boucle principale ne survient que pendant une panne reelle — moment
    // ou un delai de boucle est le cadet des soucis. Sonder toutes les 45s
    // ramene la reconnexion forcee de ~9 min a ~2 min15 dans le pire cas.
    static constexpr uint32_t KEEPALIVE_CHECK_INTERVAL_MS = 45000;   // 45 s
    static constexpr uint32_t KEEPALIVE_CHECK_TIMEOUT_MS  = 1000;    // 1 s
    static constexpr uint8_t  KEEPALIVE_FAILURE_THRESHOLD = 3;       // ~2 min15 avant reconnexion forcee
    static constexpr uint16_t KEEPALIVE_CHECK_PORT = 80;

    // Escalade FaultManager si les cycles zombie se repetent — chacun se
    // resout seul en general (~30s), donc un cycle isole ne doit pas
    // declencher d'alerte persistante. Mais une instabilite recurrente
    // (routeur qui redemarre, signal marginal...) merite un signal visible
    // et acquittable (triangle LCD + LED), pas seulement une ligne de log
    // qui defile. Non persiste au reboot (contrairement a l'incident SD) —
    // portee volontairement limitee a la session en cours.
    static constexpr uint8_t  ZOMBIE_ESCALATION_COUNT = 3;
    static constexpr uint32_t ZOMBIE_ESCALATION_WINDOW_MS = 20UL * 60UL * 1000UL;  // 20 min
    uint32_t _zombieEventsMs[ZOMBIE_ESCALATION_COUNT] = {0};
    uint8_t  _zombieEventIdx = 0;
    uint8_t  _zombieEventCount = 0;

    void scheduleAction(PendingAction action, uint32_t deadlineMs);
    bool processPendingAction(uint32_t now);

    void startConnection();
    void handleConnecting(uint32_t now);
    void handleDisconnected(uint32_t now);
    void handleConnected();
    void handleCaptivePortal();
    void checkKeepaliveReachable(uint32_t now);
    void recordZombieEventAndMaybeEscalate(uint32_t now);

    void loadKeepaliveHost();
    void saveKeepaliveHost() const;
};
