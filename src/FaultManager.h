#pragma once
#include <Arduino.h>

enum class FaultId : uint8_t {
    RELAY_I2C = 0,
    WIFI = 1,
    FILESYSTEM = 2,
    SOFTWARE = 3
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

    // Fonction normale de la LED. Elle est temporairement masquee par
    // la priorite rouge lorsqu'une erreur doit etre signalee.
    static void setNormalColor(bool red, bool green, bool blue);
    static void normalOff();

private:
    static void writeRgb(bool red, bool green, bool blue);
    static void applyNormalColor();

    static uint32_t _activeMask;
    static bool _unacknowledged;
    static bool _started;
    static bool _normalRed;
    static bool _normalGreen;
    static bool _normalBlue;
};
