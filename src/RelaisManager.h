#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "RelayTopology.h"

class ConfigManager;

#define XL9535_REG_OUTPUT_P0  0x02
#define XL9535_REG_OUTPUT_P1  0x03
#define XL9535_REG_CONFIG_P0  0x06
#define XL9535_REG_CONFIG_P1  0x07

#define MCP23017_REG_IODIRA   0x00
#define MCP23017_REG_IODIRB   0x01
#define MCP23017_REG_OLATA    0x14
#define MCP23017_REG_OLATB    0x15
#define MCP23017_ADDR         0x20

class RelaisManager {
public:
    void begin(ConfigManager* config = nullptr);
    void update();

    // API historique conservée : l'index reste un index de zone.
    void setRelay(uint8_t relay, bool state);
    bool getState(uint8_t relay) const;

    // API matérielle générique : exécute une RelayAssignment validée.
    bool setAssignment(uint8_t assignmentIndex, bool state);
    bool getAssignmentState(uint8_t assignmentIndex) const;
    const RelayTopology::RelayTopologyConfig& topology() const;

private:
    ConfigManager* _config = nullptr;
    bool _state[MAX_ZONES] = {};
    uint32_t _startMs[MAX_ZONES] = {};
    bool _assignmentState[RelayTopology::MAX_RELAY_ASSIGNMENTS] = {};

    RelayTopology::RelayTopologyConfig _topology;
    uint8_t _regP0[RelayTopology::MAX_RELAY_BOARDS] = {};
    uint8_t _regP1[RelayTopology::MAX_RELAY_BOARDS] = {};
    bool _boardReady[RelayTopology::MAX_RELAY_BOARDS] = {};
    bool _hardwareReady = false;

    void buildRuntimeTopology();
    bool initHardware();
    bool initBoard(uint8_t boardIndex);
    bool applyBoard(uint8_t boardIndex);
    bool writeReg(uint8_t addr, uint8_t reg, uint8_t val);
    uint8_t readReg(uint8_t addr, uint8_t reg);
    int16_t findZoneAssignment(uint8_t zone, uint8_t nbZones) const;
    uint8_t nbRelaisPhysical() const;
    uint32_t maxWateringMs() const;
};
