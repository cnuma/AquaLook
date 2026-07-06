#include "RelaisManager.h"
#include "ConfigManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"

void RelaisManager::begin(ConfigManager* config) {
    _config = config;
    buildRuntimeTopology();

    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        _state[i] = false;
        _startMs[i] = 0;
    }

    for (uint8_t b = 0; b < RelayTopology::MAX_RELAY_BOARDS; b++) {
        const RelayTopology::RelayBoardConfig& board = _topology.boards[b];
        const bool inv = (board.logic == RelayTopology::LOGIC_INVERTED);
        _regP0[b] = inv ? 0xFF : 0x00;
        _regP1[b] = inv ? 0xFF : 0x00;
        _boardReady[b] = false;
    }

    _hardwareReady = initHardware();
    FaultManager::setActive(FaultId::RELAY_I2C, !_hardwareReady);

    if (_hardwareReady) {
        EventLog::log(
            LOG_INFO,
            "Relais: topologie init OK, cartes=%u, canaux=%u",
            RelayTopology::MAX_RELAY_BOARDS,
            RelayTopology::totalEnabledChannels(_topology)
        );
    } else {
        EventLog::log(
            LOG_ERROR,
            "Relais: aucune carte relais I2C initialisee"
        );
    }
}

void RelaisManager::buildRuntimeTopology() {
    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;
    const uint8_t nbR = _config ? _config->nbRelais() : NB_ZONES;
    const uint8_t controller = _config
        ? _config->relayController()
        : RelayTopology::CONTROLLER_XL9535;
    const uint8_t logic = _config
        ? _config->relayLogic()
        : RelayTopology::LOGIC_DIRECT;

    RelayTopology::buildLegacyCompatibleTopology(
        _topology,
        nbZ,
        nbR,
        controller,
        logic
    );

    if (RelayTopology::hasDuplicateMappings(_topology, nbZ)) {
        EventLog::log(LOG_ERROR, "Relais: topologie invalide, doublon de mapping");
    }

    const RelayTopology::RelayBoardConfig& b0 = _topology.boards[0];
    EventLog::log(
        LOG_INFO,
        "Relais: topologie legacy, carte0=%s 0x%02X, voies=%u, logique=%s",
        RelayTopology::controllerName(b0.controller),
        b0.i2cAddress,
        b0.channelCount,
        b0.logic == RelayTopology::LOGIC_INVERTED ? "inverse" : "directe"
    );
}

bool RelaisManager::initHardware() {
    bool anyReady = false;

    for (uint8_t b = 0; b < RelayTopology::MAX_RELAY_BOARDS; b++) {
        if (!RelayTopology::validateBoard(_topology.boards[b])) continue;

        _boardReady[b] = initBoard(b);
        anyReady = anyReady || _boardReady[b];

        const RelayTopology::RelayBoardConfig& board = _topology.boards[b];
        EventLog::log(
            _boardReady[b] ? LOG_INFO : LOG_ERROR,
            "Relais: carte %u %s 0x%02X voies=%u %s",
            b,
            RelayTopology::controllerName(board.controller),
            board.i2cAddress,
            board.channelCount,
            _boardReady[b] ? "OK" : "absente ou erreur"
        );
    }

    return anyReady;
}

bool RelaisManager::initBoard(uint8_t boardIndex) {
    if (boardIndex >= RelayTopology::MAX_RELAY_BOARDS) return false;
    const RelayTopology::RelayBoardConfig& board = _topology.boards[boardIndex];
    if (!RelayTopology::validateBoard(board)) return false;

    if (board.controller == RelayTopology::CONTROLLER_MCP23017) {
        if (!writeReg(board.i2cAddress, MCP23017_REG_OLATA, _regP0[boardIndex])) return false;
        if (!writeReg(board.i2cAddress, MCP23017_REG_OLATB, _regP1[boardIndex])) return false;
        if (!writeReg(board.i2cAddress, MCP23017_REG_IODIRA, 0x00)) return false;
        if (!writeReg(board.i2cAddress, MCP23017_REG_IODIRB, 0x00)) return false;
        return writeReg(board.i2cAddress, MCP23017_REG_OLATA, _regP0[boardIndex]) &&
               writeReg(board.i2cAddress, MCP23017_REG_OLATB, _regP1[boardIndex]);
    }

    if (!writeReg(board.i2cAddress, XL9535_REG_OUTPUT_P0, _regP0[boardIndex])) return false;
    if (!writeReg(board.i2cAddress, XL9535_REG_OUTPUT_P1, _regP1[boardIndex])) return false;
    if (!writeReg(board.i2cAddress, XL9535_REG_CONFIG_P0, 0x00)) return false;
    if (!writeReg(board.i2cAddress, XL9535_REG_CONFIG_P1, 0x00)) return false;
    return writeReg(board.i2cAddress, XL9535_REG_OUTPUT_P0, _regP0[boardIndex]) &&
           writeReg(board.i2cAddress, XL9535_REG_OUTPUT_P1, _regP1[boardIndex]);
}

