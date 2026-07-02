#include "ScreenManager.h"

// ─────────────────────────────────────────────────────────────
void ScreenManager::begin(ConfigManager* config) {
    _config = config;

    // Pins rétroéclairage
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);  // allumé au boot

    // LED RGB active LOW, pilotée en PWM 8 bits
    ledcSetup(LED_CH_RED,   5000, 8);
    ledcSetup(LED_CH_GREEN, 5000, 8);
    ledcSetup(LED_CH_BLUE,  5000, 8);
    ledcAttachPin(PIN_LED_RED,   LED_CH_RED);
    ledcAttachPin(PIN_LED_GREEN, LED_CH_GREEN);
    ledcAttachPin(PIN_LED_BLUE,  LED_CH_BLUE);
    ledOff();

    _lastActivity = millis();
    _sleeping     = false;

    Serial.println("[Screen] ScreenManager OK");
}

// ─────────────────────────────────────────────────────────────
//  update() — appelé dans loop()
// ─────────────────────────────────────────────────────────────
void ScreenManager::update(bool anyRelayActive) {
    const uint32_t now = millis();

    // Un relais vient de s'activer → réveil forcé
    if (anyRelayActive && !_relayWasActive) {
        wakeUp();
    }
    _relayWasActive = anyRelayActive;

    uint8_t timeoutMin = _config ? _config->system().screenTimeoutMin : 5;

    if (!_sleeping) {
        // Vérifier si on doit passer en veille
        if (timeoutMin > 0 &&
            (now - _lastActivity) >= (uint32_t)timeoutMin * 60000UL) {
            screenOff();
        }
    } else {
        // En veille — animer la LED signe de vie
        updateLed(anyRelayActive);
    }
}

// ─────────────────────────────────────────────────────────────
//  wakeUp() — réveil depuis touch ou événement externe
// ─────────────────────────────────────────────────────────────
void ScreenManager::wakeUp() {
    _lastActivity = millis();
    if (_sleeping) {
        screenOn();
    }
}

// ─────────────────────────────────────────────────────────────
//  Contrôle rétroéclairage
// ─────────────────────────────────────────────────────────────
void ScreenManager::screenOn() {
    _sleeping = false;
    digitalWrite(PIN_TFT_BL, HIGH);
    ledOff();
    Serial.println("[Screen] Réveil");
}

void ScreenManager::screenOff() {
    _sleeping  = true;
    _ledTimer  = 0;
    _ledPhase  = 0;
    digitalWrite(PIN_TFT_BL, LOW);
    Serial.println("[Screen] Veille");
}

// ─────────────────────────────────────────────────────────────
//  LED signe de vie
// ─────────────────────────────────────────────────────────────
// Séquence arc-en-ciel : 6 couleurs RVB pures, interpolées entre elles
static const uint8_t LED_RAINBOW[6][3] = {
    {  0, 255,   0},  // vert
    {  0, 255, 255},  // cyan
    {  0,   0, 255},  // bleu
    {255,   0, 255},  // violet
    {255,   0,   0},  // rouge
    {255, 255,   0},  // jaune
};

// Séquence bicolore : vert + bleu
static const bool LED_BICOLOR[2][3] = {
    {false, true,  false},
    {false, false, true },
};

void ScreenManager::updateLed(bool relayActive) {
    const uint32_t now = millis();
    uint8_t mode = _config ? _config->system().ledMode : 1;

    // Override relais actif : LED rouge clignote toutes les 500ms
    if (relayActive) {
        if (now - _ledTimer >= 500UL) {
            _ledTimer = now;
            _ledPhase ^= 1U;
            if (_ledPhase) ledSet(true, false, false);
            else           ledOff();
        }
        return;
    }

    switch (mode) {
        case 0:
            ledOff();
            break;

        case 1:
            // Flash discret vert — 150ms allumé toutes les 4s
            if (_ledPhase == 0 && (now - _ledTimer) >= 4000UL) {
                _ledTimer = now;
                _ledPhase = 1;
                ledSet(false, true, false);
            } else if (_ledPhase == 1 && (now - _ledTimer) >= 150UL) {
                _ledTimer = now;
                _ledPhase = 0;
                ledOff();
            }
            break;

        case 2: {
            // Respiration verte progressive sur 4 secondes
            constexpr uint32_t PERIOD_MS = 4000UL;
            constexpr uint32_t HALF_MS   = PERIOD_MS / 2UL;
            const uint32_t phase = now % PERIOD_MS;
            const uint32_t level = phase < HALF_MS
                ? (phase * 180UL) / HALF_MS
                : ((PERIOD_MS - phase) * 180UL) / HALF_MS;
            ledSetBrightness(0, (uint8_t)(12UL + level), 0);
            break;
        }

        case 3: {
            // Arc-en-ciel lent et continu : chaque transition dure 2 secondes
            constexpr uint32_t STEP_MS  = 2000UL;
            constexpr uint32_t CYCLE_MS = STEP_MS * 6UL;
            const uint32_t cyclePos = now % CYCLE_MS;
            const uint8_t from = cyclePos / STEP_MS;
            const uint8_t to   = (from + 1U) % 6U;
            const uint32_t local = cyclePos % STEP_MS;

            const uint8_t r = LED_RAINBOW[from][0] +
                (int32_t)(LED_RAINBOW[to][0] - LED_RAINBOW[from][0]) *
                (int32_t)local / (int32_t)STEP_MS;
            const uint8_t g = LED_RAINBOW[from][1] +
                (int32_t)(LED_RAINBOW[to][1] - LED_RAINBOW[from][1]) *
                (int32_t)local / (int32_t)STEP_MS;
            const uint8_t b = LED_RAINBOW[from][2] +
                (int32_t)(LED_RAINBOW[to][2] - LED_RAINBOW[from][2]) *
                (int32_t)local / (int32_t)STEP_MS;

            ledSetBrightness(r, g, b);
            break;
        }

        case 4:
            // Bicolore séquence — alterne vert/bleu toutes les 3s
            if (now - _ledTimer >= 3000UL) {
                _ledTimer = now;
                _ledPhase ^= 1U;
                ledSet(LED_BICOLOR[_ledPhase][0],
                       LED_BICOLOR[_ledPhase][1],
                       LED_BICOLOR[_ledPhase][2]);
            }
            break;

        default:
            ledOff();
            break;
    }
}

// ─────────────────────────────────────────────────────────────
//  Helpers LED — LED active LOW, PWM inversé
// ─────────────────────────────────────────────────────────────
void ScreenManager::ledOff() {
    ledSetBrightness(0, 0, 0);
}

void ScreenManager::ledSet(bool r, bool g, bool b) {
    ledSetBrightness(r ? 255 : 0,
                     g ? 255 : 0,
                     b ? 255 : 0);
}

void ScreenManager::ledSetBrightness(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(LED_CH_RED,   255 - r);
    ledcWrite(LED_CH_GREEN, 255 - g);
    ledcWrite(LED_CH_BLUE,  255 - b);
}
