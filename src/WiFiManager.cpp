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
static constexpr uint32_t WIFI_SCAN_MAX_MS_PER_CHAN = 120U;

static uint32_t _scanDiagStartedMs = 0U;
static uint32_t _scanDiagLastMs = 0U;
static uint32_t _scanDiagTicks = 0U;

void WiFiManager::begin(const char* ssid, const char* pwd) {
    strlcpy(_ssid, ssid, sizeof(_ssid));
    strlcpy(_pwd, pwd, sizeof(_pwd));

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

    // Continue a observer le driver meme si le navigateur ne poll plus.
    if (_scanPending &&
        (_scanDiagLastMs == 0U || (now - _scanDiagLastMs) >= 1000U)) {
        _scanDiagLastMs = now;
        _scanDiagTicks++;
        const int16_t raw = static_cast<int16_t>(WiFi.scanComplete());
        EventLog::log(
            raw == WIFI_SCAN_RUNNING ? LOG_INFO : LOG_WARN,
            "WiFi scan diag: runtime tick=%lu elapsed=%lums raw=%d mode=%d status=%d apClients=%u heap=%u",
            (unsigned long)_scanDiagTicks,
            (unsigned long)(now - _scanDiagStartedMs),
            (int)raw,
            (int)WiFi.getMode(),
            (int)WiFi.status(),
            (unsigned)WiFi.softAPgetStationNum(),
            (unsigned)ESP.getFreeHeap()
        );
    }

    if (processPendingAction(now)) {
        return;
    }

    if (EventBus::captiveRequested &&
        _state != State::CAPTIVE_STARTING &&
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
        case State::CAPTIVE_PORTAL:
            handleCaptivePortal();
            break;
        case State::CAPTIVE_STARTING:
        case State::IDLE:
            break;
    }
}

