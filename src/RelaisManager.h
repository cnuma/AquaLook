#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "RelayTopology.h"
#include "domain/I2cExpanderSharedOutputState.h"

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

    void setI2cExpanderSharedOutputState(
        AquaLook::Domain::I2cExpanderSharedOutputState* sharedOutputState
    );

    void mirrorZoneState(uint8_t zone, bool state, uint32_t nowMs) {
        if (zone >= MAX_ZONES) return;
        _state[zone] = state;
        _startMs[zone] = state ? nowMs : 0U;
    }

    // Pilotage direct : etage I2C du moteur historique, absent du firmware V4.
    // Le declarer sous garde fait echouer la COMPILATION si un appelant
    // subsiste en V4 -- une preuve, la ou une relecture n'est qu'une opinion.
    bool getState(uint8_t relay) const;
    bool getAssignmentState(uint8_t assignmentIndex) const;
    const RelayTopology::RelayTopologyConfig& topology() const;
    // true si le cablage en vigueur vient de la NVS. false signifie qu'AUCUN
    // cablage n'est enregistre : la topologie est alors vide et le module ne
    // pilote rien -- il n'y a plus de deduction de secours.
    bool topologyFromStore() const { return _topologyFromStore; }
    // true si au moins une carte valide est declaree. C'est la question a
    // poser avant d'annoncer un defaut materiel ou une zone injoignable.
    bool isWired() const { return RelayTopology::isWired(_topology); }
    // true si CETTE zone est raccordee a une voie physique. Distingue une
    // zone simplement pas encore affectee -- un etat de configuration -- d'une
    // sortie qui refuse de repondre, qui est une panne.
    bool zoneHasOutput(uint8_t zone) const;
    // Zones ayant depasse la duree maximale d'arrosage et qu'il faut couper.
    // Lue et remise a zero par l'appelant, qui coupe par le chemin de
    // pilotage normal -- voir RelaisManager::update().
    uint16_t consumeSafetyCutMask() {
        const uint16_t mask = _safetyCutMask;
        _safetyCutMask = 0U;
        return mask;
    }

private:
    ConfigManager* _config = nullptr;
    uint16_t _safetyCutMask = 0U;
    bool _state[MAX_ZONES] = {};
    uint32_t _startMs[MAX_ZONES] = {};
    bool _assignmentState[RelayTopology::MAX_RELAY_ASSIGNMENTS] = {};

    RelayTopology::RelayTopologyConfig _topology;
    bool _topologyFromStore = false;
    uint8_t _regP0[RelayTopology::MAX_RELAY_BOARDS] = {};
    uint8_t _regP1[RelayTopology::MAX_RELAY_BOARDS] = {};
    bool _boardReady[RelayTopology::MAX_RELAY_BOARDS] = {};
    bool _hardwareReady = false;
    AquaLook::Domain::I2cExpanderSharedOutputState* _sharedOutputState = nullptr;

    void buildRuntimeTopology();
    int16_t findZoneAssignment(uint8_t zone, uint8_t nbZones) const;
    uint8_t nbRelaisPhysical() const;
    uint32_t maxWateringMs() const;
};
