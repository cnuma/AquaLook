#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

class NTPManager {
public:
    void   begin();
    void   update();
    bool   isSynced();
    String getTimeStr();
    int    getHour();
    int    getMinute();

private:
    bool     _synced   = false;
    uint32_t _lastSync = 0;
};