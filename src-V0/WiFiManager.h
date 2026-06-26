#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

class WiFiMgr {
public:
    void begin();
    void update();
    bool isConnected();

private:
    enum State { DISCONNECTED, CONNECTING, CONNECTED };
    State    _state       = DISCONNECTED;
    uint32_t _lastAttempt = 0;
    uint8_t  _attempts    = 0;
};