/**
 * @file    test_relais.cpp
 * @brief   Diagnostic XL9535 — identification logique relais
 * @details Teste les deux logiques (inverse et directe) pour determiner
 *          laquelle correspond au hardware physique.
 *          Sequence :
 *            1. Scan I2C + dump registres initiaux
 *            2. Test logique INVERSE  (bit=0 -> ON, bit=1 -> OFF)
 *            3. Test logique DIRECTE  (bit=1 -> ON, bit=0 -> OFF)
 *            4. Loop : commande interactive via Serial (i=inverse, d=directe)
 *          Build : env "test_relais" dans platformio.ini
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// ── Registres XL9535 ─────────────────────────
#define XL9535_REG_INPUT_P0   0x00
#define XL9535_REG_INPUT_P1   0x01
#define XL9535_REG_OUTPUT_P0  0x02
#define XL9535_REG_OUTPUT_P1  0x03
#define XL9535_REG_CONFIG_P0  0x06
#define XL9535_REG_CONFIG_P1  0x07

// ── Helpers I2C ──────────────────────────────

static uint8_t writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission();
}

static uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0xFF;
    Wire.requestFrom((uint8_t)XL9535_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

// ── Affichage registres ───────────────────────

static void dumpRegisters(const char* label) {
    Serial.printf("\n--- Registres %s ---\n", label);
    const struct { uint8_t r; const char* n; } regs[] = {
        {0x00,"INPUT_P0 "},{0x01,"INPUT_P1 "},
        {0x02,"OUTPUT_P0"},{0x03,"OUTPUT_P1"},
        {0x06,"CONFIG_P0"},{0x07,"CONFIG_P1"}
    };
    for (auto& e : regs) {
        uint8_t v = readReg(e.r);
        Serial.printf("  %s (0x%02X) = 0x%02X  [%08b]\n", e.n, e.r, v, v);
    }
}

// ── Init XL9535 ──────────────────────────────

// Ecriture OUTPUT AVANT CONFIG pour eviter glitch (voir RelaisManager)
static void initXL9535(uint8_t offValue) {
    writeReg(XL9535_REG_OUTPUT_P0, offValue);
    writeReg(XL9535_REG_OUTPUT_P1, offValue);
    delay(5);
    writeReg(XL9535_REG_CONFIG_P0, 0x00);
    writeReg(XL9535_REG_CONFIG_P1, 0x00);
    delay(5);
    writeReg(XL9535_REG_OUTPUT_P0, offValue);
    writeReg(XL9535_REG_OUTPUT_P1, offValue);
}

// ── Tests logiques ────────────────────────────

/**
 * @brief Test logique inverse : bit=0 -> ON, bit=1 -> OFF
 *        OFF initial = 0xFF
 */
static void testLogiqueInverse() {
    Serial.println("\n========================================");
    Serial.println("  TEST LOGIQUE INVERSE (bit=0 -> ON)");
    Serial.println("  OFF initial = 0xFF");
    Serial.println("========================================");

    initXL9535(0xFF);
    dumpRegisters("init inverse");

    Serial.println("\n  -> Z1 ON (OUTPUT_P0 bit0 = 0)");
    writeReg(XL9535_REG_OUTPUT_P0, 0xFE);  // bit0=0
    Serial.printf("     OUTPUT_P0 lu = 0x%02X\n", readReg(XL9535_REG_OUTPUT_P0));
    Serial.println("     >>> Relais 1 excite ? (oui=inverse OK)");
    delay(2000);

    Serial.println("\n  -> Z1 OFF (OUTPUT_P0 = 0xFF)");
    writeReg(XL9535_REG_OUTPUT_P0, 0xFF);
    delay(1000);

    Serial.println("\n  -> Z2 ON (OUTPUT_P0 bit1 = 0)");
    writeReg(XL9535_REG_OUTPUT_P0, 0xFD);  // bit1=0
    Serial.printf("     OUTPUT_P0 lu = 0x%02X\n", readReg(XL9535_REG_OUTPUT_P0));
    Serial.println("     >>> Relais 2 excite ?");
    delay(2000);

    Serial.println("\n  -> Tout OFF (0xFF)");
    writeReg(XL9535_REG_OUTPUT_P0, 0xFF);
    delay(1000);
}

/**
 * @brief Test logique directe : bit=1 -> ON, bit=0 -> OFF
 *        OFF initial = 0x00
 */
