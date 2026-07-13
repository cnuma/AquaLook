#include "RelaisManager.h"
#include "ConfigManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"

void RelaisManager::setXl9535SharedOutputState(
    AquaLook::Domain::Xl9535SharedOutputState* sharedOutputState
) {
    _xl9535SharedOutputState = sharedOutputState;
}

void RelaisManager::begin(ConfigManager* config) {
    _config = config;
    buildRuntimeTopology();

    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        _state[i] = false;
        _startMs[i] = 0;
    }

    for (uint8_t a = 0; a < RelayTopology::MAX_RELAY_ASSIGNMENTS; a++) {
        _assignmentState[a] = false;
    }

    for (uint8_t b = 0; b < RelayTopology::MAX_RELAY_BOARDS; b++) {
        const RelayTopology::RelayBoardConfig& board = _topology.boards[b];
        const bool inv = (board.logic == RelayTopology::LOGIC_INVERTED);
        _regP0[b] = inv ? 0xFF : 0x00;
        _regP1[b] = inv ? 0xFF : 0x00;
        _boardReady[b] = false;

        if (_xl9535SharedOutputState &&
            RelayTopology::validateBoard(board) &&
            board.controller == RelayTopology::CONTROLLER_XL9535) {
            const uint16_t value = static_cast<uint16_t>(_regP0[b]) |
                static_cast<uint16_t>(static_cast<uint16_t>(_regP1[b]) << 8U);
            _xl9535SharedOutputState->seed(board.i2cAddress, value);
        }
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

int16_t RelaisManager::findZoneAssignment(uint8_t zone, uint8_t nbZones) const {
    if (zone >= nbZones || zone >= MAX_ZONES) return -1;

    for (uint8_t a = 0; a < RelayTopology::MAX_RELAY_ASSIGNMENTS; a++) {
        if (!RelayTopology::validateAssignment(_topology, a)) continue;
        const RelayTopology::RelayAssignment& assignment = _topology.assignments[a];
        if (assignment.role == RelayTopology::ROLE_ZONE_VALVE &&
            assignment.targetIndex == zone) {
            return a;
        }
    }

    return -1;
}

void RelaisManager::setRelay(uint8_t relay, bool state) {
    if (relay >= MAX_ZONES) {
        EventLog::log(LOG_ERROR, "Relais: index zone invalide %u", relay);
        return;
    }

    _state[relay] = state;
    _startMs[relay] = state ? millis() : 0;

    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;
    const int16_t assignmentIndex = findZoneAssignment(relay, nbZ);

    if (assignmentIndex < 0) {
        EventLog::log(
            LOG_ERROR,
            "Relais: zone %u sans mapping materiel valide (%s logique)",
            relay + 1,
            state ? "ON" : "OFF"
        );
        EventBus::displayDirty = true;
        return;
    }

    setAssignment(static_cast<uint8_t>(assignmentIndex), state);
}

bool RelaisManager::setAssignment(uint8_t assignmentIndex, bool state) {
    const RelayTopology::MappingResolution mapping =
        RelayTopology::resolveAssignment(_topology, assignmentIndex);

    if (!mapping.valid) {
        EventLog::log(
            LOG_ERROR,
            "Relais: affectation %u invalide (%s logique)",
            assignmentIndex,
            state ? "ON" : "OFF"
        );
        EventBus::displayDirty = true;
        return false;
    }

    if (!_boardReady[mapping.boardIndex]) {
        EventLog::log(
            LOG_ERROR,
            "Relais: commande %s[%u] impossible, carte %u absente",
            RelayTopology::roleName(mapping.role),
            mapping.targetIndex,
            mapping.boardIndex
        );
        FaultManager::setActive(FaultId::RELAY_I2C, true);
        EventBus::displayDirty = true;
        return false;
    }

    const bool inv = (mapping.logic == RelayTopology::LOGIC_INVERTED);
    const bool physicalHigh = inv ? !state : state;

    if (mapping.controller == RelayTopology::CONTROLLER_XL9535 &&
        _xl9535SharedOutputState) {
        uint16_t sharedValue = 0U;
        if (!_xl9535SharedOutputState->updateChannel(
                mapping.i2cAddress,
                mapping.channelIndex,
                physicalHigh,
                sharedValue)) {
            EventLog::log(
                LOG_ERROR,
                "Relais: etat partage XL9535 indisponible addr=0x%02X voie=%u",
                mapping.i2cAddress,
                mapping.channelIndex + 1U
            );
            FaultManager::setActive(FaultId::RELAY_I2C, true);
            EventBus::displayDirty = true;
            return false;
        }
        _regP0[mapping.boardIndex] = static_cast<uint8_t>(sharedValue & 0xFFU);
        _regP1[mapping.boardIndex] = static_cast<uint8_t>((sharedValue >> 8U) & 0xFFU);
    } else {
        uint8_t& reg = mapping.channelIndex < 8
            ? _regP0[mapping.boardIndex]
            : _regP1[mapping.boardIndex];
        const uint8_t bit = mapping.channelIndex < 8
            ? mapping.channelIndex
            : mapping.channelIndex - 8;

        if (physicalHigh) reg |= static_cast<uint8_t>(1U << bit);
        else reg &= static_cast<uint8_t>(~(1U << bit));
    }

    const bool ok = applyBoard(mapping.boardIndex);
    _boardReady[mapping.boardIndex] = ok;
    _hardwareReady = ok;
    FaultManager::setActive(FaultId::RELAY_I2C, !ok);

    if (ok) {
        _assignmentState[assignmentIndex] = state;
    }

    EventLog::log(
        ok ? LOG_INFO : LOG_ERROR,
        "Relais: %s[%u] %s -> carte %u %s 0x%02X voie %u logique=%s",
        RelayTopology::roleName(mapping.role),
        mapping.targetIndex,
        state ? "ON" : "OFF",
        mapping.boardIndex,
        RelayTopology::controllerName(mapping.controller),
        mapping.i2cAddress,
        mapping.channelIndex + 1,
        inv ? "inverse" : "directe"
    );

    // Une transition ON/OFF reussie est deja propagee par le callback runtime.
    // Ne pas forcer ici un redraw complet du TFT. Les erreurs conservent leurs
    // displayDirty plus haut dans la fonction.
    if (!ok) EventBus::displayDirty = true;
    return ok;
}

bool RelaisManager::getState(uint8_t relay) const {
    return relay < MAX_ZONES ? _state[relay] : false;
}

bool RelaisManager::getAssignmentState(uint8_t assignmentIndex) const {
    return assignmentIndex < RelayTopology::MAX_RELAY_ASSIGNMENTS
        ? _assignmentState[assignmentIndex]
        : false;
}

const RelayTopology::RelayTopologyConfig& RelaisManager::topology() const {
    return _topology;
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
        if (_xl9535SharedOutputState) {
            uint16_t sharedValue = 0U;
            if (!_xl9535SharedOutputState->read(board.i2cAddress, sharedValue)) {
                return false;
            }
            _regP0[boardIndex] = static_cast<uint8_t>(sharedValue & 0xFFU);
            _regP1[boardIndex] = static_cast<uint8_t>((sharedValue >> 8U) & 0xFFU);
        }
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

    Wire.requestFrom(addr, static_cast<uint8_t>(1));
    return Wire.available() ? Wire.read() : 0xFF;
}

uint8_t RelaisManager::nbRelaisPhysical() const {
    return _config ? _config->nbRelais() : NB_ZONES;
}

uint32_t RelaisManager::maxWateringMs() const {
    return _config
        ? static_cast<uint32_t>(_config->system().maxWateringMin) * 60000UL
        : MAX_WATERING_DURATION_MS;
}
