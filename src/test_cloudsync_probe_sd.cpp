// Banc de test isole, ETAPE 3/N : reprend EXACTEMENT le banc de l'etape 1
// (TLS+requete CloudSync reelle, voir test_cloudsync_probe.cpp et
// docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md)
// et rajoute UN SEUL
// composant : des acces carte SD periodiques (ecriture+lecture), sur le
// MEME bus SPI dedie (HSPI, MISO=13 MOSI=11 SCLK=12 CS=10) que
// StorageManager en production -- brochage et pattern d'appel repris de
// test_sd_s3.cpp (deja valide sur ce materiel).
//
// Hypothese testee : la piste SD deja notee "correlation non confirmee"
// dans le checkpoint (incidents SD survenant dans les memes fenetres
// temporelles que les echecs CloudSync) -- et StorageManager.cpp documente
// un VRAI bug historique de contention SD/AsyncTCP cross-tache sur cette
// meme carte (assert xQueueGenericSend, corrige par SHARED_SPI), donc ce
// coin du systeme a deja une histoire de conflits subtils entre taches.
//
// AsyncTCP n'est PAS inclus ici (deja teste seul a l'etape 2, RAS) : cette
// etape isole la SD seule d'abord. Si elle aussi est RAS, l'etape suivante
// combinera SD+AsyncTCP+ecran ensemble (le trio complet), au cas ou la
// contention n'apparaitrait qu'a l'intersection de plusieurs composants.

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "test_cloudsync_probe_common.h"

namespace {
SPIClass spiSD(HSPI);
constexpr uint32_t SD_IO_INTERVAL_MS = 1500U;  // rythme soutenu, pas du martelage
const char* const SD_TEST_PATH = "/aqualook_probe_sd.txt";
uint32_t sdWriteCount = 0;
uint32_t sdErrorCount = 0;
bool sdReady = false;
}  // namespace

void setup() {
    ProbeCommon::begin(
        "=== AquaLook - banc isole ETAPE 3/N : + acces carte SD periodiques ===");

    Serial.printf("[PROBE] SD (bus SPI dedie) : MISO=13 MOSI=11 SCLK=12 CS=10\n");
    spiSD.begin(12 /*SCLK*/, 13 /*MISO*/, 11 /*MOSI*/, 10 /*CS*/);
    sdReady = SD.begin(10, spiSD);
    if (!sdReady) {
        Serial.println(F("[PROBE] ECHEC SD.begin() -- le test continue SANS acces SD reels"));
    } else {
        uint8_t cardType = SD.cardType();
        const char* typeName = cardType == CARD_MMC   ? "MMC"
                                : cardType == CARD_SD  ? "SDSC"
                                : cardType == CARD_SDHC ? "SDHC"
                                                        : "INCONNU/ABSENT";
        Serial.printf("[PROBE] SD montee : type=%s\n", typeName);
    }
}

void loop() {
    ProbeCommon::loop();

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

    static uint32_t lastReportMs = 0;
    if (millis() - lastReportMs >= 10000U) {
        lastReportMs = millis();
        Serial.printf("[PROBE] SD : %lu E/S effectuees, %lu erreurs\n",
                      static_cast<unsigned long>(sdWriteCount),
                      static_cast<unsigned long>(sdErrorCount));
    }

    delay(50);
}
