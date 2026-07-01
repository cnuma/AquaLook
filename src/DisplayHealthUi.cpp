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

#define private public
#include "DisplayManager.h"
#undef private

#include "DisplayHealthUi.h"
#include "SystemHealth.h"

namespace {
constexpr uint16_t ALERT_X = 266;
constexpr uint16_t ALERT_Y = 3;
constexpr uint16_t ALERT_W = 25;
constexpr uint16_t ALERT_H = 22;
constexpr uint16_t REBOOT_X = 224;
constexpr uint16_t REBOOT_Y = 42;
constexpr uint16_t REBOOT_W = 88;
constexpr uint16_t REBOOT_H = 30;
uint32_t lastHealthTap = 0;
}

void displayHealthHandleTouch(DisplayManager& display) {
    if (!SystemHealth::hasAny()) return;

    uint16_t tx = 0, ty = 0;
    if (!display.getTouchPoint(tx, ty)) return;

    const uint32_t now = millis();
    if (now - lastHealthTap < 600UL) return;

    if (tx >= ALERT_X && tx < ALERT_X + ALERT_W &&
        ty >= ALERT_Y && ty < ALERT_Y + ALERT_H) {
        lastHealthTap = now;
        display._screen = Screen::ADMIN;
        display._adminPage = AdminPage::LOGS;
        display._needsFullRedraw = true;
        display._screenMgr.wakeUp();
        return;
    }

    if (display._screen == Screen::ADMIN &&
        display._adminPage == AdminPage::LOGS &&
        SystemHealth::rebootRecommended() &&
        tx >= REBOOT_X && tx < REBOOT_X + REBOOT_W &&
        ty >= REBOOT_Y && ty < REBOOT_Y + REBOOT_H) {
        lastHealthTap = now;
        delay(120);
        ESP.restart();
    }
}

void displayHealthDraw(DisplayManager& display) {
    if (!SystemHealth::hasAny() || display._splashActive) return;

    TFT_eSPI& tft = display._tft;

    tft.fillTriangle(ALERT_X + 12, ALERT_Y,
                     ALERT_X, ALERT_Y + 21,
                     ALERT_X + 24, ALERT_Y + 21,
                     TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("!", ALERT_X + 12, ALERT_Y + 14);
    tft.setTextDatum(TL_DATUM);

    if (display._screen != Screen::ADMIN ||
        display._adminPage != AdminPage::LOGS) return;

    tft.fillRoundRect(4, 31, 312, 48, 5, TFT_DARKGREY);
    tft.drawRoundRect(4, 31, 312, 48, 5, TFT_RED);

    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(SystemHealth::title(), 10, 36);
    tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    tft.drawString(SystemHealth::adviceLine1(), 10, 50);
    tft.drawString(SystemHealth::adviceLine2(), 10, 63);

    if (SystemHealth::rebootRecommended()) {
        tft.fillRoundRect(REBOOT_X, REBOOT_Y, REBOOT_W, REBOOT_H, 4, TFT_RED);
        tft.drawRoundRect(REBOOT_X, REBOOT_Y, REBOOT_W, REBOOT_H, 4, TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.drawString("Redemarrer", REBOOT_X + REBOOT_W / 2,
                       REBOOT_Y + REBOOT_H / 2);
        tft.setTextDatum(TL_DATUM);
    }
}
