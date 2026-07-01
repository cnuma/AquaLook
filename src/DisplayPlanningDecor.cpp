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

#include "DisplayPlanningDecor.h"

namespace {
uint32_t s_lastDisplayUpdate = UINT32_MAX;
Screen s_lastScreen = Screen::SYSTEM;
HomeMode s_lastMode = HomeMode::GRID4;
uint8_t s_lastGrid4View = 255;

uint16_t tempBg(float tempC) {
    if (tempC < 5.0f)  return 0x11A9;
    if (tempC < 12.0f) return 0x1A4B;
    if (tempC < 20.0f) return 0x2246;
    if (tempC < 27.0f) return 0x4A24;
    return 0x49A4;
}

uint8_t rainBarHeight(float rainMm, uint8_t maxHeight) {
    if (rainMm <= 0.0f || maxHeight == 0) return 0;
    float ratio = rainMm / 20.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    uint8_t h = (uint8_t)(ratio * maxHeight + 0.5f);
    return h == 0 ? 1 : h;
}

void drawWeatherIcon(TFT_eSPI& tft, uint16_t x, uint16_t y,
                     float rainMm, bool valid) {
    if (!valid) {
        tft.setTextSize(1);
        tft.setTextColor(Theme::MUTED, Theme::BG);
        tft.drawString("--", x, y);
        return;
    }

    if (rainMm > 1.0f) {
        tft.fillRoundRect(x, y + 1, 14, 5, 2, Theme::MUTED);
        tft.fillRoundRect(x + 3, y, 10, 5, 2, Theme::MUTED);
        tft.drawFastVLine(x + 3, y + 7, 3, Theme::BLUE);
        tft.drawFastVLine(x + 7, y + 9, 3, Theme::BLUE);
        tft.drawFastVLine(x + 11, y + 7, 3, Theme::BLUE);
    } else {
        tft.fillCircle(x + 7, y + 6, 3, Theme::AMBER);
        tft.drawFastHLine(x, y + 6, 3, Theme::AMBER);
        tft.drawFastHLine(x + 12, y + 6, 3, Theme::AMBER);
        tft.drawFastVLine(x + 7, y, 2, Theme::AMBER);
        tft.drawFastVLine(x + 7, y + 11, 2, Theme::AMBER);
    }
}

void hatchBullet(TFT_eSPI& tft, int16_t x, int16_t y, int16_t r) {
    const uint16_t stripe = Theme::BG;
    const uint16_t outline = Theme::TEXT;

    // Un contour clair différencie immédiatement la pastille intervalle.
    tft.drawCircle(x, y, r, outline);

    if (r <= 2) {
        // Sur les pastilles minuscules, une croix est plus lisible qu'une hachure.
        tft.drawLine(x - 1, y - 1, x + 1, y + 1, stripe);
        tft.drawLine(x - 1, y + 1, x + 1, y - 1, stripe);
        return;
    }

    // Trois diagonales épaissies, suffisamment contrastées sans masquer
    // complètement la couleur d'identité de la zone.
    tft.drawLine(x - r + 1, y,         x,         y + r - 1, stripe);
    tft.drawLine(x - r + 1, y - 1,     x,         y + r - 2, stripe);
    tft.drawLine(x - 1,     y - r + 1, x + r - 1, y + 1,     stripe);
    tft.drawLine(x - 1,     y - r + 2, x + r - 1, y + 2,     stripe);
    if (r >= 4) {
        tft.drawLine(x - r + 2, y + r - 1, x - r + 1, y + r - 2, outline);
        tft.drawLine(x + r - 2, y - r + 1, x + r - 1, y - r + 2, outline);
    }
}

void redrawListWeather(DisplayManager& d) {
    if (!d._config || !d._config->weatherVisualsEnabled()) return;

    TFT_eSPI& tft = d._tft;
    const CfgDisplay& disp = d._config->display();
    const uint16_t labelW = 22;
    const uint16_t dayW = 42;
    const uint16_t planY = 28;

    for (uint8_t col = 0; col < 5; ++col) {
        ForecastDay fd = d._weather ? d._weather->getForecastDay(col) : ForecastDay{};
        if (!fd.valid) continue;

        const int x0 = labelW + col * dayW + 1;
        const int y0 = planY + 11;
        const int h = d._planHdrH > 11 ? d._planHdrH - 12 : 0;
        if (h <= 0) continue;

        tft.fillRect(x0, y0, dayW - 2, h, Theme::BG);

        tft.drawRect(x0 + dayW - 7, y0 + 2, 4, h - 4, Theme::BLUE);
        const uint8_t barH = rainBarHeight(fd.rainMm, h > 6 ? h - 6 : 0);
        if (barH) tft.fillRect(x0 + dayW - 6, y0 + h - 3 - barH, 2, barH, Theme::BLUE);

        const int cx = labelW + col * dayW + dayW / 2;
        if (disp.showWeatherIcon) {
            drawWeatherIcon(tft, cx - 7, planY + 13, fd.rainMm, fd.valid);
        }

        if (disp.showWeatherTemp && fd.tempMax > -50.0f) {
            const uint16_t tempY = planY + (disp.showWeatherIcon ? 28 : 15);
            const int pillW = 14;
            const int pillH = 10;
            const int minX = cx - 17;
            const int maxX = cx - 2;
            char tmin[5], tmax[5];
            snprintf(tmin, sizeof(tmin), "%.0f", fd.tempMin);
            snprintf(tmax, sizeof(tmax), "%.0f", fd.tempMax);
            tft.fillRoundRect(minX, tempY, pillW, pillH, 4, tempBg(fd.tempMin));
            tft.fillRoundRect(maxX, tempY, pillW, pillH, 4, tempBg(fd.tempMax));
            tft.setTextSize(1);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(Theme::TEXT, tempBg(fd.tempMin));
            tft.drawString(tmin, minX + pillW / 2, tempY + 5);
            tft.setTextColor(Theme::TEXT, tempBg(fd.tempMax));
            tft.drawString(tmax, maxX + pillW / 2, tempY + 5);
            tft.setTextDatum(TL_DATUM);
        }
    }
}

void redrawGrid2Weather(DisplayManager& d) {
    if (!d._config || !d._config->weatherVisualsEnabled()) return;

    TFT_eSPI& tft = d._tft;
    const uint16_t destY = 25;
    const uint16_t planW = 64;
    const uint16_t hdrH = 42;
    const uint16_t labelW = 12;
    const uint16_t colW = (planW - labelW) / 2;

    for (uint8_t c = 0; c < 2; ++c) {
        ForecastDay fd = d._weather ? d._weather->getForecastDay(c) : ForecastDay{};
        if (!fd.valid || fd.tempMax <= -50.0f) continue;

        const uint16_t cx = labelW + c * colW;
        const uint16_t boxY = destY + 10;
        const uint16_t boxH = hdrH - 11;
        tft.fillRect(cx + 1, boxY, colW - 2, boxH, Theme::BG);

        tft.drawRect(cx + colW - 6, boxY + 1, 4, boxH - 2, Theme::BLUE);
        const uint8_t barH = rainBarHeight(fd.rainMm, boxH > 4 ? boxH - 4 : 0);
        if (barH) tft.fillRect(cx + colW - 5, boxY + boxH - 2 - barH, 2, barH, Theme::BLUE);

        const uint16_t iconX = cx + 3;
        const uint16_t iconY = destY + 12;
        if (fd.rainMm > 1.0f) {
            tft.fillRoundRect(iconX, iconY + 1, 9, 4, 2, Theme::MUTED);
            tft.drawFastVLine(iconX + 2, iconY + 6, 2, Theme::BLUE);
            tft.drawFastVLine(iconX + 6, iconY + 6, 2, Theme::BLUE);
        } else {
            tft.fillCircle(iconX + 4, iconY + 4, 3, Theme::AMBER);
        }

        char tmin[5], tmax[5];
        snprintf(tmin, sizeof(tmin), "%.0f", fd.tempMin);
        snprintf(tmax, sizeof(tmax), "%.0f", fd.tempMax);
        const uint16_t pillX = cx + 2;
        const uint16_t pillW = colW - 9;
        const uint16_t pillH = 8;
        const uint16_t maxY = destY + 24;
        const uint16_t minY = destY + 33;
        tft.fillRoundRect(pillX, maxY, pillW, pillH, 3, tempBg(fd.tempMax));
        tft.fillRoundRect(pillX, minY, pillW, pillH, 3, tempBg(fd.tempMin));
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(Theme::TEXT, tempBg(fd.tempMax));
        tft.drawString(tmax, pillX + pillW / 2, maxY + pillH / 2);
        tft.setTextColor(Theme::TEXT, tempBg(fd.tempMin));
        tft.drawString(tmin, pillX + pillW / 2, minY + pillH / 2);
        tft.setTextDatum(TL_DATUM);
    }
}

void drawIntervalBulletsList(DisplayManager& d) {
    if (!d._schedule) return;
    const uint8_t nbPlan = min(d._nbZones, (uint8_t)4);
    for (uint8_t z = 0; z < nbPlan; ++z) {
        ZoneSchedule zs = d._schedule->getZoneSchedule(z);
        if (zs.mode == 0) continue;
        const uint16_t rowY = 28 + d._planHdrH + z * d._planZoneH;
        const int16_t r = max(4, min(6, (int)(d._planZoneH / 2 - 1)));
        hatchBullet(d._tft, 11, rowY + d._planZoneH / 2, r);
    }
}

void drawIntervalBulletsGrid2(DisplayManager& d) {
    if (!d._schedule) return;
    const uint16_t destY = 25;
    const uint16_t hdrH = 42;
    const uint16_t zoneAreaH = 215 - hdrH;
    const uint8_t nbPlan = min(d._nbZones, (uint8_t)8);
    uint16_t zoneH = nbPlan > 0 ? zoneAreaH / nbPlan : zoneAreaH;
    if (zoneH < 4) zoneH = 4;

    for (uint8_t z = 0; z < nbPlan; ++z) {
        ZoneSchedule zs = d._schedule->getZoneSchedule(z);
        if (zs.mode == 0) continue;
        const uint16_t rowY = destY + hdrH + z * zoneH;
        const int16_t r = max(3, min(5, (int)(zoneH / 2)));
        hatchBullet(d._tft, 6, rowY + zoneH / 2, r);
    }
}

void drawIntervalBulletsGrid4(DisplayManager& d) {
    if (!d._schedule || d._grid4View > 1) return;
    const uint8_t zStart = d._grid4View == 0 ? 0 : 8;
    const uint8_t zEnd = min(d._nbZones, (uint8_t)(zStart + 8));
    const uint8_t nbZ = zEnd - zStart;
    if (nbZ == 0) return;
    uint16_t zoneH = (198 - 28) / nbZ;
    if (zoneH < 4) zoneH = 4;

    for (uint8_t zi = 0; zi < nbZ; ++zi) {
        const uint8_t z = zStart + zi;
        ZoneSchedule zs = d._schedule->getZoneSchedule(z);
        if (zs.mode == 0) continue;
        const uint16_t rowY = 20 + 28 + zi * zoneH;
        const int16_t r = max(3, min(5, (int)(zoneH / 2)));
        hatchBullet(d._tft, 11, rowY + zoneH / 2, r);
    }
}
}

void displayPlanningDecorDraw(DisplayManager& display) {
    if (display._screenMgr.isAsleep() || display._screen != Screen::HOME) return;

    if (s_lastDisplayUpdate == display._lastUpdate &&
        s_lastScreen == display._screen &&
        s_lastMode == display._homeMode &&
        s_lastGrid4View == display._grid4View) {
        return;
    }

    s_lastDisplayUpdate = display._lastUpdate;
    s_lastScreen = display._screen;
    s_lastMode = display._homeMode;
    s_lastGrid4View = display._grid4View;

    switch (display._homeMode) {
        case HomeMode::LIST:
            redrawListWeather(display);
            drawIntervalBulletsList(display);
            break;
        case HomeMode::GRID2:
            redrawGrid2Weather(display);
            drawIntervalBulletsGrid2(display);
            break;
        case HomeMode::GRID4:
            drawIntervalBulletsGrid4(display);
            break;
    }
}
