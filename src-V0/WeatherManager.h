#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

class WeatherManager {
public:
    void   begin();
    void   update(bool wifiConnected);
    bool   isRainExpected();
    float  getRainMm();
    String getStatusStr();

private:
    bool     _rainExpected = false;
    float    _rainMm       = 0.0;
    uint32_t _lastCheck    = 0;
    bool     _firstDone    = false;

    void fetch();
};