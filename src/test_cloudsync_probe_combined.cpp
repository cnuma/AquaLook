// Banc de test isole, ETAPE 5/N : combine les TROIS composants deja
// testes SEULS (AsyncTCP/ESPAsyncWebServer etape 2, carte SD etape 3,
// ecran QSPI etape 4 -- TOUS RAS individuellement, voir
// docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md)
// EN MEME TEMPS, a cote du banc
// CloudSync de base (test_cloudsync_probe_common.*). Teste l'hypothese
// d'INTERSECTION : la contention ne se manifesterait peut-etre qu'a la
// rencontre de plusieurs composants, pas dans chacun pris isolement --
// hypothese renforcee par un vrai bug historique de ce type deja trouve
// sur cette carte (contention SD/AsyncTCP cross-tache, voir
// StorageManager.cpp et le commentaire dans test_cloudsync_probe_sd.cpp).
//
// Reprend le code de chaque etape precedente tel quel (memes broches,
// memes rythmes, meme trafic LAN genere depuis le PC pendant le test).

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>

#include "test_cloudsync_probe_common.h"

namespace {

// ── AsyncTCP (etape 2) ───────────────────────────────────────────────
AsyncWebServer server(80);
uint32_t requestCount = 0;

// ── SD (etape 3) ─────────────────────────────────────────────────────
SPIClass spiSD(HSPI);
constexpr uint32_t SD_IO_INTERVAL_MS = 1500U;
const char* const SD_TEST_PATH = "/aqualook_probe_sd.txt";
uint32_t sdWriteCount = 0;
uint32_t sdErrorCount = 0;
bool sdReady = false;

// ── Ecran (etape 4) ──────────────────────────────────────────────────
Arduino_DataBus* bus = new Arduino_ESP32QSPI(
    AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);
Arduino_NV3041A* panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS);
Arduino_GFX* gfx = new Arduino_Canvas(AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT, panel);
constexpr uint32_t REDRAW_INTERVAL_MS = 1000U;
uint32_t redrawCount = 0;

}  // namespace

void setup() {
    ProbeCommon::begin(
        "=== AquaLook - banc isole ETAPE 5/N : SD + AsyncTCP + ecran COMBINES ===");

    // AsyncTCP
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        ++requestCount;
        req->send(200, "text/plain", "ok");
    });
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        ++requestCount;
        req->send(200, "application/json",
                  "{\"probe\":true,\"heapFree\":" + String(ESP.getFreeHeap()) + "}");
    });
    server.begin();
    Serial.println(F("[PROBE] serveur AsyncTCP demarre sur :80"));

    // SD
    Serial.printf("[PROBE] SD (bus SPI dedie) : MISO=13 MOSI=11 SCLK=12 CS=10\n");
    spiSD.begin(12, 13, 11, 10);
    sdReady = SD.begin(10, spiSD);
    if (!sdReady) {
        Serial.println(F("[PROBE] ECHEC SD.begin() -- le test continue SANS acces SD reels"));
    } else {
        Serial.println(F("[PROBE] SD montee"));
    }

    // Ecran
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

    // SD
    static uint32_t nextSdIoAtMs = 0;
    if (sdReady && static_cast<int32_t>(millis() - nextSdIoAtMs) >= 0) {
        nextSdIoAtMs = millis() + SD_IO_INTERVAL_MS;
        bool ok = false;
        File w = SD.open(SD_TEST_PATH, FILE_WRITE);
        if (w) {
            String payload = "probe-sd-" + String(millis());
            ok = (w.print(payload) == static_cast<int>(payload.length()));
            w.close();
        }
        if (ok) {
            File r = SD.open(SD_TEST_PATH, FILE_READ);
            ok = static_cast<bool>(r);
            if (r) r.close();
        }
        ++sdWriteCount;
        if (!ok) {
            ++sdErrorCount;
            Serial.printf("[PROBE] SD : echec E/S #%lu (sur %lu tentatives)\n",
                          static_cast<unsigned long>(sdErrorCount),
                          static_cast<unsigned long>(sdWriteCount));
        }
    }

    // Ecran
    static uint32_t nextRedrawAtMs = 0;
    if (static_cast<int32_t>(millis() - nextRedrawAtMs) >= 0) {
        nextRedrawAtMs = millis() + REDRAW_INTERVAL_MS;
        ++redrawCount;
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
        Serial.printf(
            "[PROBE] combine : LAN=%lu req, SD=%lu E/S (%lu err), ecran=%lu rafraichissements\n",
            static_cast<unsigned long>(requestCount),
            static_cast<unsigned long>(sdWriteCount),
            static_cast<unsigned long>(sdErrorCount),
            static_cast<unsigned long>(redrawCount));
    }

    delay(50);
}
