// Banc de test isole, ETAPE 4/N : reprend EXACTEMENT le banc de l'etape 1
// (TLS+requete CloudSync reelle, voir test_cloudsync_probe.cpp et
// docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md)
// et rajoute UN SEUL
// composant : des rafraichissements d'ecran periodiques sur le VRAI bus
// QSPI (pilote Arduino_GFX, meme brochage/panel que DisplayManager en
// production) -- PAS la classe DisplayManager complete (qui entrainerait
// EventBus/FaultManager/HomeCache/etc., a l'oppose de l'isolation
// recherchee ici) : init + dessin repris directement de test_sd_s3.cpp
// (deja valide sur ce materiel), pilote au plus pres du vrai
// Arduino_ESP32QSPI/Arduino_NV3041A/Arduino_Canvas.
//
// AsyncTCP (RAS a l'etape 2) et SD (RAS a l'etape 3) deja ecartes seuls.
// Si l'ecran seul est RAS aussi, l'etape suivante combinera les trois
// ensemble (SD+AsyncTCP+ecran), au cas ou la contention n'apparaitrait
// qu'a l'intersection de plusieurs composants.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "test_cloudsync_probe_common.h"

namespace {
Arduino_DataBus* bus = new Arduino_ESP32QSPI(
    AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);
Arduino_NV3041A* panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS);
Arduino_GFX* gfx = new Arduino_Canvas(AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT, panel);

constexpr uint32_t REDRAW_INTERVAL_MS = 1000U;  // ~1 rafraichissement/s, rythme d'une horloge affichee
uint32_t redrawCount = 0;
}  // namespace

void setup() {
    ProbeCommon::begin(
        "=== AquaLook - banc isole ETAPE 4/N : + rafraichissements ecran QSPI periodiques ===");

    ledcSetup(AQ_S3_LCD_BL_CHANNEL, 5000, 12);
    ledcAttachPin(AQ_S3_LCD_BL, AQ_S3_LCD_BL_CHANNEL);
    ledcWrite(AQ_S3_LCD_BL_CHANNEL, 4095);
    gfx->begin();
    gfx->fillScreen(0x0000);
    gfx->flush();
    Serial.println(F("[PROBE] ecran QSPI initialise"));
}

void loop() {
    ProbeCommon::loop();

    static uint32_t nextRedrawAtMs = 0;
    if (static_cast<int32_t>(millis() - nextRedrawAtMs) >= 0) {
        nextRedrawAtMs = millis() + REDRAW_INTERVAL_MS;
        ++redrawCount;

        // Dessin partiel (pas fillScreen complet) pour se rapprocher d'un
        // rafraichissement d'horloge/etat, comme DisplayManager::update()
        // en production -- pas un simple ecran fixe.
        gfx->fillRect(10, 10, 300, 40, 0x0000);
        gfx->setTextColor(0xFFFF);
        gfx->setTextSize(2);
        gfx->setCursor(10, 20);
        gfx->printf("probe %lu", static_cast<unsigned long>(redrawCount));
        gfx->flush();
    }

    static uint32_t lastReportMs = 0;
    if (millis() - lastReportMs >= 10000U) {
        lastReportMs = millis();
        Serial.printf("[PROBE] ecran : %lu rafraichissements effectues\n",
                      static_cast<unsigned long>(redrawCount));
    }

    delay(50);
}
