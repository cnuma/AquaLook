#pragma once
#include <Arduino.h>
#include <WiFi.h>

// ═══════════════════════════════════════════════════════════════
//  WiFiManager — machine d'états non bloquante
//
//  États :
//    IDLE            → pas encore initialisé
//    CONNECTING      → tentative connexion STA en cours
//    CONNECTED       → connexion STA établie
//    DISCONNECTED    → STA perdue, tentative de reconnexion
//    CAPTIVE_PORTAL  → mode AP actif, attente configuration
//
//  Transitions :
//    begin()              → CONNECTING
//    CONNECTING (timeout) → DISCONNECTED
//    STA OK               → CONNECTED
//    STA perdue           → DISCONNECTED
//    DISCONNECTED (retry) → CONNECTING
//    captiveRequested     → CAPTIVE_PORTAL (depuis EventBus)
//    stopCaptive()        → CONNECTING (reboot conseillé)
//
//  Invariant I19 : CAPTIVE_PORTAL et CONNECTED sont exclusifs.
// ═══════════════════════════════════════════════════════════════

class WiFiManager {
public:
    enum class State : uint8_t {
        IDLE,
        CONNECTING,
        CONNECTED,
        DISCONNECTED,
        CAPTIVE_PORTAL
    };

    // ── Cycle de vie ──────────────────────────────────────────
    /// ssid/pwd depuis configMgr.wifi() — invariant I9
    void begin(const char* ssid, const char* pwd);

    /// À appeler dans loop() — non bloquant
    void update();

    // ── Portail captif ────────────────────────────────────────
    /// Lance le mode AP + DNS redirect.
    /// Appelé par EventBus::captiveRequested dans update().
    void startCaptivePortal();

    /// Arrête le mode AP, reboot pour reconnexion STA.
    void stopCaptivePortal();

    // ── Scan réseau (non bloquant) ────────────────────────────
    /// Lance un scan asynchrone — retour immédiat.
    /// Résultats disponibles via getScanCount() >= 0 quand scan terminé.
    void startScan();

    /// Résultat d'un réseau scanné
    struct ScanEntry {
        char    ssid[33];   // 32 chars + null
        int8_t  rssi;       // dBm
        bool    secured;    // WPA/WPA2/WEP → true
    };

    /// Nombre de réseaux trouvés lors du dernier scan.
    /// Retourne -1 si scan en cours, 0 si aucun résultat ou scan non lancé.
    int16_t     getScanCount()    const;

    /// Réseau i du dernier scan (i < getScanCount()).
    ScanEntry   getScanEntry(uint8_t i) const;

    /// Libère la mémoire du scan (à appeler après avoir lu tous les résultats).
    /// Obligatoire avant un prochain startScan().
    void        clearScan();

    // ── Getters ───────────────────────────────────────────────
    State       getState()        const { return _state; }
    bool        isConnected()     const { return _state == State::CONNECTED; }
    bool        isCaptivePortal() const { return _state == State::CAPTIVE_PORTAL; }
    IPAddress   getIP()           const { return WiFi.localIP(); }
    IPAddress   getApIP()         const { return WiFi.softAPIP(); }
    const char* getSsid()         const { return _ssid; }
    int8_t      getRssi()         const;  // dBm, 0 si non connecté
    const char* stateStr()        const;

private:
    char    _ssid[64]  = "";
    char    _pwd[64]   = "";
    State   _state     = State::IDLE;

    uint32_t _lastActionMs    = 0;  // timestamp dernière transition
    uint8_t  _retryCount      = 0;
    bool     _scanPending     = false;  // scan asynchrone lancé, résultats non encore lus

    // Constantes de timing (compile-time, pas en flash)
    static constexpr uint32_t CONNECT_TIMEOUT_MS  = 15000;  // 15s avant abandon
    static constexpr uint32_t RETRY_INTERVAL_MS   = 30000;  // 30s entre tentatives
    static constexpr uint8_t  MAX_RETRIES         = 5;      // puis DISCONNECTED stable

    void startConnection();
    void handleConnecting(uint32_t now);
    void handleDisconnected(uint32_t now);
    void handleConnected();
    void handleCaptivePortal();
};
