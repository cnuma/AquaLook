#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BuildInfo.h"

extern TFT_eSPI* g_tftPtr;

class TJpg_Decoder {
public:
    using OutputCallback = bool (*)(int16_t, int16_t, uint16_t, uint16_t, uint16_t*);

    void setJpgScale(uint8_t) {}
    void setSwapBytes(bool) {}
    void setCallback(OutputCallback) {}

    template <typename FileSystem>
    bool drawFsJpg(int16_t, int16_t, const char*, FileSystem&) {
        if (!g_tftPtr) return false;

        char buildLine[64];
        snprintf(buildLine, sizeof(buildLine), "v%s  b%s  %s",
                 BuildInfo::VERSION,
                 BuildInfo::BUILD_NUMBER,
                 BuildInfo::GIT_SHA);

        g_tftPtr->fillRect(0, 0, 320, 200, TFT_WHITE);
        g_tftPtr->setTextDatum(MC_DATUM);
        g_tftPtr->setTextSize(1);

        g_tftPtr->setTextColor(0x049F, TFT_WHITE);
        g_tftPtr->setFreeFont(&FreeSansBold12pt7b);
        g_tftPtr->drawString(BuildInfo::PRODUCT, 160, 62);

        g_tftPtr->setFreeFont(nullptr);
        g_tftPtr->setTextColor(0x7BEF, TFT_WHITE);
        g_tftPtr->drawString("IRRIGATION CONTROLLER", 160, 96);

        g_tftPtr->setTextColor(0xAD55, TFT_WHITE);
        g_tftPtr->drawString(buildLine, 160, 122);

        g_tftPtr->setTextColor(0x049F, TFT_WHITE);
        g_tftPtr->drawString(BuildInfo::SIGNATURE, 160, 145);

        g_tftPtr->setTextDatum(TL_DATUM);
        delay(BuildInfo::SPLASH_MIN_READ_MS);
        return true;
    }
};

extern TJpg_Decoder TJpgDec;
