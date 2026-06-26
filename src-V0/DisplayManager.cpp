#include "DisplayManager.h"

// ── Couleurs ───────────────────────────────────
#define C_BG        0x0841   // #0a0f0d
#define C_SURFACE   0x10C2   // #111a15
#define C_GREEN     0x4F8A   // #4ade80
#define C_GREEN_DIM 0x1923   // #1a4a2e
#define C_BLUE      0x1DDF   // #38bdf8
#define C_AMBER     0xFD44   // #fbbf24
#define C_RED       0xF8CE   // #f87171
#define C_MUTED     0x2C4A   // #5a7a65
#define C_TEXT      0xE3EC   // #e2f0e8
#define C_BORDER    0x1803   // #1e3028

void DisplayManager::begin(NTPManager* ntp, WeatherManager* weather,
                            RelaisManager* relais, ScheduleManager* schedule) {
    _ntp      = ntp;
    _weather  = weather;
    _relais   = relais;
    _schedule = schedule;

    _tft.init();
    Serial.printf("[Display] Width: %d Height: %d\n", 
    _tft.width(), _tft.height());
    _tft.setRotation(1);  // paysage correct
    
    _tft.fillScreen(C_BG);
    _tft.setTextDatum(TL_DATUM);

    Serial.println("[Display] Initialisé");
    _needsRedraw = true;
}

void DisplayManager::update() {
    // ── Touch toutes les 100ms ──
    if (millis() - _lastTouch > 100) {
        handleTouch();
        _lastTouch = millis();
    }

    // ── Redraw toutes les 5s ou si forcé ──
    if (_needsRedraw || millis() - _lastDraw > 5000) {
        switch (_currentScreen) {
            case SCREEN_HOME:   drawHome();   break;
            case SCREEN_ZONE:   drawZone(_selectedZone); break;
            case SCREEN_METEO:  drawMeteo();  break;
            case SCREEN_STATUS: drawStatus(); break;
        }
        _needsRedraw = false;
        _lastDraw    = millis();
    }
}

// ── Header ────────────────────────────────────
void DisplayManager::drawHeader(const char* title) {
    _tft.fillRect(0, 0, 320, 28, C_SURFACE);
    _tft.setTextColor(C_GREEN, C_SURFACE);
    _tft.setTextSize(1);
    _tft.setFreeFont(&FreeSansBold9pt7b);
    _tft.drawString("🌿 " + String(title), 8, 7);

    // Heure à droite
    if (_ntp && _ntp->isSynced()) {
        String t = _ntp->getTimeStr();
        t = t.substring(11, 16);  // HH:MM
        _tft.setTextColor(C_MUTED, C_SURFACE);
        _tft.drawString(t, 260, 7);
    }

    _tft.drawFastHLine(0, 28, 320, C_BORDER);
}

// ── Bouton ────────────────────────────────────
void DisplayManager::drawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 const char* label, uint16_t bg, uint16_t fg) {
    _tft.fillRoundRect(x, y, w, h, 6, bg);
    _tft.drawRoundRect(x, y, w, h, 6, C_BORDER);
    _tft.setTextColor(fg, bg);
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(label, x + w/2, y + h/2);
    _tft.setTextDatum(TL_DATUM);
}

// ── Zone card ─────────────────────────────────
void DisplayManager::drawZoneCard(uint8_t zone, uint16_t x, uint16_t y,
                                   uint16_t w, uint16_t h) {
    bool active = _relais && _relais->getState(zone);
    uint16_t bg = active ? C_GREEN_DIM : C_SURFACE;
    uint16_t border = active ? C_GREEN : C_BORDER;

    _tft.fillRoundRect(x, y, w, h, 8, bg);
    _tft.drawRoundRect(x, y, w, h, 8, border);

    // Nom zone
    _tft.setTextColor(C_TEXT, bg);
    _tft.setFreeFont(&FreeSansBold9pt7b);
    _tft.drawString("Zone " + String(zone + 1), x + 8, y + 8);

    // État
    if (active) {
        _tft.setTextColor(C_GREEN, bg);
        _tft.drawString("ON", x + w - 28, y + 8);
    } else {
        _tft.setTextColor(C_MUTED, bg);
        _tft.drawString("OFF", x + w - 32, y + 8);
    }

    // Raison
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextColor(C_MUTED, bg);
    String reason = _schedule ? _schedule->getLastReason(zone) : "N/A";
    if (reason.length() > 18) reason = reason.substring(0, 18) + "..";
    _tft.drawString(reason, x + 8, y + 28);

    // Prochain slot
    if (_schedule) {
        ZoneSchedule zs = _schedule->getZoneSchedule(zone);
        _tft.setTextColor(C_BLUE, bg);
        if (zs.mode == SCHEDULE_MODE_DAYS) {
            // Cherche le premier slot actif
            for (uint8_t d = 0; d < NB_DAYS; d++) {
                for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                    if (zs.daySlots[d].slots[s].enabled) {
                        char buf[20];
                        sprintf(buf, "%s %02d:%02d",
                            d==0?"Lun":d==1?"Mar":d==2?"Mer":
                            d==3?"Jeu":d==4?"Ven":d==5?"Sam":"Dim",
                            zs.daySlots[d].slots[s].hour,
                            zs.daySlots[d].slots[s].minute);
                        _tft.drawString(buf, x + 8, y + 46);
                        goto next;
                    }
                }
            }
            next:;
        } else {
            char buf[20];
            sprintf(buf, "/%dj %02d:%02d",
                zs.intervalDays,
                zs.intervalSlots.slots[0].hour,
                zs.intervalSlots.slots[0].minute);
            _tft.drawString(buf, x + 8, y + 46);
        }
    }
}

