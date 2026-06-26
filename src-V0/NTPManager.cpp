#include "NTPManager.h"

void NTPManager::begin() {
    configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER1, NTP_SERVER2);
    Serial.println("[NTP] Synchronisation...");
}

void NTPManager::update() {
    if (_synced && millis() - _lastSync < NTP_SYNC_INTERVAL) return;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 100)) {
        _synced   = true;
        _lastSync = millis();
        Serial.printf("[NTP] Heure : %s\n", getTimeStr().c_str());
    }
}

bool NTPManager::isSynced() {
    return _synced;
}

String NTPManager::getTimeStr() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return "??:??:??";
    char buf[32];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
    return String(buf);
}

int NTPManager::getHour() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return -1;
    return timeinfo.tm_hour;
}

int NTPManager::getMinute() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return -1;
    return timeinfo.tm_min;
}