#include "RelaisManager.h"
#include "ConfigManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"

void RelaisManager::begin(ConfigManager* config) {
    _config = config;
    const bool inv = (_config && _config->relayLogic() == 0);
    _regP0 = inv ? 0xFF : 0x00;
    _regP1 = inv ? 0xFF : 0x00;

    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        _state[i] = false;
        _startMs[i] = 0;
    }

    _hardwareReady = initHardware();
    FaultManager::setActive(FaultId::RELAY_I2C, !_hardwareReady);

    const char* name =
        controller() == RELAY_CONTROLLER_MCP23017 ? "MCP23017" : "XL9535";

    if (_hardwareReady) {
        EventLog::log(
            LOG_INFO,
            "Relais: %s 0x%02X, %u relais, logique=%s, init OK",
            name, i2cAddress(), nbRelaisPhysical(),
            inv ? "inverse" : "directe"
        );
    } else {
        EventLog::log(
            LOG_ERROR,
            "Relais: %s 0x%02X absent ou en erreur",
            name, i2cAddress()
        );
    }
}

bool RelaisManager::initHardware() {
    if (controller() == RELAY_CONTROLLER_MCP23017) {
        if (!writeReg(MCP23017_REG_OLATA, _regP0)) return false;
        if (!writeReg(MCP23017_REG_OLATB, _regP1)) return false;
        if (!writeReg(MCP23017_REG_IODIRA, 0x00)) return false;
        if (!writeReg(MCP23017_REG_IODIRB, 0x00)) return false;
        return writeReg(MCP23017_REG_OLATA, _regP0) &&
               writeReg(MCP23017_REG_OLATB, _regP1);
    }

    if (!writeReg(XL9535_REG_OUTPUT_P0, _regP0)) return false;
    if (!writeReg(XL9535_REG_OUTPUT_P1, _regP1)) return false;
    if (!writeReg(XL9535_REG_CONFIG_P0, 0x00)) return false;
    if (!writeReg(XL9535_REG_CONFIG_P1, 0x00)) return false;
    return writeReg(XL9535_REG_OUTPUT_P0, _regP0) &&
           writeReg(XL9535_REG_OUTPUT_P1, _regP1);
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

    if (relay < nbRelaisPhysical()) {
        const bool inv = (_config && _config->relayLogic() == 0);
        uint8_t& reg = relay < 8 ? _regP0 : _regP1;
        const uint8_t bit = relay < 8 ? relay : relay - 8;
        const bool physicalHigh = inv ? !state : state;

        if (physicalHigh) reg |= (uint8_t)(1U << bit);
        else reg &= (uint8_t)~(1U << bit);

        if (_hardwareReady) {
            _hardwareReady = applyHardware();
            FaultManager::setActive(
                FaultId::RELAY_I2C,
                !_hardwareReady
            );
        } else {
            EventLog::log(
                LOG_ERROR,
                "Relais: commande zone %u impossible, controleur absent",
                relay + 1
            );
        }

        EventLog::log(
            LOG_INFO,
            "Relais: zone %u HW %s, controleur=%s, logique=%s",
            relay + 1,
            state ? "ON" : "OFF",
            controller() == RELAY_CONTROLLER_MCP23017
                ? "MCP23017" : "XL9535",
            inv ? "inverse" : "directe"
        );
    } else {
        EventLog::log(
            LOG_INFO,
            "Relais: zone logique %u %s",
            relay + 1, state ? "ON" : "OFF"
        );
    }

    EventBus::displayDirty = true;
}

bool RelaisManager::getState(uint8_t relay) const {
    return relay < MAX_ZONES ? _state[relay] : false;
}

bool RelaisManager::applyHardware() {
    bool ok = true;

    if (controller() == RELAY_CONTROLLER_MCP23017) {
        ok = writeReg(MCP23017_REG_OLATA, _regP0);
        if (ok && nbRelaisPhysical() > 8)
            ok = writeReg(MCP23017_REG_OLATB, _regP1);
    } else {
        ok = writeReg(XL9535_REG_OUTPUT_P0, _regP0);
        if (ok && nbRelaisPhysical() > 8)
            ok = writeReg(XL9535_REG_OUTPUT_P1, _regP1);
    }

    return ok;
}

bool RelaisManager::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(i2cAddress());
    Wire.write(reg);
    Wire.write(val);
    const uint8_t err = Wire.endTransmission();

    if (err != 0) {
        FaultManager::setActive(FaultId::RELAY_I2C, true);
        EventLog::log(
            LOG_ERROR,
            "Relais: erreur I2C addr=0x%02X reg=0x%02X err=%u",
            i2cAddress(), reg, err
        );
    }

    return err == 0;
}

uint8_t RelaisManager::readReg(uint8_t reg) {
    Wire.beginTransmission(i2cAddress());
    Wire.write(reg);

    const uint8_t err = Wire.endTransmission(false);
    if (err != 0) {
        FaultManager::setActive(FaultId::RELAY_I2C, true);
        EventLog::log(
            LOG_ERROR,
            "Relais: lecture I2C impossible reg=0x%02X err=%u",
            reg, err
        );
        return 0xFF;
    }

    Wire.requestFrom(i2cAddress(), (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

uint8_t RelaisManager::controller() const {
    return _config
        ? _config->relayController()
        : RELAY_CONTROLLER_XL9535;
}

uint8_t RelaisManager::i2cAddress() const {
    return controller() == RELAY_CONTROLLER_MCP23017
        ? MCP23017_ADDR : XL9535_ADDR;
}

uint8_t RelaisManager::nbRelaisPhysical() const {
    return _config ? _config->nbRelais() : NB_ZONES;
}

uint32_t RelaisManager::maxWateringMs() const {
    return _config
        ? (uint32_t)_config->system().maxWateringMin * 60000UL
        : MAX_WATERING_DURATION_MS;
}