// ── Écran HOME ────────────────────────────────
void DisplayManager::drawHome() {
    _tft.fillScreen(C_BG);
    drawHeader("Arrosage");

    // Météo mini
    _tft.fillRoundRect(4, 34, 100, 50, 6, C_SURFACE);
    _tft.drawRoundRect(4, 34, 100, 50, 6, C_BORDER);
    _tft.setFreeFont(&FreeSansBold9pt7b);

    if (_weather) {
        float mm = _weather->getRainMm();
        _tft.setTextColor(C_BLUE, C_SURFACE);
        _tft.setTextDatum(MC_DATUM);
        _tft.drawString(String(mm, 1) + "mm", 54, 52);
        _tft.setFreeFont(&FreeSans9pt7b);
        _tft.setTextColor(_weather->isRainExpected() ? C_AMBER : C_GREEN, C_SURFACE);
        _tft.drawString(_weather->isRainExpected() ? "Bloque" : "OK", 54, 70);
        _tft.setTextDatum(TL_DATUM);
    }

    // Zones
    drawZoneCard(0, 110, 34, 98, 70);
    drawZoneCard(1, 215, 34, 98, 70);

    // Boutons navigation
    drawButton(4,   112, 74, 36, "Detail Z1", C_SURFACE, C_TEXT);
    drawButton(82,  112, 74, 36, "Detail Z2", C_SURFACE, C_TEXT);
    drawButton(160, 112, 74, 36, "Meteo",     C_SURFACE, C_BLUE);
    drawButton(238, 112, 78, 36, "Status",    C_SURFACE, C_MUTED);

    // Barre WiFi
    _tft.fillRect(0, 155, 320, 5, C_BORDER);
    _tft.fillRect(0, 155, 160, 5, C_GREEN);
}

// ── Écran ZONE ────────────────────────────────
void DisplayManager::drawZone(uint8_t zone) {
    _tft.fillScreen(C_BG);
    drawHeader(("Zone " + String(zone + 1)).c_str());

    bool active = _relais && _relais->getState(zone);

    // État grand format
    _tft.fillRoundRect(4, 34, 312, 60, 8,
        active ? C_GREEN_DIM : C_SURFACE);
    _tft.drawRoundRect(4, 34, 312, 60, 8,
        active ? C_GREEN : C_BORDER);

    _tft.setFreeFont(&FreeSansBold9pt7b);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(active ? C_GREEN : C_MUTED,
                      active ? C_GREEN_DIM : C_SURFACE);
    _tft.drawString(active ? "ARROSAGE EN COURS" : "EN ATTENTE",
                    160, 64);
    _tft.setTextDatum(TL_DATUM);

    // Raison
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextColor(C_MUTED, C_BG);
    String reason = _schedule ? _schedule->getLastReason(zone) : "";
    _tft.drawString("Statut : " + reason, 8, 102);

    // Météo
    if (_weather) {
        String meteo = "Meteo : " + String(_weather->getRainMm(), 1) + "mm";
        _tft.setTextColor(_weather->isRainExpected() ? C_AMBER : C_GREEN, C_BG);
        _tft.drawString(meteo, 8, 120);
    }

    // Boutons action
    drawButton(4,   142, 148, 40,
        active ? "Arreter" : "Demarrer",
        active ? C_RED : C_GREEN,
        active ? TFT_WHITE : TFT_BLACK);

    drawButton(160, 142, 76, 40, "Forcer", C_AMBER, TFT_BLACK);
    drawButton(240, 142, 76, 40, "Retour", C_SURFACE, C_TEXT);
}

