#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"
#include "NTPManager.h"
#include "WeatherManager.h"
#include "RelaisManager.h"
#include "ScheduleManager.h"
#include "ConfigManager.h"
#include "ScreenManager.h"
#include "Theme.h"

#define private public
#include "DisplayManager.h"
#undef private

namespace {
uint32_t s_lastButtonFixMs = 0;
uint32_t s_lastDisplayUpdate = UINT32_MAX;

void redrawWideButtonLowerArea(DisplayManager& d, uint8_t zone,
                               uint16_t x, uint16_t btnY,
                               uint16_t visibleH) {
    if (visibleH < 32) return;

    const bool active = d._relais && d._relais->getState(zone);
    const uint16_t bg = active ? Theme::ACTIVE_BG : Theme::SURFACE;
    const uint16_t innerX = x + 5;
    const uint16_t innerW = DisplayManager::PL_BTN_W - 10;

    d._tft.setFreeFont(nullptr);
    d._tft.setTextSize(1);

    if (active) {
        // Effacer toute la partie basse réellement visible du bouton.
        const uint16_t clearLocalY = 64;
        if (visibleH > clearLocalY) {
            d._tft.fillRect(innerX, btnY + clearLocalY,
                            innerW, visibleH - clearLocalY, bg);
        }

        const uint32_t elapsed = d._schedule ? d._schedule->getElapsedMs(zone) : 0;
        char elapsedBuf[16];
        snprintf(elapsedBuf, sizeof(elapsedBuf), "+%02lu:%02lu",
                 elapsed / 60000UL, (elapsed % 60000UL) / 1000UL);
        d._tft.setTextColor(Theme::ON_ACTIVE_TEXT, bg);
        d._tft.setTextDatum(TL_DATUM);
        d._tft.drawString(elapsedBuf, x + 6, btnY + 66);

        // Remonté nettement pour rester visible même lorsque le bouton est tronqué.
        const uint16_t hintY = btnY + visibleH - 24;
        d._tft.setTextColor(Theme::ON_ACTIVE_MUTED, bg);
        d._tft.setTextDatum(TC_DATUM);
        d._tft.drawString("Appuyer pour arreter",
                          x + DisplayManager::PL_BTN_W / 2,
                          hintY);
        d._tft.setTextDatum(TL_DATUM);
        return;
    }

    // Supprimer explicitement le badge météo placé en haut à droite du bouton.
    // Cette zone est indépendante du texte "Prochain" situé à gauche.
    d._tft.fillRect(x + DisplayManager::PL_BTN_W - 48,
                    btnY + 16,
                    42,
                    18,
                    bg);

    // Effacer toute la zone basse du bouton inactif : température, mm de pluie,
    // état pluie et anciens libellés redondants ne doivent plus rester visibles.
    const uint16_t clearLocalY = 50;
    if (visibleH > clearLocalY) {
        d._tft.fillRect(innerX, btnY + clearLocalY,
                        innerW, visibleH - clearLocalY, bg);
    }

    if (d._schedule) {
        const ZoneSchedule zs = d._schedule->getZoneSchedule(zone);
        char modeBuf[28];
        if (zs.mode == 0) {
            snprintf(modeBuf, sizeof(modeBuf), "Jours fixes");
        } else {
            snprintf(modeBuf, sizeof(modeBuf), "Intervalle / %uj",
                     (unsigned)zs.intervalDays);
        }
        d._tft.setTextColor(Theme::MUTED, bg);
        d._tft.setTextDatum(TL_DATUM);
        d._tft.drawString(modeBuf, x + 6, btnY + 56);
    }

    const uint16_t hintY = btnY + visibleH - 20;
    d._tft.setTextColor(Theme::MUTED, bg);
    d._tft.setTextDatum(TC_DATUM);
    d._tft.drawString("Appuyer pour arroser",
                      x + DisplayManager::PL_BTN_W / 2,
                      hintY);
    d._tft.setTextDatum(TL_DATUM);
}
}

void displayButtonFixDraw(DisplayManager& display) {
    if (display._screenMgr.isAsleep() || display._screen != Screen::HOME) return;
    if (display._homeMode != HomeMode::LIST ||
        display._nbZones == 0 || display._nbZones > 2) return;

    const uint32_t now = millis();
    const bool displayChanged = s_lastDisplayUpdate != display._lastUpdate;
    if (!displayChanged && now - s_lastButtonFixMs < 500) return;

    s_lastDisplayUpdate = display._lastUpdate;
    s_lastButtonFixMs = now;

    const uint8_t nbPlan = min(display._nbZones, (uint8_t)4);
    const uint16_t planH = display._planHdrH + nbPlan * display._planZoneH;
    const uint16_t btnY = DisplayManager::PL_PLAN_Y + planH + display._planGap;
    if (btnY >= 240) return;

    uint16_t visibleH = 240 - btnY;
    if (visibleH > DisplayManager::PL_BTN_H) visibleH = DisplayManager::PL_BTN_H;

    for (uint8_t zone = 0; zone < display._nbZones; ++zone) {
        const uint16_t x = zone == 0
            ? DisplayManager::PL_BTN_Z1_X
            : DisplayManager::PL_BTN_Z2_X;
        redrawWideButtonLowerArea(display, zone, x, btnY, visibleH);
    }
}
