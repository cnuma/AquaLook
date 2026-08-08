#include "WiFiManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"
#include "TimeUtils.h"
#include <DNSServer.h>

static DNSServer _dnsServer;
static bool _dnsStarted = false;
static constexpr uint8_t DNS_PORT = 53;
static constexpr const char* CAPTIVE_AP_SSID = "Arrosage-Setup";

void WiFiManager::begin(const char* ssid, const char* pwd) {
    strlcpy(_ssid, ssid ? ssid : "", sizeof(_ssid));
    strlcpy(_pwd, pwd ? pwd : "", sizeof(_pwd));

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);

    if (_ssid[0] == '\0') {
        EventLog::log(LOG_WARN, "WiFi: pas de SSID, portail captif");
        EventBus::captiveRequested = true;
    } else {
        EventLog::log(
            LOG_INFO,
            "WiFi: SSID='%s', mot de passe present=%s",
            _ssid,
            strlen(_pwd) > 0 ? "oui" : "non"
        );
        startConnection();
    }
}

void WiFiManager::update() {
    const uint32_t now = millis();

    if (processPendingAction(now)) return;

    if (EventBus::captiveRequested &&
        _state != State::CAPTIVE_STARTING &&
        _state != State::CAPTIVE_SCANNING &&
        _state != State::CAPTIVE_PORTAL) {
        EventBus::captiveRequested = false;
        startCaptivePortal();
        return;
    }

    switch (_state) {
        case State::CONNECTING:
            handleConnecting(now);
            break;
        case State::CONNECTED:
            handleConnected();
            break;
        case State::DISCONNECTED:
            handleDisconnected(now);
            break;
        case State::CAPTIVE_SCANNING:
            handleCaptiveScan(now);
            break;
        case State::CAPTIVE_PORTAL:
            handleCaptivePortal();
            break;
        case State::CAPTIVE_STARTING:
        case State::IDLE:
            break;
    }
}

void WiFiManager::scheduleAction(PendingAction action, uint32_t deadlineMs) {
    _pendingAction = action;
    _pendingDeadlineMs = deadlineMs;
}

bool WiFiManager::processPendingAction(uint32_t now) {
    if (_pendingAction == PendingAction::NONE) return false;
    if (!AquaLook::Time::deadlineReached(now, _pendingDeadlineMs)) return true;

    const PendingAction action = _pendingAction;
    _pendingAction = PendingAction::NONE;

    switch (action) {
        case PendingAction::STA_SET_MODE:
            WiFi.mode(WIFI_STA);
            scheduleAction(PendingAction::STA_BEGIN, now + WIFI_MODE_SETTLE_MS);
            return true;

        case PendingAction::STA_BEGIN:
            WiFi.begin(_ssid, _pwd);
            _lastActionMs = now;
            return true;

        case PendingAction::CAPTIVE_SCAN_START: {
            WiFi.mode(WIFI_STA);
            WiFi.scanDelete();
            _scanCacheCount = 0U;
            _scanCacheReady = false;

            const int16_t launch = static_cast<int16_t>(WiFi.scanNetworks(true, false));
            _scanStartedMs = now;
            _scanPending = launch == WIFI_SCAN_RUNNING;
            _state = State::CAPTIVE_SCANNING;

            EventLog::log(
                launch == WIFI_SCAN_FAILED ? LOG_WARN : LOG_INFO,
                "WiFi: prescan portail lance result=%d",
                static_cast<int>(launch)
            );

            if (launch >= 0) {
                finalizeCaptiveScan(launch, false);
            } else if (launch == WIFI_SCAN_FAILED) {
                finalizeCaptiveScan(0, false);
            }
            return true;
        }

        case PendingAction::AP_SET_MODE:
            WiFi.mode(WIFI_AP);
            WiFi.softAP(CAPTIVE_AP_SSID);
            scheduleAction(PendingAction::AP_FINALIZE, now + WIFI_AP_SETTLE_MS);
            return true;

        case PendingAction::AP_FINALIZE: {
            const IPAddress apIp = WiFi.softAPIP();
            EventLog::log(
                LOG_INFO,
                "WiFi: AP '%s' IP=%s cacheScan=%u",
                CAPTIVE_AP_SSID,
                apIp.toString().c_str(),
                static_cast<unsigned>(_scanCacheCount)
            );
            _dnsServer.start(DNS_PORT, "*", apIp);
            _dnsStarted = true;
            _state = State::CAPTIVE_PORTAL;
            EventBus::displayDirty = true;
            return true;
        }

        case PendingAction::RESTART:
            ESP.restart();
            return true;

        case PendingAction::NONE:
            return false;
    }

    return false;
}

