#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

class ConfigManager;

// XL9535
#define XL9535_REG_OUTPUT_P0  0x02
#define XL9535_REG_OUTPUT_P1  0x03
#define XL9535_REG_CONFIG_P0  0x06
#define XL9535_REG_CONFIG_P1  0x07

// MCP23017 en mode BANK=0 (défaut après reset)
#define MCP23017_REG_IODIRA   0x00
#define MCP23017_REG_IODIRB   0x01
#define MCP23017_REG_OLATA    0x14
#define MCP23017_REG_OLATB    0x15
#define MCP23017_ADDR         0x20

class RelaisManager {
public:
    void begin(ConfigManager* config = nullptr);
    void update();
    void setRelay(uint8_t relay, bool state);
    bool getState(uint8_t relay) const;

private:
    ConfigManager* _config = nullptr;
    bool     _state[MAX_ZONES]   = {};
    uint32_t _startMs[MAX_ZONES] = {};
    uint8_t  _regP0 = 0xFF;
    uint8_t  _regP1 = 0xFF;
    bool     _hardwareReady = false;

    uint8_t controller() const;
    uint8_t i2cAddress() const;
    bool initHardware();
    void applyHardware();
    bool writeReg(uint8_t reg, uint8_t val);
    uint8_t readReg(uint8_t reg);
    uint8_t nbRelaisPhysical() const;
    uint32_t maxWateringMs() const;
};
