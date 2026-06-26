#include "WiFiManager.h"

void WiFiMgr::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    _state       = CONNECTING;
    _lastAttempt = millis();
    Serial.printf("[WiFi] Connexion à %s...\n", WIFI_SSID);
}

void WiFiMgr::update() {
    switch (_state) {
        case CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                _state    = CONNECTED;
                _attempts = 0;
                Serial.printf("[WiFi] Connecté — IP : %s\n",
                    WiFi.localIP().toString().c_str());
            } else if (millis() - _lastAttempt > WIFI_RETRY_INTERVAL) {
                _attempts++;
                Serial.printf("[WiFi] Tentative %d...\n", _attempts);
                WiFi.reconnect();
                _lastAttempt = millis();
            }
            break;

        case CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                _state = DISCONNECTED;
                Serial.println("[WiFi] Connexion perdue");
            }
            break;

        case DISCONNECTED:
            if (millis() - _lastAttempt > WIFI_RETRY_INTERVAL) {
                Serial.println("[WiFi] Reconnexion...");
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
                _state       = CONNECTING;
                _lastAttempt = millis();
            }
            break;
    }
}

bool WiFiMgr::isConnected() {
    return _state == CONNECTED;
}