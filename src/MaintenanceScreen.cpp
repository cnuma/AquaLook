#include "MaintenanceScreen.h"

#include "EventLog.h"
#include "StatusLed.h"

#if AQUALOOK_BOARD_S3
#include <TFT_eSPI.h>
#include "ScreenGeometry.h"
#include "Theme.h"
#endif

namespace {
// Meme violet que le voyant d'etat et le Web (#6633cc).
constexpr uint8_t LED_R = 102U, LED_G = 51U, LED_B = 204U;
constexpr uint32_t REDRAW_MIN_MS = 250UL;
// Duree de lecture d'un echec avant le retour au mode normal. Bornee, et
// uniquement en mode maintenance : aucun relais n'est pilote dans ce mode.
constexpr uint32_t FAIL_HOLD_MS = 4000UL;

bool     g_active = false;
uint8_t  g_totalSteps = 0U;
uint32_t g_lastDrawMs = 0U;

#if AQUALOOK_BOARD_S3
TFT_eSPI g_tft;

constexpr int32_t W = AquaLook::Panel::WIDTH;
constexpr uint16_t BG = Theme::PURPLE;
constexpr uint16_t FG = 0xFFFF;
constexpr uint16_t TRACK = 0x3110;      // violet sombre : piste de la barre
constexpr uint16_t FAIL_FG = Theme::AMBER;
// Bandes repeintes (y, hauteur) : le reste de l'ecran n'est dessine qu'une fois.
constexpr int32_t STEP_Y = 116, STEP_H = 30;
constexpr int32_t BAR_X = 40, BAR_Y = 156, BAR_W = W - 80, BAR_H = 18;
constexpr int32_t INFO_Y = 182, INFO_H = 28;

void text(const char* s, int32_t x, int32_t y, uint8_t datum, const GFXfont* font, uint16_t fg) {
    g_tft.setFreeFont(font);
    g_tft.setTextColor(fg, BG);
    g_tft.setTextDatum(datum);
    g_tft.drawString(s, x, y);
}
#endif
}  // namespace

namespace MaintenanceScreen {

void begin(const char* subtitle, uint8_t nbZones, uint8_t totalSteps) {
    g_totalSteps = totalSteps;
    g_active = true;

    AquaLook::StatusLed::setStatus(LED_R, LED_G, LED_B);
    AquaLook::StatusLed::setZoneCount(nbZones);
    for (uint8_t z = 0U; z < nbZones; ++z) {
        AquaLook::StatusLed::setZone(z, LED_R, LED_G, LED_B);
    }
    AquaLook::StatusLed::commit();

#if AQUALOOK_BOARD_S3
    g_tft.init();
    g_tft.setTextSize(1);
    g_tft.fillScreen(BG);   // seul dessin plein ecran de ce mode
    text("MISE A JOUR DU MODULE", W / 2, 50, MC_DATUM, THEME_FONT_HEADLINE, FG);
    text(subtitle ? subtitle : "", W / 2, 82, MC_DATUM, THEME_FONT_BODY, FG);
    text("Ne pas eteindre le module", W / 2, 240, MC_DATUM, THEME_FONT_TITLE, FG);
#endif
    EventLog::log(LOG_INFO, "[MAJ-ETAPE] debut %s (%u etapes)", subtitle ? subtitle : "",
                  static_cast<unsigned>(totalSteps));
}

bool active() { return g_active; }

void step(uint8_t index, const char* label) {
    if (!g_active) return;
    EventLog::log(LOG_INFO, "[MAJ-ETAPE] %u/%u %s", static_cast<unsigned>(index),
                  static_cast<unsigned>(g_totalSteps), label ? label : "");
#if AQUALOOK_BOARD_S3
    char line[72];
    snprintf(line, sizeof(line), "Etape %u/%u : %s", static_cast<unsigned>(index),
             static_cast<unsigned>(g_totalSteps), label ? label : "");
    g_tft.fillRect(0, STEP_Y, W, STEP_H, BG);
    g_tft.fillRect(0, BAR_Y - 2, W, (INFO_Y + INFO_H) - (BAR_Y - 2), BG);
    text(line, BAR_X, STEP_Y + STEP_H / 2, ML_DATUM, THEME_FONT_BODY, FG);
    g_lastDrawMs = 0U;
#endif
}

void progress(uint32_t done, uint32_t total, bool kilobytes, const char* detail) {
    if (!g_active || total == 0U) return;
    const uint32_t now = millis();
    const bool edge = (done == 0U || done >= total);
    if (!edge && g_lastDrawMs != 0U && (now - g_lastDrawMs) < REDRAW_MIN_MS) return;
    g_lastDrawMs = now ? now : 1U;
#if AQUALOOK_BOARD_S3
    const uint32_t pct = (done >= total) ? 100U : (uint32_t)((uint64_t)done * 100U / total);
    const int32_t fillW = (int32_t)((uint64_t)(BAR_W - 4) * pct / 100U);
    g_tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, FG);
    g_tft.fillRect(BAR_X + 2, BAR_Y + 2, fillW, BAR_H - 4, FG);
    g_tft.fillRect(BAR_X + 2 + fillW, BAR_Y + 2, BAR_W - 4 - fillW, BAR_H - 4, TRACK);

    char line[72];
    if (kilobytes) {
        snprintf(line, sizeof(line), "%lu / %lu Ko (%lu %%)",
                 (unsigned long)(done / 1024U), (unsigned long)(total / 1024U),
                 (unsigned long)pct);
    } else if (detail && detail[0]) {
        snprintf(line, sizeof(line), "%lu / %lu   %s", (unsigned long)done,
                 (unsigned long)total, detail);
    } else {
        snprintf(line, sizeof(line), "%lu / %lu", (unsigned long)done, (unsigned long)total);
    }
    g_tft.fillRect(0, INFO_Y, W, INFO_H, BG);
    text(line, BAR_X, INFO_Y + INFO_H / 2, ML_DATUM, THEME_FONT_BODY, FG);
#else
    (void)kilobytes; (void)detail;
#endif
}

void finish(bool ok, const char* message) {
    if (!g_active) return;
    EventLog::log(ok ? LOG_INFO : LOG_ERROR, "[MAJ-ETAPE] %s %s", ok ? "fin" : "echec",
                  message ? message : "");
#if AQUALOOK_BOARD_S3
    char line[72];
    snprintf(line, sizeof(line), "%s%s", ok ? "" : "Echec : ", message ? message : "");
    g_tft.fillRect(0, STEP_Y, W, STEP_H, BG);
    g_tft.fillRect(0, INFO_Y, W, INFO_H, BG);
    text(line, W / 2, STEP_Y + STEP_H / 2, MC_DATUM, THEME_FONT_TITLE, ok ? FG : FAIL_FG);
    if (!ok) {
        text("Retour au fonctionnement normal, rien n'a change", W / 2,
             INFO_Y + INFO_H / 2, MC_DATUM, THEME_FONT_BODY, FG);
        delay(FAIL_HOLD_MS);
    }
#endif
}

}  // namespace MaintenanceScreen