void WiFiManager::scheduleAction(
    PendingAction action,
    uint32_t deadlineMs
) {
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
            scheduleAction(
                PendingAction::STA_BEGIN,
                now + WIFI_MODE_SETTLE_MS
            );
            return true;

        case PendingAction::STA_BEGIN:
            WiFi.begin(_ssid, _pwd);
            _lastActionMs = now;
            return true;

        case PendingAction::AP_SET_MODE:
            // Garder l'interface STA active pendant toute la duree du portail.
            // Un scan peut alors utiliser le STA sans changer le mode radio et
            // sans couper la connexion HTTP du client associe au SoftAP.
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP(CAPTIVE_AP_SSID);
            EventLog::log(
                LOG_INFO,
                "WiFi scan diag: AP_SET_MODE mode=%d status=%d apClients=%u",
                (int)WiFi.getMode(),
                (int)WiFi.status(),
                (unsigned)WiFi.softAPgetStationNum()
            );
            scheduleAction(
                PendingAction::AP_FINALIZE,
                now + WIFI_AP_SETTLE_MS
            );
            return true;

        case PendingAction::AP_FINALIZE: {
            const IPAddress apIp = WiFi.softAPIP();

            EventLog::log(
                LOG_INFO,
                "WiFi: AP '%s' IP=%s",
                CAPTIVE_AP_SSID,
                apIp.toString().c_str()
            );
            EventLog::log(
                LOG_INFO,
                "WiFi scan diag: AP_FINALIZE mode=%d status=%d apClients=%u",
                (int)WiFi.getMode(),
                (int)WiFi.status(),
                (unsigned)WiFi.softAPgetStationNum()
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

    const bool timedOut =
        (now - _lastActionMs) > CONNECT_TIMEOUT_MS;
    const bool hardFail = (s == WL_CONNECT_FAILED);
    const bool noSsid = (s == WL_NO_SSID_AVAIL);

    if (hardFail || noSsid || timedOut) {
        const char* cause =
            hardFail ? "mot de passe refuse" :
            noSsid ? "SSID introuvable" :
            "timeout 15s";

        EventLog::log(
            LOG_WARN,
            "WiFi: echec #%u, %s, wl_status=%d, retry dans %lus",
            _retryCount + 1,
            cause,
            static_cast<int>(s),
            RETRY_INTERVAL_MS / 1000UL
        );

        if (noSsid) {
            EventLog::log(
                LOG_WARN,
                "WiFi: '%s' absent, verifier SSID ou portee",
                _ssid
            );
        }

        if (hardFail) {
            EventLog::log(
                LOG_WARN,
                "WiFi: connexion refusee, verifier le mot de passe"
            );
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
            EventLog::log(
                LOG_ERROR,
                "WiFi: %u echecs consecutifs sur '%s'",
                MAX_RETRIES,
                _ssid
            );
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

void WiFiManager::startConnection() {
    EventLog::log(
        LOG_INFO,
        "WiFi: tentative #%u sur '%s'",
        _retryCount + 1,
        _ssid
    );

    WiFi.disconnect(true);
    _state = State::CONNECTING;
    scheduleAction(
        PendingAction::STA_SET_MODE,
        millis() + WIFI_DISCONNECT_SETTLE_MS
    );
}

void WiFiManager::startCaptivePortal() {
    EventLog::log(LOG_INFO, "WiFi: demarrage portail captif");

    if (_dnsStarted) {
        _dnsServer.stop();
        _dnsStarted = false;
    }

    WiFi.disconnect(true);
    _state = State::CAPTIVE_STARTING;
    scheduleAction(
        PendingAction::AP_SET_MODE,
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
    scheduleAction(
        PendingAction::RESTART,
        millis() + WIFI_RESTART_SETTLE_MS
    );
}

int8_t WiFiManager::getRssi() const {
    if (_state != State::CONNECTED) return 0;
    return static_cast<int8_t>(WiFi.RSSI());
}

const char* WiFiManager::stateStr() const {
    switch (_state) {
        case State::IDLE:
            return "IDLE";
        case State::CONNECTING:
            return "CONNECTING";
        case State::CONNECTED:
            return "CONNECTED";
        case State::DISCONNECTED:
            return "DISCONNECTED";
        case State::CAPTIVE_STARTING:
            return "CAPTIVE_STARTING";
        case State::CAPTIVE_PORTAL:
            return "CAPTIVE_PORTAL";
        default:
            return "UNKNOWN";
    }
}

void WiFiManager::startScan() {
    if (_scanPending) {
        EventLog::log(
            LOG_WARN,
            "WiFi scan diag: start ignore pending=yes complete=%d mode=%d status=%d apClients=%u",
            (int)WiFi.scanComplete(),
            (int)WiFi.getMode(),
            (int)WiFi.status(),
            (unsigned)WiFi.softAPgetStationNum()
        );
        return;
    }

    EventLog::log(
        LOG_INFO,
        "WiFi scan diag: start avant complete=%d mode=%d status=%d apClients=%u heap=%u",
        (int)WiFi.scanComplete(),
        (int)WiFi.getMode(),
        (int)WiFi.status(),
        (unsigned)WiFi.softAPgetStationNum(),
        (unsigned)ESP.getFreeHeap()
    );

    // Balayage actif court : le STA quitte le canal du SoftAP pendant le scan.
    // Limiter le temps par canal evite de laisser le navigateur captif sans
    // reponse assez longtemps pour qu'il abandonne son polling HTTP.
    const int16_t launchResult = static_cast<int16_t>(
        WiFi.scanNetworks(true, false, false, WIFI_SCAN_MAX_MS_PER_CHAN)
    );
    _scanPending = true;
    _scanDiagStartedMs = millis();
    _scanDiagLastMs = 0U;
    _scanDiagTicks = 0U;
    EventLog::log(
        LOG_INFO,
        "WiFi scan diag: lance result=%d complete=%d maxMsChan=%lu mode=%d status=%d apClients=%u heap=%u",
        (int)launchResult,
        (int)WiFi.scanComplete(),
        (unsigned long)WIFI_SCAN_MAX_MS_PER_CHAN,
        (int)WiFi.getMode(),
        (int)WiFi.status(),
        (unsigned)WiFi.softAPgetStationNum(),
        (unsigned)ESP.getFreeHeap()
    );
}

int16_t WiFiManager::getScanCount() const {
    if (!_scanPending) {
        EventLog::log(
            LOG_INFO,
            "WiFi scan diag: poll pending=no mode=%d status=%d apClients=%u",
            (int)WiFi.getMode(),
            (int)WiFi.status(),
            (unsigned)WiFi.softAPgetStationNum()
        );
        return 0;
    }

    const int16_t raw = static_cast<int16_t>(WiFi.scanComplete());
    const int16_t result = raw == WIFI_SCAN_RUNNING ? -1 : raw;
    EventLog::log(
        raw == WIFI_SCAN_FAILED ? LOG_ERROR : LOG_INFO,
        "WiFi scan diag: poll pending=yes raw=%d result=%d mode=%d status=%d apClients=%u heap=%u",
        (int)raw,
        (int)result,
        (int)WiFi.getMode(),
        (int)WiFi.status(),
        (unsigned)WiFi.softAPgetStationNum(),
        (unsigned)ESP.getFreeHeap()
    );
    return result;
}

WiFiManager::ScanEntry
WiFiManager::getScanEntry(uint8_t i) const {
    ScanEntry e;
    e.ssid[0] = '\0';
    e.rssi = 0;
    e.secured = false;

    const int16_t n = static_cast<int16_t>(WiFi.scanComplete());
    if (n <= 0 || i >= static_cast<uint8_t>(n)) {
        EventLog::log(
            LOG_WARN,
            "WiFi scan diag: entree invalide i=%u count=%d",
            (unsigned)i,
            (int)n
        );
        return e;
    }

    const String s = WiFi.SSID(i);
    strlcpy(e.ssid, s.c_str(), sizeof(e.ssid));
    e.rssi = static_cast<int8_t>(WiFi.RSSI(i));
    e.secured =
        WiFi.encryptionType(i) != WIFI_AUTH_OPEN;

    EventLog::log(
        LOG_INFO,
        "WiFi scan diag: entree i=%u/%d ssid='%s' rssi=%d secured=%s",
        (unsigned)i,
        (int)n,
        e.ssid,
        (int)e.rssi,
        e.secured ? "yes" : "no"
    );
    return e;
}

void WiFiManager::clearScan() {
    EventLog::log(
        LOG_INFO,
        "WiFi scan diag: clear avant complete=%d mode=%d status=%d apClients=%u heap=%u",
        (int)WiFi.scanComplete(),
        (int)WiFi.getMode(),
        (int)WiFi.status(),
        (unsigned)WiFi.softAPgetStationNum(),
        (unsigned)ESP.getFreeHeap()
    );
    WiFi.scanDelete();
    _scanPending = false;
    _scanDiagStartedMs = 0U;
    _scanDiagLastMs = 0U;
    _scanDiagTicks = 0U;
    EventLog::log(
        LOG_INFO,
        "WiFi scan diag: clear apres complete=%d mode=%d status=%d apClients=%u heap=%u",
        (int)WiFi.scanComplete(),
        (int)WiFi.getMode(),
        (int)WiFi.status(),
        (unsigned)WiFi.softAPgetStationNum(),
        (unsigned)ESP.getFreeHeap()
    );
    // Rester en AP+STA : repasser en WIFI_AP couperait de nouveau le client.
}
