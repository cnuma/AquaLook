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
#include "MaintenanceResult.h"

#define private public
#include "DisplayManager.h"
#undef private

#include "DisplayPlanningDecor.h"

namespace {
uint32_t s_lastDisplayUpdate = UINT32_MAX;
uint32_t s_lastDecorMs = 0;
Screen s_lastScreen = Screen::SYSTEM;
HomeMode s_lastMode = HomeMode::GRID4;
uint8_t s_lastGrid4View = 255;
bool s_updateStateLoaded = false;
bool s_updateAvailable = false;
char s_availableVersion[24] = "";

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

bool intervalDayIsPlanned(const ZoneSchedule& zs,
                          uint32_t todayEpochDay,
                          uint8_t daysAhead) {
    const uint32_t interval = zs.intervalDays > 0 ? zs.intervalDays : 1;
    const uint32_t anchor   = zs.intervalAnchorDay;
    const uint32_t targetDay = todayEpochDay + daysAhead;

    return anchor > 0 &&
           targetDay >= anchor &&
           ((targetDay - anchor) % interval) == 0;
}

void hatchRect(TFT_eSPI& tft, int16_t x, int16_t y,
               int16_t w, int16_t h) {
    if (w < 4 || h < 4) return;

    const int16_t spacing = 6;
    const int16_t maxX = w - 1;
    const int16_t maxY = h - 1;

    for (int16_t startX = -maxY; startX <= maxX; startX += spacing) {
        const int16_t x1 = startX < 0 ? 0 : startX;
        const int16_t y1 = startX < 0 ? maxY + startX : maxY;
        const int16_t x2 = min(maxX, (int16_t)(startX + maxY));
        const int16_t y2 = maxY - (x2 - startX);

        tft.drawLine(x + x1, y + y1, x + x2, y + y2, Theme::TEXT2);
        if (y1 > 0 && y2 > 0) {
            tft.drawLine(x + x1, y + y1 - 1, x + x2, y + y2 - 1, Theme::TEXT2);
        }
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

void hatchIntervalDaysList(DisplayManager& d) {
    if (!d._schedule || !d._ntp || !d._ntp->isSynced()) return;

    const uint32_t todayEpochDay = d._ntp->getEpochDay();
    const uint8_t nbPlan = min(d._nbZones, (uint8_t)4);

    for (uint8_t z = 0; z < nbPlan; ++z) {
        const ZoneSchedule zs = d._schedule->getZoneSchedule(z);
        if (zs.mode == 0) continue;

        const uint16_t rowY = 28 + d._planHdrH + z * d._planZoneH;
        for (uint8_t col = 0; col < 7; ++col) {
            if (intervalDayIsPlanned(zs, todayEpochDay, col)) continue;
            hatchRect(d._tft, 22 + col * 42 + 2, rowY + 1, 38, d._planZoneH - 2);
        }
    }
}

void hatchIntervalDaysGrid2(DisplayManager& d) {
    if (!d._schedule || !d._ntp || !d._ntp->isSynced()) return;

    const uint16_t labelW = 12;
    const uint16_t colW = (64 - labelW) / 2;
    const uint8_t nbPlan = min(d._nbZones, (uint8_t)8);
    uint16_t zoneH = nbPlan > 0 ? (215 - 42) / nbPlan : 215 - 42;
    if (zoneH < 4) zoneH = 4;

    const uint32_t todayEpochDay = d._ntp->getEpochDay();
    for (uint8_t z = 0; z < nbPlan; ++z) {
        const ZoneSchedule zs = d._schedule->getZoneSchedule(z);
        if (zs.mode == 0) continue;

        const uint16_t rowY = 25 + 42 + z * zoneH;
        for (uint8_t col = 0; col < 2; ++col) {
            if (intervalDayIsPlanned(zs, todayEpochDay, col)) continue;
            hatchRect(d._tft, labelW + col * colW + 1, rowY + 1, colW - 2, zoneH - 2);
        }
    }
}

void hatchIntervalDaysGrid4(DisplayManager& d) {
    if (!d._schedule || !d._ntp || !d._ntp->isSynced() || d._grid4View > 1) return;

    const uint8_t zStart = d._grid4View == 0 ? 0 : 8;
    const uint8_t zEnd = min(d._nbZones, (uint8_t)(zStart + 8));
    const uint8_t nbZ = zEnd - zStart;
    if (nbZ == 0) return;

    uint16_t zoneH = (198 - 28) / nbZ;
    if (zoneH < 4) zoneH = 4;
    const uint16_t dayW = 42;
    const uint32_t todayEpochDay = d._ntp->getEpochDay();

    for (uint8_t zi = 0; zi < nbZ; ++zi) {
        const uint8_t z = zStart + zi;
        const ZoneSchedule zs = d._schedule->getZoneSchedule(z);
        if (zs.mode == 0) continue;

        const uint16_t rowY = 20 + 28 + zi * zoneH;
        for (uint8_t col = 0; col < 7; ++col) {
            if (intervalDayIsPlanned(zs, todayEpochDay, col)) continue;
            hatchRect(d._tft, 22 + col * dayW + 1, rowY + 1, dayW - 2, zoneH - 2);
        }
    }
}

void simplifyWideButtons(DisplayManager& d) {
    // Les cartes LIST 1/2 zones sont entièrement rendues par renderBtnSprite().
    // Aucun nettoyage périodique ne doit être appliqué ici : l'ancienne bande
    // y+65..y+79 coupait le texte "Appuyer pour arroser" après la fin d'un cycle.
    (void)d;
}

void loadUpdateState() {
    const MaintenanceResult result = MaintenanceResultStore::load();
    s_updateAvailable = result.valid &&
                        result.success &&
                        result.updateAvailable &&
                        strcmp(result.command, "check_version") == 0;
    strlcpy(s_availableVersion, result.availableVersion, sizeof(s_availableVersion));
    s_updateStateLoaded = true;
}

void drawUpdateAvailableIcon(DisplayManager& d) {
    if (!s_updateStateLoaded) loadUpdateState();
    if (!s_updateAvailable) return;

    // Bande réservée à gauche du sprite signal (x=300..319).
    // L'icône d'erreur conserve sa propre position et peut rester visible.
    const uint16_t headerH = d._homeMode == HomeMode::GRID4 ? 20U :
                             (d._homeMode == HomeMode::GRID2 ? 25U : 28U);
    const int16_t cx = 285;
    const int16_t cy = headerH / 2;
    const int16_t radius = headerH <= 20 ? 7 : 8;

    d._tft.fillCircle(cx, cy, radius, Theme::BLUE);
    d._tft.drawFastVLine(cx, cy - 4, 7, Theme::TEXT);
    d._tft.drawLine(cx, cy - 5, cx - 3, cy - 2, Theme::TEXT);
    d._tft.drawLine(cx, cy - 5, cx + 3, cy - 2, Theme::TEXT);
    d._tft.drawFastHLine(cx - 4, cy + 4, 9, Theme::TEXT);
}
}

void displayPlanningDecorDraw(DisplayManager& display) {
    if (display._screenMgr.isAsleep() || display._screen != Screen::HOME) return;

    const bool displayChanged =
        s_lastDisplayUpdate != display._lastUpdate ||
        s_lastScreen != display._screen ||
        s_lastMode != display._homeMode ||
        s_lastGrid4View != display._grid4View;

    if (!displayChanged) return;

    s_lastDisplayUpdate = display._lastUpdate;
    s_lastScreen = display._screen;
    s_lastMode = display._homeMode;
    s_lastGrid4View = display._grid4View;

    switch (display._homeMode) {
        case HomeMode::LIST:  redrawListWeather(display);  break;
        case HomeMode::GRID2: redrawGrid2Weather(display); break;
        case HomeMode::GRID4: break;
    }

    switch (display._homeMode) {
        case HomeMode::LIST:
            hatchIntervalDaysList(display);
            simplifyWideButtons(display);
            break;
        case HomeMode::GRID2:
            hatchIntervalDaysGrid2(display);
            break;
        case HomeMode::GRID4:
            hatchIntervalDaysGrid4(display);
            break;
    }

    drawUpdateAvailableIcon(display);
}