void WiFiManager::handleConnecting(uint32_t now) {
    const wl_status_t s = WiFi.status();

    if (s == WL_CONNECTED) {
        WiFi.setSleep(false);
        _state = State::CONNECTED;
        _retryCount = 0;
        FaultManager::setActive(FaultId::WIFI, false);
        EventLog::log(
            LOG_INFO,
            "WiFi: connecte IP=%s RSSI=%ddBm, veille desactivee",
            WiFi.localIP().toString().c_str(),
            WiFi.RSSI()
        );
        EventBus::displayDirty = true;
        return;
    }

    const bool timedOut = (now - _lastActionMs) > CONNECT_TIMEOUT_MS;
    const bool hardFail = s == WL_CONNECT_FAILED;
    const bool noSsid = s == WL_NO_SSID_AVAIL;

    if (hardFail || noSsid || timedOut) {
        const char* cause = hardFail ? "mot de passe refuse" :
                            noSsid ? "SSID introuvable" : "timeout 15s";
        EventLog::log(
            LOG_WARN,
            "WiFi: echec #%u, %s, wl_status=%d, retry dans %lus",
            _retryCount + 1,
            cause,
            static_cast<int>(s),
            RETRY_INTERVAL_MS / 1000UL
        );
        if (noSsid) {
            EventLog::log(LOG_WARN, "WiFi: '%s' absent, verifier SSID ou portee", _ssid);
        }
        if (hardFail) {
            EventLog::log(LOG_WARN, "WiFi: connexion refusee, verifier le mot de passe");
        }

        WiFi.disconnect(true);
        _state = State::DISCONNECTED;
        _lastActionMs = now;
        _retryCount++;
        EventBus::displayDirty = true;
    }
}

void WiFiManager::handleConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        EventLog::log(
            LOG_WARN,
            "WiFi: connexion perdue, wl_status=%d",
            static_cast<int>(WiFi.status())
        );
        _state = State::DISCONNECTED;
        _lastActionMs = millis();
        EventBus::displayDirty = true;
    }
}

void WiFiManager::handleDisconnected(uint32_t now) {
    if ((now - _lastActionMs) < RETRY_INTERVAL_MS) return;

    if (_retryCount >= MAX_RETRIES) {
        if (_retryCount == MAX_RETRIES) {
            FaultManager::setActive(FaultId::WIFI, true);
            EventLog::log(LOG_ERROR, "WiFi: %u echecs consecutifs sur '%s'", MAX_RETRIES, _ssid);
            _retryCount++;
            EventBus::displayDirty = true;
        }
        return;
    }

    startConnection();
}

void WiFiManager::handleCaptivePortal() {
    if (_dnsStarted) _dnsServer.processNextRequest();
}

void WiFiManager::handleCaptiveScan(uint32_t now) {
    const int16_t raw = static_cast<int16_t>(WiFi.scanComplete());
    if (raw == WIFI_SCAN_RUNNING) {
        if ((now - _scanStartedMs) >= CAPTIVE_SCAN_TIMEOUT_MS) {
            finalizeCaptiveScan(0, true);
        }
        return;
    }

    if (raw == WIFI_SCAN_FAILED) {
        finalizeCaptiveScan(0, false);
        return;
    }

    finalizeCaptiveScan(raw, false);
}

