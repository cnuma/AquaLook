#pragma once
#include <Arduino.h>
#include <WiFi.h>

// WiFiManager — connexion STA et portail captif non bloquants.
// Le portail effectue un scan AVANT d'ouvrir le SoftAP puis conserve
// les resultats en RAM. Aucun scan radio n'est lance pendant qu'un client
// est connecte au portail captif.
class WiFiManager {
public:
    enum class State : uint8_t {
        IDLE,
        CONNECTING,
        CONNECTED,
        DISCONNECTED,
        CAPTIVE_STARTING,
        CAPTIVE_SCANNING,
        CAPTIVE_PORTAL
    };

    struct ScanEntry {
        char ssid[33];
        int8_t rssi;
        bool secured;
    };

    void begin(const char* ssid, const char* pwd);
    void update();

    void startCaptivePortal();
    void stopCaptivePortal();

    // Compatibilite API Web : le scan est precharge avant l'ouverture AP.
    // Pendant le portail, cette methode ne declenche jamais un scan radio.
    void startScan();
    int16_t getScanCount() const;
    ScanEntry getScanEntry(uint8_t i) const;
    void clearScan();

    State getState() const { return _state; }
    bool isConnected() const { return _state == State::CONNECTED; }
    bool isCaptivePortal() const { return _state == State::CAPTIVE_PORTAL; }
    IPAddress getIP() const { return WiFi.localIP(); }
    IPAddress getApIP() const { return WiFi.softAPIP(); }
    const char* getSsid() const { return _ssid; }
    int8_t getRssi() const;
    const char* stateStr() const;

private:
    enum class PendingAction : uint8_t {
        NONE,
        STA_SET_MODE,
        STA_BEGIN,
        CAPTIVE_SCAN_SET_MODE,
        CAPTIVE_SCAN_START,
        AP_SET_MODE,
        AP_FINALIZE,
        RESTART
    };

    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000U;
    static constexpr uint32_t RETRY_INTERVAL_MS = 30000U;
    static constexpr uint8_t MAX_RETRIES = 5U;
    static constexpr uint32_t WIFI_DISCONNECT_SETTLE_MS = 100U;
    static constexpr uint32_t WIFI_MODE_SETTLE_MS = 50U;
    static constexpr uint32_t WIFI_AP_SETTLE_MS = 200U;
    static constexpr uint32_t WIFI_RESTART_SETTLE_MS = 200U;
    static constexpr uint32_t CAPTIVE_SCAN_MODE_SETTLE_MS = 150U;
    static constexpr uint32_t CAPTIVE_SCAN_TIMEOUT_MS = 15000U;
    static constexpr uint8_t SCAN_CACHE_MAX = 32U;

    char _ssid[64] = "";
    char _pwd[64] = "";
    State _state = State::IDLE;
    uint32_t _lastActionMs = 0U;
    uint8_t _retryCount = 0U;

    PendingAction _pendingAction = PendingAction::NONE;
    uint32_t _pendingDeadlineMs = 0U;

    bool _scanPending = false;
    bool _scanCacheReady = false;
    uint32_t _scanStartedMs = 0U;
    uint8_t _scanCacheCount = 0U;
    ScanEntry _scanCache[SCAN_CACHE_MAX]{};

    void scheduleAction(PendingAction action, uint32_t deadlineMs);
    bool processPendingAction(uint32_t now);
    void startConnection();
    void handleConnecting(uint32_t now);
    void handleDisconnected(uint32_t now);
    void handleConnected();
    void handleCaptivePortal();
    void handleCaptiveScan(uint32_t now);
    void finalizeCaptiveScan(int16_t rawCount, bool timedOut);
};