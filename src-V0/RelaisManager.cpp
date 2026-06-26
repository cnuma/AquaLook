#include "RelaisManager.h"

void RelaisManager::begin() {
    Serial.println("[Relais] debut begin");
    writeReg(REG_CONFIG_P0, 0x00);
    Serial.println("[Relais] CONFIG_P0 ok");
    writeReg(REG_CONFIG_P1, 0x00);
    Serial.println("[Relais] CONFIG_P1 ok");
    writeReg(REG_OUTPUT_P0, 0xFF);
    Serial.println("[Relais] OUTPUT_P0 ok");
    Serial.println("[Relais] Initialisé");
}

void RelaisManager::setRelay(uint8_t relay, bool state) {
    if (relay >= NB_ZONES) return;
    uint8_t current = readReg(REG_OUTPUT_P0);
    if (state) {
        current &= ~(1 << relay);
        _startTime[relay] = millis();
    } else {
        current |= (1 << relay);
        _startTime[relay] = 0;
    }
    writeReg(REG_OUTPUT_P0, current);
    _state[relay] = state;

    // Fix ZONE_NAMES — tableau local pour éviter le crash
    const char* names[NB_ZONES] = {ZONE_NAMES_0, ZONE_NAMES_1};
    Serial.printf("[Relais] Zone %d (%s) : %s\n",
        relay + 1, names[relay], state ? "ON" : "OFF");
}

bool RelaisManager::getState(uint8_t relay) {
    if (relay >= NB_ZONES) return false;
    return _state[relay];
}

void RelaisManager::update() {
    for (uint8_t i = 0; i < NB_ZONES; i++) {
        if (_state[i] && _startTime[i] > 0) {
            if (millis() - _startTime[i] >= MAX_WATERING_DURATION) {
                Serial.printf("[Relais] SECURITE — Zone %d coupée\n", i + 1);
                setRelay(i, false);
            }
        }
    }
}

void RelaisManager::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

uint8_t RelaisManager::readReg(uint8_t reg) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(XL9535_ADDR, 1);
    return Wire.available() ? Wire.read() : 0xFF;
}