void WiFiManager::finalizeCaptiveScan(int16_t rawCount, bool timedOut) {
    const int16_t usable = rawCount > 0 ? rawCount : 0;
    _scanCacheCount = 0U;

    for (int16_t i = 0; i < usable && _scanCacheCount < SCAN_CACHE_MAX; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;

        ScanEntry& e = _scanCache[_scanCacheCount++];
        strlcpy(e.ssid, ssid.c_str(), sizeof(e.ssid));
        e.rssi = static_cast<int8_t>(WiFi.RSSI(i));
        e.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }

    WiFi.scanDelete();
    _scanPending = false;
    _scanCacheReady = true;

    EventLog::log(
        timedOut ? LOG_WARN : LOG_INFO,
        timedOut
            ? "WiFi: prescan portail timeout, cache vide"
            : "WiFi: prescan portail termine brut=%d cache=%u",
        static_cast<int>(rawCount),
        static_cast<unsigned>(_scanCacheCount)
    );

    _state = State::CAPTIVE_STARTING;
    scheduleAction(PendingAction::AP_SET_MODE, millis() + WIFI_MODE_SETTLE_MS);
}

void WiFiManager::startConnection() {
    EventLog::log(LOG_INFO, "WiFi: tentative #%u sur '%s'", _retryCount + 1, _ssid);
    WiFi.disconnect(true);
    _state = State::CONNECTING;
    scheduleAction(PendingAction::STA_SET_MODE, millis() + WIFI_DISCONNECT_SETTLE_MS);
}

void WiFiManager::startCaptivePortal() {
    EventLog::log(LOG_INFO, "WiFi: preparation portail captif, prescan avant AP");

    if (_dnsStarted) {
        _dnsServer.stop();
        _dnsStarted = false;
    }

    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    _scanPending = false;
    _scanCacheReady = false;
    _scanCacheCount = 0U;
    _state = State::CAPTIVE_STARTING;
    scheduleAction(
        PendingAction::CAPTIVE_SCAN_START,
        millis() + WIFI_DISCONNECT_SETTLE_MS
    );
}

void WiFiManager::stopCaptivePortal() {
    if (_dnsStarted) {
        _dnsServer.stop();
        _dnsStarted = false;
    }

    WiFi.softAPdisconnect(true);
    EventLog::log(LOG_INFO, "WiFi: portail arrete, reboot programme");
    _state = State::IDLE;
    scheduleAction(PendingAction::RESTART, millis() + WIFI_RESTART_SETTLE_MS);
}

void WiFiManager::startScan() {
    // Le cache a ete construit AVANT l'ouverture du SoftAP. Ne jamais lancer
    // un nouveau balayage radio ici : il ferait perdre la connexion du client.
    if (_scanCacheReady) return;

    EventLog::log(
        LOG_WARN,
        "WiFi: demande scan portail sans cache; aucun scan live declenche"
    );
}

int16_t WiFiManager::getScanCount() const {
    if (_state == State::CAPTIVE_SCANNING || _scanPending) return -1;
    if (!_scanCacheReady) return 0;

    // WebManager utilise historiquement 0 pour signifier 'aucun scan lance'.
    // Une entree virtuelle vide permet de signaler un cache valide mais vide;
    // getScanEntry(0) retournera alors une entree vide et la reponse JSON finale
    // contiendra networks:[] avec scanning:false.
    return _scanCacheCount > 0U ? static_cast<int16_t>(_scanCacheCount) : 1;
}

WiFiManager::ScanEntry WiFiManager::getScanEntry(uint8_t i) const {
    ScanEntry empty{};
    empty.ssid[0] = '\0';
    empty.rssi = 0;
    empty.secured = false;

    if (!_scanCacheReady || i >= _scanCacheCount) return empty;
    return _scanCache[i];
}

void WiFiManager::clearScan() {
    // Compatibilite WebManager : conserver le cache pour tous les clics
    // suivants. La memoire est statique et sera remplacee au prochain portail.
}

int8_t WiFiManager::getRssi() const {
    if (_state != State::CONNECTED) return 0;
    return static_cast<int8_t>(WiFi.RSSI());
}

const char* WiFiManager::stateStr() const {
    switch (_state) {
        case State::IDLE: return "IDLE";
        case State::CONNECTING: return "CONNECTING";
        case State::CONNECTED: return "CONNECTED";
        case State::DISCONNECTED: return "DISCONNECTED";
        case State::CAPTIVE_STARTING: return "CAPTIVE_STARTING";
        case State::CAPTIVE_SCANNING: return "CAPTIVE_SCANNING";
        case State::CAPTIVE_PORTAL: return "CAPTIVE_PORTAL";
        default: return "UNKNOWN";
    }
}
