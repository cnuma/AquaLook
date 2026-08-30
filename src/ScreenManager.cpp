#include "ScreenManager.h"
#include "FaultManager.h"
#include "MaintenanceResult.h"
#include "StatusLed.h"
#include "Theme.h"

static const uint8_t LED_RAINBOW[6][3] = {
    {0, 255, 0},
    {0, 255, 255},
    {0, 0, 255},
    {255, 0, 255},
    {255, 0, 0},
    {255, 255, 0}
};

static const bool LED_BICOLOR[2][3] = {
    {false, true, false},
    {false, false, true}
};

#if AQUALOOK_BOARD_S3
// Retroeclairage de la JC4827W543C_I : AQ_S3_LCD_BL (GPIO1), pilote en PWM
// et non en tout-ou-rien comme PIN_TFT_BL sur la carte historique.
//
// Le canal LEDC est deja configure par TFT_eSPI::init()
// (lib/tft_espi_compat_s3), appele par DisplayManager::initTft() bien avant
// ScreenManager::begin() dans setup(). On se contente donc d'ecrire le
// rapport cyclique, sans reconfigurer : reconfigurer ici risquerait de
// diverger silencieusement des reglages de l'adaptateur.
//
// Resolution 12 bits => pleine echelle a 4095. Avant le 29 aout 2026 la
// veille ecran etait inoperante sur cette carte : ScreenManager ne pilotait
// que PIN_TFT_BL (=21), qui est en realite une ligne de donnees du bus QSPI
// ici et ne devait surtout pas etre touchee.
static inline void setBacklight(bool on) {
    ledcWrite(AQ_S3_LCD_BL_CHANNEL, on ? 4095 : 0);
}
#endif

void ScreenManager::begin(ConfigManager* config) {
    _config = config;

#if !AQUALOOK_BOARD_S3
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);
#else
    setBacklight(true);
#endif
    // Sur la JC4827W543C_I, PIN_TFT_BL (=21, retroeclairage de l'ancienne
    // carte) est en realite AQ_S3_LCD_D0, une ligne de donnees du bus QSPI
    // de l'ecran (platformio.ini). pinMode()/digitalWrite() dessus la
    // reconfigurait en simple GPIO et cassait le bus QSPI en silence des
    // ce begin() - trouve le 28 aout 2026 par bissection : le splash
    // (dessine avant ScreenManager::begin()) marchait, tout dessin
    // ulterieur (HOME, sprites, meme un fillRect direct) restait invisible.
    // D'ou le pilotage par setBacklight() ci-dessus, sur la vraie broche.

    // Voyant d'etat : canaux LEDC de la LED RGB embarquee sur la carte
    // historique, ruban WS2812 sur GPIO46 sur la S3 (qui n'a aucun
    // voyant embarque - HW_JC4827W543_PORT_IMPACT.md §8 test 7, et les
    // anciennes broches 4/17 y percutent le tactile GT911 et le bus I2C
    // du bloc relais). Le choix est confine dans StatusLed.cpp.
    AquaLook::StatusLed::begin();
    if (_config) {
        AquaLook::StatusLed::setZoneCount(_config->system().nbZones);
    }

    _normalLedRed = 0;
    _normalLedGreen = 0;
    _normalLedBlue = 0;
    loadUpdateState();
    renderLed();

    _lastActivity = millis();
    _sleeping = false;

    Serial.println("[Screen] ScreenManager OK");
}

void ScreenManager::update(bool anyRelayActive, bool wifiSearching,
                           uint16_t activeZoneMask, uint8_t nbZones,
                           uint16_t rainBlockedMask) {
    const uint32_t now = millis();

    if (anyRelayActive && !_relayWasActive) {
        wakeUp();
    }
    _relayWasActive = anyRelayActive;

    const uint8_t timeoutMin =
        _config ? _config->system().screenTimeoutMin : 5;

    if (!_sleeping &&
        timeoutMin > 0 &&
        (now - _lastActivity) >=
            (uint32_t)timeoutMin * 60000UL) {
        screenOff();
    }

    // La LED reste le seul retour visuel en veille ecran (backlight eteint) :
    // recherche WiFi visible meme ecran off, pas seulement en usage normal.
    if (_sleeping) {
        updateLed(anyRelayActive, wifiSearching);
        renderZones(activeZoneMask, rainBlockedMask, nbZones);
    } else {
        ledOff();
        // Ecran allume : l'utilisateur a les cartes de zone sous les yeux,
        // le ruban n'apporte rien et resterait allume pour rien.
        AquaLook::StatusLed::clearZones();
    }

    renderLed();
}

void ScreenManager::wakeUp() {
    _lastActivity = millis();

    if (_sleeping) {
        screenOn();
    }
}

