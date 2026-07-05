#pragma once

#include <Arduino.h>

// Compatibilite transitoire apres suppression du decodeur JPEG.
// DisplayManager conserve encore ses anciens appels, mais aucun code de
// decodage n'est lie au firmware. Le splash utilise toujours son fallback texte.
class TJpg_Decoder {
public:
    using OutputCallback = bool (*)(int16_t, int16_t, uint16_t, uint16_t, uint16_t*);

    void setJpgScale(uint8_t) {}
    void setSwapBytes(bool) {}
    void setCallback(OutputCallback) {}

    template <typename FileSystem>
    bool drawFsJpg(int16_t, int16_t, const char*, FileSystem&) {
        return false;
    }
};

extern TJpg_Decoder TJpgDec;
