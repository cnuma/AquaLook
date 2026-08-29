#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BuildInfo.h"
#include "ScreenGeometry.h"

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

        // Largeur et centre repris de ScreenGeometry.h : figes a 320/160,
        // ils ne peignaient que la moitie gauche d'un ecran de 480 px et
        // centraient le texte de travers (constate le 29 aout 2026).
        constexpr int16_t W  = AquaLook::Panel::WIDTH;
        constexpr int16_t CX = AquaLook::Panel::CENTER_X;
        g_tftPtr->fillRect(0, 0, W, 200, TFT_WHITE);
        g_tftPtr->setTextDatum(MC_DATUM);
        g_tftPtr->setTextSize(1);

        g_tftPtr->setTextColor(0x049F, TFT_WHITE);
        g_tftPtr->setFreeFont(&FreeSansBold12pt7b);
        g_tftPtr->drawString(BuildInfo::PRODUCT, CX, 62);

        g_tftPtr->setFreeFont(nullptr);
        g_tftPtr->setTextColor(0x7BEF, TFT_WHITE);
        g_tftPtr->drawString("IRRIGATION CONTROLLER", CX, 96);

        g_tftPtr->setTextColor(0xAD55, TFT_WHITE);
        g_tftPtr->drawString(buildLine, CX, 122);

        g_tftPtr->setTextColor(0x049F, TFT_WHITE);
        g_tftPtr->drawString(BuildInfo::SIGNATURE, CX, 145);

        g_tftPtr->setTextDatum(TL_DATUM);
        delay(BuildInfo::SPLASH_MIN_READ_MS);
        return true;
    }
};

extern TJpg_Decoder TJpgDec;
