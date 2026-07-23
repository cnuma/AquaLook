#pragma once

#include <Arduino.h>

struct MaintenanceResult {
    bool valid = false;
    bool success = false;
    uint32_t tlsDurationMs = 0U;
    uint32_t recordedUptimeMs = 0U;
    uint32_t minFreeHeap = 0U;
    char command[24] = "";
    char httpLine[96] = "";
    char detail[128] = "";
};

class MaintenanceResultStore {
public:
    static MaintenanceResult load();
    static bool save(const MaintenanceResult& result);
    static bool clear();
};
