#pragma once

#include <Arduino.h>

struct MaintenanceResult {
    bool valid = false;
    bool success = false;
    bool updateAvailable = false;
    bool notificationPending = false;
    uint32_t tlsDurationMs = 0U;
    uint32_t recordedUptimeMs = 0U;
    uint32_t minFreeHeap = 0U;
    uint32_t manifestSize = 0U;
    uint32_t firmwareSize = 0U;
    char command[24] = "";
    char httpLine[96] = "";
    char detail[128] = "";
    char installedVersion[24] = "";
    char availableVersion[24] = "";
    char channel[16] = "";
    char target[16] = "";
    char environment[40] = "";
    char board[32] = "";
    char firmwareUrl[192] = "";
    char sha256[65] = "";
};

class MaintenanceResultStore {
public:
    static MaintenanceResult load();
    static bool save(const MaintenanceResult& result);
    static bool clear();
};