void RelaisManager::update() {
    const uint32_t now = millis();
    const uint32_t maxMs = maxWateringMs();
    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;

    for (uint8_t i = 0; i < nbZ; i++) {
        if (_state[i] && _startMs[i] > 0 &&
            now - _startMs[i] >= maxMs) {
            EventLog::log(
                LOG_ERROR,
                "Relais: securite, zone %u coupee apres %lus",
                i + 1, maxMs / 1000UL
            );
            setRelay(i, false);
        }
    }
}

void RelaisManager::setRelay(uint8_t relay, bool state) {
    if (relay >= MAX_ZONES) {
        EventLog::log(LOG_ERROR, "Relais: index zone invalide %u", relay);
        return;
    }

    _state[relay] = state;
    _startMs[relay] = state ? millis() : 0;

    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;
    const RelayTopology::MappingResolution mapping =
        RelayTopology::resolveMapping(_topology, relay, nbZ);

    if (!mapping.valid) {
        EventLog::log(
            LOG_ERROR,
            "Relais: zone %u sans mapping materiel valide (%s logique)",
            relay + 1,
            state ? "ON" : "OFF"
        );
        EventBus::displayDirty = true;
        return;
    }

    if (!_boardReady[mapping.boardIndex]) {
        EventLog::log(
            LOG_ERROR,
            "Relais: commande zone %u impossible, carte %u absente",
            relay + 1,
            mapping.boardIndex
        );
        FaultManager::setActive(FaultId::RELAY_I2C, true);
        EventBus::displayDirty = true;
        return;
    }

    uint8_t& reg = mapping.channelIndex < 8
        ? _regP0[mapping.boardIndex]
        : _regP1[mapping.boardIndex];
    const uint8_t bit = mapping.channelIndex < 8
        ? mapping.channelIndex
        : mapping.channelIndex - 8;
    const bool inv = (mapping.logic == RelayTopology::LOGIC_INVERTED);
    const bool physicalHigh = inv ? !state : state;

    if (physicalHigh) reg |= (uint8_t)(1U << bit);
    else reg &= (uint8_t)~(1U << bit);

    const bool ok = applyBoard(mapping.boardIndex);
    _boardReady[mapping.boardIndex] = ok;
    _hardwareReady = ok;
    FaultManager::setActive(FaultId::RELAY_I2C, !ok);

    EventLog::log(
        ok ? LOG_INFO : LOG_ERROR,
        "Relais: zone %u %s -> carte %u %s 0x%02X voie %u logique=%s",
        relay + 1,
        state ? "ON" : "OFF",
        mapping.boardIndex,
        RelayTopology::controllerName(mapping.controller),
        mapping.i2cAddress,
        mapping.channelIndex + 1,
        inv ? "inverse" : "directe"
    );

    EventBus::displayDirty = true;
}

bool RelaisManager::getState(uint8_t relay) const {
    return relay < MAX_ZONES ? _state[relay] : false;
}

bool RelaisManager::applyBoard(uint8_t boardIndex) {
    if (boardIndex >= RelayTopology::MAX_RELAY_BOARDS) return false;
    const RelayTopology::RelayBoardConfig& board = _topology.boards[boardIndex];
    if (!RelayTopology::validateBoard(board)) return false;

    bool ok = true;

    if (board.controller == RelayTopology::CONTROLLER_MCP23017) {
        ok = writeReg(board.i2cAddress, MCP23017_REG_OLATA, _regP0[boardIndex]);
        if (ok && board.channelCount > 8)
            ok = writeReg(board.i2cAddress, MCP23017_REG_OLATB, _regP1[boardIndex]);
    } else {
        ok = writeReg(board.i2cAddress, XL9535_REG_OUTPUT_P0, _regP0[boardIndex]);
        if (ok && board.channelCount > 8)
            ok = writeReg(board.i2cAddress, XL9535_REG_OUTPUT_P1, _regP1[boardIndex]);
    }

    return ok;
}

bool RelaisManager::writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    const uint8_t err = Wire.endTransmission();

    if (err != 0) {
        FaultManager::setActive(FaultId::RELAY_I2C, true);
        EventLog::log(
            LOG_ERROR,
            "Relais: erreur I2C addr=0x%02X reg=0x%02X err=%u",
            addr, reg, err
        );
    }

    return err == 0;
}

uint8_t RelaisManager::readReg(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);

    const uint8_t err = Wire.endTransmission(false);
    if (err != 0) {
        FaultManager::setActive(FaultId::RELAY_I2C, true);
        EventLog::log(
            LOG_ERROR,
            "Relais: lecture I2C impossible addr=0x%02X reg=0x%02X err=%u",
            addr, reg, err
        );
        return 0xFF;
    }

    Wire.requestFrom(addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

uint8_t RelaisManager::nbRelaisPhysical() const {
    return _config ? _config->nbRelais() : NB_ZONES;
}

uint32_t RelaisManager::maxWateringMs() const {
    return _config
        ? (uint32_t)_config->system().maxWateringMin * 60000UL
        : MAX_WATERING_DURATION_MS;
}
