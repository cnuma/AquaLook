#pragma once
#include <Arduino.h>

enum class FaultId : uint8_t {
    RELAY_I2C = 0,
    WIFI = 1,
    FILESYSTEM = 2,
    SOFTWARE = 3,
    STORAGE_SD = 4
};

class FaultManager {
public:
    static void begin();
    static void update();

    static void setActive(FaultId id, bool active);
    static void notifyError();
    static void acknowledge();

    static bool hasActiveFaults();
    static bool hasUnacknowledgedErrors();
    static bool isAcknowledged();
    static uint32_t activeMask();

    static void resolveColor(uint8_t normalRed,
                             uint8_t normalGreen,
                             uint8_t normalBlue,
                             uint8_t& outRed,
                             uint8_t& outGreen,
                             uint8_t& outBlue);

private:
    static uint32_t _activeMask;
    static bool _unacknowledged;
    static bool _started;
};