static void testLogiqueDirect() {
    Serial.println("\n========================================");
    Serial.println("  TEST LOGIQUE DIRECTE (bit=1 -> ON)");
    Serial.println("  OFF initial = 0x00");
    Serial.println("========================================");

    initXL9535(0x00);
    dumpRegisters("init directe");

    Serial.println("\n  -> Z1 ON (OUTPUT_P0 bit0 = 1)");
    writeReg(XL9535_REG_OUTPUT_P0, 0x01);  // bit0=1
    Serial.printf("     OUTPUT_P0 lu = 0x%02X\n", readReg(XL9535_REG_OUTPUT_P0));
    Serial.println("     >>> Relais 1 excite ? (oui=directe OK)");
    delay(2000);

    Serial.println("\n  -> Z1 OFF (OUTPUT_P0 = 0x00)");
    writeReg(XL9535_REG_OUTPUT_P0, 0x00);
    delay(1000);

    Serial.println("\n  -> Z2 ON (OUTPUT_P0 bit1 = 1)");
    writeReg(XL9535_REG_OUTPUT_P0, 0x02);  // bit1=1
    Serial.printf("     OUTPUT_P0 lu = 0x%02X\n", readReg(XL9535_REG_OUTPUT_P0));
    Serial.println("     >>> Relais 2 excite ?");
    delay(2000);

    Serial.println("\n  -> Tout OFF (0x00)");
    writeReg(XL9535_REG_OUTPUT_P0, 0x00);
    delay(1000);
}

// ── Setup / Loop ──────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║  Diagnostic logique relais XL9535      ║");
    Serial.println("║  SDA=27  SCL=22  addr=0x20             ║");
    Serial.println("╚═══════════════════════════════════════╝");

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    delay(50);

    // Scan I2C
    Serial.println("\n[I2C] Scan...");
    Wire.beginTransmission(XL9535_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.printf("ERREUR : XL9535 absent @ 0x%02X\n", XL9535_ADDR);
        while (true) delay(1000);
    }
    Serial.printf("[I2C] XL9535 trouve @ 0x%02X\n", XL9535_ADDR);

    dumpRegisters("AVANT INIT (etat reset HW)");

    // Test 1 : logique inverse
    testLogiqueInverse();

    // Pause
    Serial.println("\n--- Pause 2s avant test logique directe ---");
    delay(2000);

    // Test 2 : logique directe
    testLogiqueDirect();

    // Etat final : tout OFF dans les deux logiques (0x00 = direct OFF = inverse ON)
    // On laisse en directe OFF = 0x00 pour securite
    initXL9535(0x00);

    Serial.println("\n========================================");
    Serial.println("  DIAGNOSTIC TERMINE");
    Serial.println("  Commandes loop (Serial Monitor) :");
    Serial.println("  '1' -> Z1 ON  (logique inverse)");
    Serial.println("  '2' -> Z2 ON  (logique inverse)");
    Serial.println("  'q' -> Tout OFF (0xFF inverse)");
    Serial.println("  'a' -> Z1 ON  (logique directe)");
    Serial.println("  'b' -> Z2 ON  (logique directe)");
    Serial.println("  'z' -> Tout OFF (0x00 directe)");
    Serial.println("  'd' -> Dump registres");
    Serial.println("========================================\n");

    // Etat initial loop : tout OFF logique inverse
    initXL9535(0xFF);
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        switch (c) {
            case '1':
                writeReg(XL9535_REG_OUTPUT_P0, 0xFE);
                Serial.printf("[INV] Z1 ON  -> OUTPUT=0x%02X\n",
                              readReg(XL9535_REG_OUTPUT_P0));
                break;
            case '2':
                writeReg(XL9535_REG_OUTPUT_P0, 0xFD);
                Serial.printf("[INV] Z2 ON  -> OUTPUT=0x%02X\n",
                              readReg(XL9535_REG_OUTPUT_P0));
                break;
            case 'q':
                writeReg(XL9535_REG_OUTPUT_P0, 0xFF);
                Serial.printf("[INV] Tout OFF -> OUTPUT=0x%02X\n",
                              readReg(XL9535_REG_OUTPUT_P0));
                break;
            case 'a':
                writeReg(XL9535_REG_OUTPUT_P0, 0x01);
                Serial.printf("[DIR] Z1 ON  -> OUTPUT=0x%02X\n",
                              readReg(XL9535_REG_OUTPUT_P0));
                break;
            case 'b':
                writeReg(XL9535_REG_OUTPUT_P0, 0x02);
                Serial.printf("[DIR] Z2 ON  -> OUTPUT=0x%02X\n",
                              readReg(XL9535_REG_OUTPUT_P0));
                break;
            case 'z':
                writeReg(XL9535_REG_OUTPUT_P0, 0x00);
                Serial.printf("[DIR] Tout OFF -> OUTPUT=0x%02X\n",
                              readReg(XL9535_REG_OUTPUT_P0));
                break;
            case 'd':
                dumpRegisters("etat actuel");
                break;
        }
    }
}