void ScreenManager::screenOn() {
    _sleeping = false;
#if AQUALOOK_BOARD_S3
    setBacklight(true);
#else
    digitalWrite(PIN_TFT_BL, HIGH);
#endif
    ledOff();

    Serial.println("[Screen] Réveil");
}

void ScreenManager::screenOff() {
    _sleeping = true;
    _ledTimer = 0;
    _ledPhase = 0;

#if AQUALOOK_BOARD_S3
    setBacklight(false);
#else
    digitalWrite(PIN_TFT_BL, LOW);
#endif

    Serial.println("[Screen] Veille");
}

void ScreenManager::loadUpdateState() {
    const MaintenanceResult result = MaintenanceResultStore::load();
    const bool firmwarePending = result.valid &&
                                 result.updateAvailable &&
                                 result.availableVersion[0] != '\0';
    const bool webAssetsPending = result.valid &&
                                  result.webAssetsUpdateAvailable &&
                                  result.webAssetsAvailableVersion[0] != '\0';
    _updatePending = firmwarePending || webAssetsPending;
    _updateStateLoaded = true;
}

void ScreenManager::updateLed(bool relayActive, bool wifiSearching) {
    const uint32_t now = millis();
    const uint8_t mode =
        _config ? _config->system().ledMode : 1;

    // Code couleur volontairement disjoint de celui de FaultManager::
    // resolveColor() (rouge clignotant, applique ensuite par renderLed()) :
    // le rouge doit rester reserve exclusivement a une erreur/panne
    // detectee, jamais reutilise ici pour un etat operationnel normal.
    if (relayActive) {
        if (now - _ledTimer >= 500UL) {
            _ledTimer = now;
            _ledPhase ^= 1U;

            if (_ledPhase) {
                ledSet(false, false, true);
            } else {
                ledOff();
            }
        }
        return;
    }

    // Recherche WiFi (connexion en cours ou reconnexion apres zombie) :
    // clignotement ambre rapide, priorite juste sous l'arrosage actif.
    // Meme couleur que l'icone signal du LCD (renderSignalSprite()) —
    // avant, le LCD clignotait en ambre et la LED en bleu pour le meme
    // etat, incoherence corrigee ici.
    if (wifiSearching) {
        if (now - _ledTimer >= 250UL) {
            _ledTimer = now;
            _ledPhase ^= 1U;

            if (_ledPhase) {
                ledSetBrightness(255, 100, 0);
            } else {
                ledOff();
            }
        }
        return;
    }

    // Mise a jour en attente (firmware ou ressources Web) : clignotement
    // violet lent, meme teinte que l'icone LCD (Theme::PURPLE) et la
    // pastille Web (--purple #6633cc). Priorite sous arrosage/WiFi, mais
    // au-dessus du mode LED normal choisi par l'utilisateur -- une mise a
    // jour disponible reste visible quel que soit le mode.
    if (!_updateStateLoaded) loadUpdateState();
    if (_updatePending) {
        if (now - _ledTimer >= 1500UL) {
            _ledTimer = now;
            _ledPhase ^= 1U;

            if (_ledPhase) {
                ledSetBrightness(102, 51, 204);
            } else {
                ledOff();
            }
        }
        return;
    }

    switch (mode) {
        case 0:
            ledOff();
            break;

        case 1:
            if (_ledPhase == 0 &&
                (now - _ledTimer) >= 4000UL) {
                _ledTimer = now;
                _ledPhase = 1;
                ledSet(false, true, false);
            } else if (_ledPhase == 1 &&
                       (now - _ledTimer) >= 150UL) {
                _ledTimer = now;
                _ledPhase = 0;
                ledOff();
            }
            break;

        case 2: {
            constexpr uint32_t PERIOD_MS = 4000UL;
            constexpr uint32_t HALF_MS = PERIOD_MS / 2UL;

            const uint32_t phase = now % PERIOD_MS;
            const uint32_t level = phase < HALF_MS
                ? (phase * 180UL) / HALF_MS
                : ((PERIOD_MS - phase) * 180UL) / HALF_MS;

            ledSetBrightness(
                0,
                (uint8_t)(12UL + level),
                0
            );
            break;
        }

        case 3: {
            constexpr uint32_t STEP_MS = 2000UL;
            constexpr uint32_t CYCLE_MS = STEP_MS * 6UL;

            const uint32_t cyclePos = now % CYCLE_MS;
            const uint8_t from = cyclePos / STEP_MS;
            const uint8_t to = (from + 1U) % 6U;
            const uint32_t local = cyclePos % STEP_MS;

            const uint8_t r =
                LED_RAINBOW[from][0] +
                (int32_t)(
                    LED_RAINBOW[to][0] -
                    LED_RAINBOW[from][0]
                ) *
                (int32_t)local /
                (int32_t)STEP_MS;

            const uint8_t g =
                LED_RAINBOW[from][1] +
                (int32_t)(
                    LED_RAINBOW[to][1] -
                    LED_RAINBOW[from][1]
                ) *
                (int32_t)local /
                (int32_t)STEP_MS;

            const uint8_t b =
                LED_RAINBOW[from][2] +
                (int32_t)(
                    LED_RAINBOW[to][2] -
                    LED_RAINBOW[from][2]
                ) *
                (int32_t)local /
                (int32_t)STEP_MS;

            ledSetBrightness(r, g, b);
            break;
        }

        case 4:
            if (now - _ledTimer >= 3000UL) {
                _ledTimer = now;
                _ledPhase ^= 1U;

                ledSet(
                    LED_BICOLOR[_ledPhase][0],
                    LED_BICOLOR[_ledPhase][1],
                    LED_BICOLOR[_ledPhase][2]
                );
            }
            break;

        default:
            ledOff();
            break;
    }
}

