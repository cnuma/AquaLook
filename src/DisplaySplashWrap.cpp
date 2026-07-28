#include <Arduino.h>
#include <cstring>

#include "OtaBuildIdentity.h"
#include "Theme.h"

#define private public
#include "DisplayManager.h"
#undef private

namespace {

bool containsIgnoreCase(const char* text, const char* token) {
    if (!text || !token || !token[0]) return false;
    String haystack(text);
    String needle(token);
    haystack.toLowerCase();
    needle.toLowerCase();
    return haystack.indexOf(needle) >= 0;
}

bool isDegradedStep(const char* label) {
    return containsIgnoreCase(label, "indisponible") ||
           containsIgnoreCase(label, "echec") ||
           containsIgnoreCase(label, "fallback") ||
           containsIgnoreCase(label, "degrade");
}

void drawIdentityOverlay(DisplayManager& display, uint8_t step, const char* label) {
    TFT_eSPI& tft = display._tft;
    constexpr int16_t overlayY = 154;
    constexpr int16_t overlayH = 45;

    const bool degraded = isDegradedStep(label);
    const uint16_t accent = degraded ? TFT_ORANGE : Theme::SPLASH_ACCENT;

    tft.fillRect(0, overlayY, 320, overlayH, TFT_WHITE);
    tft.drawFastHLine(0, overlayY, 320, Theme::SPLASH_TRACK);

    tft.setFreeFont(nullptr);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(Theme::SPLASH_MUTED, TFT_WHITE);

    char productLine[48];
    snprintf(
        productLine,
        sizeof(productLine),
        "%s %s",
        OtaBuildIdentity::PRODUCT,
        OtaBuildIdentity::VERSION
    );
    tft.drawString(productLine, 160, overlayY + 5);

    char buildLine[80];
    snprintf(
        buildLine,
        sizeof(buildLine),
        "%s | build %s | %s",
        OtaBuildIdentity::OTA_TARGET,
        OtaBuildIdentity::BUILD_NUMBER,
        OtaBuildIdentity::GIT_SHA
    );
    tft.setTextColor(accent, TFT_WHITE);
    tft.drawString(buildLine, 160, overlayY + 18);

    char stateLine[72];
    snprintf(
        stateLine,
        sizeof(stateLine),
        "%s %u/%u - %s",
        degraded ? "DEGRADE" : "BOOT",
        static_cast<unsigned>(step + 1U),
        static_cast<unsigned>(DisplayManager::SPLASH_STEPS),
        label ? label : "Initialisation"
    );
    tft.setTextColor(degraded ? TFT_RED : Theme::SPLASH_MUTED2, TFT_WHITE);
    tft.drawString(stateLine, 160, overlayY + 31);
    tft.setTextDatum(TL_DATUM);
}

}  // namespace

// Le linker redirige le symbole C++ de DisplayManager::showSplash vers ce
// wrapper. La méthode originale est toujours appelée avant l'overlay.
extern "C" void __real__ZN14DisplayManager10showSplashEhPKc(
    DisplayManager* display,
    uint8_t step,
    const char* label
);

extern "C" void __wrap__ZN14DisplayManager10showSplashEhPKc(
    DisplayManager* display,
    uint8_t step,
    const char* label
) {
    __real__ZN14DisplayManager10showSplashEhPKc(display, step, label);
    if (display) drawIdentityOverlay(*display, step, label);
}
