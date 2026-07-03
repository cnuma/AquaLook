#include "WiFiManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include <DNSServer.h>

// DNS server pour le portail captif (redirect tout le trafic UDP/53)
static DNSServer  _dnsServer;
static bool       _dnsStarted = false;

static constexpr uint8_t DNS_PORT = 53;

// ═══════════════════════════════════════════════════════════════
//  Cycle de vie
// ═══════════════════════════════════════════════════════════════

void WiFiManager::begin(const char* ssid, const char* pwd) {
    strlcpy(_ssid, ssid, sizeof(_ssid));
    strlcpy(_pwd,  pwd,  sizeof(_pwd));

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // reconnexion gérée manuellement

    if (_ssid[0] == '\0') {
        EventLog::log(LOG_WARN, "WiFi: pas de SSID — portail captif");
        EventBus::captiveRequested = true;
    } else {
        EventLog::log(LOG_INFO, "WiFi: SSID='%s' len_pwd=%d", _ssid, strlen(_pwd));
        startConnection();
    }
}

// ─────────────────────────────────────────────────────────────
//  update() — à appeler dans loop(), non bloquant
// ─────────────────────────────────────────────────────────────
void WiFiManager::update() {
    uint32_t now = millis();

    // Portail captif demandé depuis l'extérieur (écran ou web)
    // Invariant I19 : on ne démarre le portail que si pas déjà actif
    if (EventBus::captiveRequested && _state != State::CAPTIVE_PORTAL) {
        EventBus::captiveRequested = false;
        startCaptivePortal();
        return;
    }

    switch (_state) {
        case State::CONNECTING:    handleConnecting(now);    break;
        case State::CONNECTED:     handleConnected();        break;
        case State::DISCONNECTED:  handleDisconnected(now);  break;
        case State::CAPTIVE_PORTAL: handleCaptivePortal();   break;
        case State::IDLE:          break;
    }
}

// ═══════════════════════════════════════════════════════════════
//  Handlers d'état (privés)
// ═══════════════════════════════════════════════════════════════

void WiFiManager::handleConnecting(uint32_t now) {
    wl_status_t s = WiFi.status();

    if (s == WL_CONNECTED) {
        WiFi.setSleep(false);

        _state      = State::CONNECTED;
        _retryCount = 0;

        EventLog::log(LOG_INFO,
                    "WiFi: connecte IP=%s RSSI=%ddBm — veille WiFi desactivee",
                    WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());

        EventBus::displayDirty = true;
        return;
}

    // Détecter la cause précise de l'échec — uniquement sur événement, pas en polling
    bool timedOut  = (now - _lastActionMs) > CONNECT_TIMEOUT_MS;
    bool hardFail  = (s == WL_CONNECT_FAILED);
    bool noSsid    = (s == WL_NO_SSID_AVAIL);

    if (hardFail || noSsid || timedOut) {
        // Log détaillé : cause + status driver + retry count
        const char* cause = hardFail ? "mot de passe refuse" :
                            noSsid   ? "SSID introuvable"    :
                                       "timeout 15s";
        EventLog::log(LOG_WARN,
            "WiFi: echec #%d — %s (wl_status=%d) retry dans %ds",
            _retryCount + 1, cause, (int)s, RETRY_INTERVAL_MS / 1000);

        // Sur WL_NO_SSID_AVAIL : le réseau n'est pas visible — utile pour
        // distinguer "mauvais mot de passe" de "routeur éteint"
        if (noSsid) {
            EventLog::log(LOG_WARN, "WiFi: '%s' absent — verif SSID ou portee", _ssid);
        }
        if (hardFail) {
            EventLog::log(LOG_WARN, "WiFi: connexion refusee — verif mot de passe");
        }

        WiFi.disconnect(true);
        _state        = State::DISCONNECTED;
        _lastActionMs = now;
        _retryCount++;
        EventBus::displayDirty = true;
    }
}

void WiFiManager::handleConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        EventLog::log(LOG_WARN, "WiFi: connexion perdue (wl_status=%d)", (int)WiFi.status());
        _state        = State::DISCONNECTED;
        _lastActionMs = millis();
        EventBus::displayDirty = true;
    }
}

void WiFiManager::handleDisconnected(uint32_t now) {
    if ((now - _lastActionMs) >= RETRY_INTERVAL_MS) {
        if (_retryCount >= MAX_RETRIES) {
            if (_retryCount == MAX_RETRIES) {
                EventLog::log(LOG_ERROR,
                    "WiFi: %d echecs consecutifs sur '%s' — portail captif requis",
                    MAX_RETRIES, _ssid);
                _retryCount++;
                EventBus::displayDirty = true;
            }
        } else {
            startConnection();
        }
    }
}