void ScreenManager::renderLed() {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    FaultManager::resolveColor(
        _normalLedRed,
        _normalLedGreen,
        _normalLedBlue,
        red,
        green,
        blue
    );

    // Seule sortie materielle du voyant de tout le firmware. La logique
    // ci-dessus (updateLed, FaultManager) ne sait pas et n'a pas a savoir
    // s'il s'agit d'une LED RGB embarquee ou d'un ruban WS2812.
    AquaLook::StatusLed::setStatus(red, green, blue);
    AquaLook::StatusLed::commit();
}

void ScreenManager::renderZones(uint16_t activeZoneMask, uint16_t rainBlockedMask,
                                uint8_t nbZones) {
#if !AQUALOOK_BOARD_S3
    // Carte historique : voyant unique, aucune LED de zone a piloter.
    // Neutralise a la compilation plutot qu'en s'appuyant sur le fait que
    // StatusLed::setZone() n'y fait rien : sinon la carte de production
    // paierait a chaque tour de boucle une sinusoide calculee pour rien.
    (void)activeZoneMask; (void)rainBlockedMask; (void)nbZones;
#else
    if (nbZones == 0U) return;   // appelant qui ne fournit pas l'etat par zone

    AquaLook::StatusLed::setZoneCount(nbZones);

    // Respiration commune : sinusoide sur 2 s, jamais totalement eteinte
    // pour que la zone reste identifiable meme au creux.
    const float phase = (millis() % 2000UL) / 2000.0f;
    const float wave  = 0.5f * (1.0f - cosf(phase * 2.0f * (float)PI));
    const uint16_t level = (uint16_t)(40.0f + wave * 215.0f);

    for (uint8_t z = 0; z < nbZones; z++) {
        const uint16_t bit = (uint16_t)(1U << z);

        // La LED porte un ETAT, pas une identite : la position designe
        // deja la zone, la couleur est donc libre de dire autre chose.
        //
        //   bleu    arrosage en cours
        //   orange  arrosage prevu mais suspendu par la pluie
        //   eteint  rien de prevu
        //
        // L'arrosage prime sur le blocage : une zone qui arrose n'est,
        // par definition, pas bloquee.
        uint16_t themeColor;
        if (activeZoneMask & bit) {
            themeColor = Theme::BLUE;
        } else if (rainBlockedMask & bit) {
            // Exactement la teinte dont le planning colore deja un creneau
            // suspendu pour cause de pluie (rainBlk ? Theme::AMBER : ...),
            // pour que le ruban et l'ecran ne puissent pas se contredire.
            themeColor = Theme::AMBER;
        } else {
            AquaLook::StatusLed::setZone(z, 0, 0, 0);
            continue;
        }

        // 565 -> 888, puis mise a l'echelle par la respiration. Passer par
        // les constantes du theme plutot que par des valeurs recopiees
        // garantit que ruban et LCD suivent la meme source.
        const uint16_t r5 = (uint16_t)((themeColor >> 11) & 0x1FU);
        const uint16_t g6 = (uint16_t)((themeColor >> 5)  & 0x3FU);
        const uint16_t b5 = (uint16_t)(themeColor         & 0x1FU);

        const uint8_t r = (uint8_t)((r5 * 255U / 31U) * level / 255U);
        const uint8_t g = (uint8_t)((g6 * 255U / 63U) * level / 255U);
        const uint8_t b = (uint8_t)((b5 * 255U / 31U) * level / 255U);

        AquaLook::StatusLed::setZone(z, r, g, b);
    }
#endif
}

void ScreenManager::ledOff() {
    ledSetBrightness(0, 0, 0);
}

void ScreenManager::ledSet(bool r, bool g, bool b) {
    ledSetBrightness(
        r ? 255 : 0,
        g ? 255 : 0,
        b ? 255 : 0
    );
}

void ScreenManager::ledSetBrightness(
    uint8_t r,
    uint8_t g,
    uint8_t b
) {
    _normalLedRed = r;
    _normalLedGreen = g;
    _normalLedBlue = b;
}