// ── Écran METEO ───────────────────────────────
void DisplayManager::drawMeteo() {
    _tft.fillScreen(C_BG);
    drawHeader("Meteo");

    if (!_weather) return;

    float mm = _weather->getRainMm();
    bool  blk = _weather->isRainExpected();

    // Valeur principale
    _tft.setFreeFont(&FreeSansBold12pt7b);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(C_BLUE, C_BG);
    _tft.drawString(String(mm, 1) + " mm", 160, 70);

    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextColor(C_MUTED, C_BG);
    _tft.drawString("prevus sur 12h", 160, 95);

    // Badge
    uint16_t badgeBg  = blk ? C_AMBER : C_GREEN;
    uint16_t badgeFg  = TFT_BLACK;
    _tft.fillRoundRect(90, 108, 140, 28, 14, badgeBg);
    _tft.setTextColor(badgeFg, badgeBg);
    _tft.drawString(blk ? "Arrosage bloque" : "Arrosage autorise", 160, 122);

    _tft.setTextDatum(TL_DATUM);

    // Retour
    drawButton(110, 148, 100, 36, "Retour", C_SURFACE, C_TEXT);
}

// ── Écran STATUS ──────────────────────────────
void DisplayManager::drawStatus() {
    _tft.fillScreen(C_BG);
    drawHeader("Systeme");

    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextColor(C_TEXT, C_BG);

    // Infos système
    _tft.drawString("RAM libre : " +
        String(ESP.getFreeHeap() / 1024) + " KB", 8, 40);
    _tft.drawString("Uptime : " +
        String(millis() / 60000) + " min", 8, 60);

    if (_ntp && _ntp->isSynced()) {
        _tft.setTextColor(C_GREEN, C_BG);
        _tft.drawString("NTP : " + _ntp->getTimeStr(), 8, 80);
    }

    // IP WiFi
    _tft.setTextColor(C_BLUE, C_BG);
    _tft.drawString("IP : " + WiFi.localIP().toString(), 8, 100);

    // Version
    _tft.setTextColor(C_MUTED, C_BG);
    _tft.drawString("Arrosage v1.0 - ESP32 CYD", 8, 130);

    drawButton(110, 148, 100, 36, "Retour", C_SURFACE, C_TEXT);
}

// ── Touch ─────────────────────────────────────
void DisplayManager::handleTouch() {
    uint16_t tx, ty;
    if (!_tft.getTouch(&tx, &ty)) return;

    // Debounce
    static uint32_t lastTap = 0;
    if (millis() - lastTap < 300) return;
    lastTap = millis();

    switch (_currentScreen) {
        case SCREEN_HOME:
            if (isTouched(4,   112, 74, 36, tx, ty)) {
                _selectedZone = 0;
                _currentScreen = SCREEN_ZONE;
                _needsRedraw = true;
            } else if (isTouched(82, 112, 74, 36, tx, ty)) {
                _selectedZone = 1;
                _currentScreen = SCREEN_ZONE;
                _needsRedraw = true;
            } else if (isTouched(160, 112, 74, 36, tx, ty)) {
                _currentScreen = SCREEN_METEO;
                _needsRedraw = true;
            } else if (isTouched(238, 112, 78, 36, tx, ty)) {
                _currentScreen = SCREEN_STATUS;
                _needsRedraw = true;
            }
            break;

        case SCREEN_ZONE:
            if (isTouched(4, 142, 148, 40, tx, ty)) {
                // Toggle manuel
                if (_relais) {
                    bool state = !_relais->getState(_selectedZone);
                    _relais->setRelay(_selectedZone, state);
                }
                _needsRedraw = true;
            } else if (isTouched(160, 142, 76, 40, tx, ty)) {
                // Forcer arrosage
                if (_schedule) {
                    bool current = _schedule->getZoneSchedule(_selectedZone).forceToday;
                    _schedule->setForceToday(_selectedZone, !current);
                }
                _needsRedraw = true;
            } else if (isTouched(240, 142, 76, 40, tx, ty)) {
                _currentScreen = SCREEN_HOME;
                _needsRedraw = true;
            }
            break;

        case SCREEN_METEO:
        case SCREEN_STATUS:
            if (isTouched(110, 148, 100, 36, tx, ty)) {
                _currentScreen = SCREEN_HOME;
                _needsRedraw = true;
            }
            break;
    }
}

bool DisplayManager::isTouched(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                uint16_t tx, uint16_t ty) {
    return tx >= x && tx <= x + w && ty >= y && ty <= y + h;
}

uint16_t DisplayManager::color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}