void WiFiManager::handleCaptivePortal() {
    if (_dnsStarted) {
        _dnsServer.processNextRequest();
    }
    // Le WebManager gère les requêtes HTTP en parallèle (AsyncWebServer)
}

// ═══════════════════════════════════════════════════════════════
//  Connexion STA
// ═══════════════════════════════════════════════════════════════

void WiFiManager::startConnection() {
    EventLog::log(LOG_INFO, "WiFi: tentative #%d sur '%s'", _retryCount + 1, _ssid);
    // Reset complet du driver avant chaque tentative — évite les états
    // résiduels (wl_status=6) après un échec ou un changement de mode AP→STA
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(50);
    WiFi.begin(_ssid, _pwd);
    _state        = State::CONNECTING;
    _lastActionMs = millis();
}

// ═══════════════════════════════════════════════════════════════
//  Portail captif
//  Invariant I19 : exclusif de CONNECTED
// ═══════════════════════════════════════════════════════════════

void WiFiManager::startCaptivePortal() {
    EventLog::log(LOG_INFO, "WiFi: demarrage portail captif");

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);

    const char* apSsid = "Arrosage-Setup";
    WiFi.softAP(apSsid);
    delay(200);

    IPAddress apIp = WiFi.softAPIP();
    EventLog::log(LOG_INFO, "WiFi: AP '%s' IP=%s", apSsid, apIp.toString().c_str());

    _dnsServer.start(DNS_PORT, "*", apIp);
    _dnsStarted = true;

    _state = State::CAPTIVE_PORTAL;
    EventBus::displayDirty = true;
}

void WiFiManager::stopCaptivePortal() {
    if (_dnsStarted) {
        _dnsServer.stop();
        _dnsStarted = false;
    }
    WiFi.softAPdisconnect(true);
    EventLog::log(LOG_INFO, "WiFi: portail arrete — reboot");
    delay(200);
    ESP.restart();
}

// ═══════════════════════════════════════════════════════════════
//  Getters
// ═══════════════════════════════════════════════════════════════

int8_t WiFiManager::getRssi() const {
    if (_state != State::CONNECTED) return 0;
    return (int8_t)WiFi.RSSI();
}

const char* WiFiManager::stateStr() const {
    switch (_state) {
        case State::IDLE:           return "IDLE";
        case State::CONNECTING:     return "CONNECTING";
        case State::CONNECTED:      return "CONNECTED";
        case State::DISCONNECTED:   return "DISCONNECTED";
        case State::CAPTIVE_PORTAL: return "CAPTIVE_PORTAL";
        default:                    return "UNKNOWN";
    }
}

// ═══════════════════════════════════════════════════════════════
//  Scan réseau — non bloquant
//
//  WiFi.scanNetworks(async=true) lance le scan et retourne immédiatement.
//  WiFi.scanComplete() retourne :
//    WIFI_SCAN_RUNNING (-1)  : scan en cours
//    WIFI_SCAN_FAILED  (-2)  : échec
//    N >= 0                  : N réseaux trouvés, résultats disponibles
//
//  Contrainte : le scan en mode AP (portail captif) fonctionne sur l'ESP32
//  car il supporte WIFI_AP_STA. En mode STA pur, idem.
//  WiFi.scanDelete() libère la mémoire allouée par le driver — à appeler
//  après avoir lu tous les résultats (fait dans getScanCount() / getScanEntry()).
// ═══════════════════════════════════════════════════════════════

void WiFiManager::startScan() {
    if (_scanPending) return;  // scan déjà en cours, ignorer

    // En mode AP pur, passer temporairement en AP+STA pour pouvoir scanner
    if (_state == State::CAPTIVE_PORTAL) {
        WiFi.mode(WIFI_AP_STA);
    }

    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
    _scanPending = true;
    EventLog::log(LOG_INFO, "WiFi: scan reseau lance");
}

int16_t WiFiManager::getScanCount() const {
    if (!_scanPending) return 0;
    int16_t n = (int16_t)WiFi.scanComplete();
    return (n == WIFI_SCAN_RUNNING) ? -1 : n;
}

WiFiManager::ScanEntry WiFiManager::getScanEntry(uint8_t i) const {
    ScanEntry e;
    e.ssid[0] = '\0';
    e.rssi    = 0;
    e.secured = false;

    int16_t n = (int16_t)WiFi.scanComplete();
    if (n <= 0 || i >= (uint8_t)n) return e;

    String s = WiFi.SSID(i);
    strlcpy(e.ssid, s.c_str(), sizeof(e.ssid));
    e.rssi    = (int8_t)WiFi.RSSI(i);
    e.secured = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    return e;
}

void WiFiManager::clearScan() {
    WiFi.scanDelete();
    _scanPending = false;
    if (_state == State::CAPTIVE_PORTAL) {
        WiFi.mode(WIFI_AP);
    }
}
