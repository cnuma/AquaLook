#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Pointeur deja initialise par DisplayManager::showSplash().
extern TFT_eSPI* g_tftPtr;

// Compatibilite transitoire apres suppression du decodeur JPEG.
// Les anciens appels restent compilables, mais aucun decodeur JPEG n'est lie.
// Si un ancien splash.jpg subsiste encore dans LittleFS, drawFsJpg() affiche
// directement le splash texte afin d'eviter un ecran vide.
class TJpg_Decoder {
public:
    using OutputCallback = bool (*)(int16_t, int16_t, uint16_t, uint16_t, uint16_t*);

    void setJpgScale(uint8_t) {}
    void setSwapBytes(bool) {}
    void setCallback(OutputCallback) {}

    template <typename FileSystem>
    bool drawFsJpg(int16_t, int16_t, const char*, FileSystem&) {
        if (!g_tftPtr) return false;

        g_tftPtr->fillRect(0, 0, 320, 200, TFT_WHITE);
        g_tftPtr->setTextColor(0x049F, TFT_WHITE);
        g_tftPtr->setFreeFont(&FreeSansBold12pt7b);
        g_tftPtr->setTextSize(1);
        g_tftPtr->setTextDatum(MC_DATUM);
        g_tftPtr->drawString("AquaLook", 160, 80);

        g_tftPtr->setFreeFont(nullptr);
        g_tftPtr->setTextColor(0x7BEF, TFT_WHITE);
        g_tftPtr->drawString("IRRIGATION CONTROLLER", 160, 110);
        g_tftPtr->setTextColor(0xAD55, TFT_WHITE);
        g_tftPtr->drawString("ESP32 | Arduino", 160, 126);
        g_tftPtr->setTextDatum(TL_DATUM);
        return true;
    }
};

extern TJpg_Decoder TJpgDec;
