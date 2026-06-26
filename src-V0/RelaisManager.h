#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

#define REG_OUTPUT_P0  0x02
#define REG_OUTPUT_P1  0x03
#define REG_CONFIG_P0  0x06
#define REG_CONFIG_P1  0x07

class RelaisManager {
public:
    void begin();
    void setRelay(uint8_t relay, bool state);
    bool getState(uint8_t relay);
    void update();

private:
    bool     _state[NB_ZONES]     = {false};
    uint32_t _startTime[NB_ZONES] = {0};

    void    writeReg(uint8_t reg, uint8_t val);
    uint8_t readReg(uint8_t reg);
};