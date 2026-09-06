#include "V4RelayPhysicalBackend.h"

#include "EventLog.h"
#include "FaultManager.h"

namespace AquaLook { namespace Runtime {

void V4RelayPhysicalBackend::bind(
    const RelayTopology::RelayTopologyConfig* topology,
    const Domain::ControllerDefinition* controllers,
    size_t controllerCount,
    const Domain::BoardDefinition* boards,
    size_t boardCount,
    const Domain::PortDefinition* ports,
    size_t portCount,
    Domain::BinaryActuatorDriverRegistry* driverRegistry
) {
    _topology = topology;
    _controllers = controllers;
    _controllerCount = controllers ? controllerCount : 0U;
    _boards = boards;
    _boardCount = boards ? boardCount : 0U;
    _ports = ports;
    _portCount = ports ? portCount : 0U;
    _driverRegistry = driverRegistry;

    for (uint8_t zoneIndex = 0U; zoneIndex < MAX_ZONES; ++zoneIndex) {
        _sessions[zoneIndex] = Domain::BinaryActuatorSession();
    }
}

bool V4RelayPhysicalBackend::isReady() const {
    return _topology != nullptr &&
           _controllers != nullptr && _controllerCount > 0U &&
           _boards != nullptr && _boardCount > 0U &&
           _ports != nullptr && _portCount > 0U &&
           _driverRegistry != nullptr && !_driverRegistry->empty();
}

// Un echec de pilotage doit s entendre. On n emet qu au CHANGEMENT d etat :
// un defaut persistant noierait le journal circulaire, et le retablissement
// doit rester lisible.
void V4RelayPhysicalBackend::reportFailure(uint8_t zoneIndex, const char* etape) {
    FaultManager::setActive(FaultId::RELAY_I2C, true);
    if (!_faultRaised) {
        _faultRaised = true;
        EventLog::log(LOG_ERROR, "Relais V4: echec %s zone=%u", etape, zoneIndex + 1U);
    }
}

void V4RelayPhysicalBackend::reportSuccess() {
    if (_faultRaised) {
        _faultRaised = false;
        EventLog::log(LOG_INFO, "Relais V4: pilotage retabli");
    }
    FaultManager::setActive(FaultId::RELAY_I2C, false);
}

bool V4RelayPhysicalBackend::setZoneValve(
    uint8_t zoneIndex,
    bool active,
    uint32_t nowMs
) {
    (void)nowMs;

    if (zoneIndex >= MAX_ZONES || !isZoneMigrated(zoneIndex) || !isReady()) {
        return false;
    }

    ResolvedZoneTarget target;
    if (!resolveZoneTarget(zoneIndex, target)) {
        reportFailure(zoneIndex, "resolution");
        return false;
    }

    Domain::BinaryActuatorSession& session = _sessions[zoneIndex];
    if (session.configured == 0U) {
        const Domain::BinaryActuatorDriverResult configured =
            Domain::configureBinaryActuator(
                *target.driver,
                *target.controller,
                *target.port,
                session
            );
        if (!configured.succeeded()) {
            reportFailure(zoneIndex, "configuration");
            return false;
        }
    }

    const Domain::BinaryActuatorState requested = active
        ? Domain::BinaryActuatorState::ACTIVE
        : Domain::BinaryActuatorState::INACTIVE;

    const bool ok = Domain::commandBinaryActuator(
        *target.driver,
        *target.port,
        requested,
        session
    ).succeeded();

    if (ok) {
        reportSuccess();
    } else {
        reportFailure(zoneIndex, "commande");
    }
    return ok;
}

bool V4RelayPhysicalBackend::getZoneValveState(
    uint8_t zoneIndex,
    bool& active
) const {
    active = false;

    if (zoneIndex >= MAX_ZONES || !isZoneMigrated(zoneIndex) || !isReady()) {
        return false;
    }

    ResolvedZoneTarget target;
    if (!resolveZoneTarget(zoneIndex, target)) {
        return false;
    }

    Domain::BinaryActuatorSession& session = _sessions[zoneIndex];
    if (session.configured == 0U) {
        const Domain::BinaryActuatorDriverResult configured =
            Domain::configureBinaryActuator(
                *target.driver,
                *target.controller,
                *target.port,
                session
            );
        if (!configured.succeeded()) {
            return false;
        }
    }

    const Domain::BinaryActuatorDriverResult result =
        Domain::readBinaryActuator(*target.driver, *target.port, session);

    if (!result.succeeded()) {
        return false;
    }

    if (result.state == Domain::BinaryActuatorState::ACTIVE) {
        active = true;
        return true;
    }

    if (result.state == Domain::BinaryActuatorState::INACTIVE) {
        active = false;
        return true;
    }

    return false;
}

bool V4RelayPhysicalBackend::isZoneMigrated(uint8_t zoneIndex) const {
    if (zoneIndex >= MAX_ZONES || zoneIndex >= 32U) {
        return false;
    }

    const uint32_t zoneBit = static_cast<uint32_t>(1UL << zoneIndex);
    return (_migratedZoneMask & zoneBit) != 0U;
}

bool V4RelayPhysicalBackend::hasAnyMigratedZone() const {
    return _migratedZoneMask != 0U;
}

bool V4RelayPhysicalBackend::resolveZoneTarget(
    uint8_t zoneIndex,
    ResolvedZoneTarget& target
) const {
    target = ResolvedZoneTarget();

    if (!isReady() || zoneIndex >= MAX_ZONES) {
        return false;
    }

    const RelayTopology::MappingResolution mapping =
        RelayTopology::resolveZoneValve(*_topology, zoneIndex, MAX_ZONES);
    if (!mapping.valid) {
        return false;
    }

    target.board = findBoardByTopologyIndex(mapping.boardIndex);
    if (!target.board) {
        return false;
    }

    target.controller = findController(target.board->controllerId);
    if (!target.controller) {
        return false;
    }

    target.port = findPort(*target.board, mapping.channelIndex);
    if (!target.port) {
        return false;
    }

    target.driver = _driverRegistry->findByController(target.controller->id);
    return target.valid();
}

const Domain::ControllerDefinition* V4RelayPhysicalBackend::findController(
    Domain::ControllerId controllerId
) const {
    for (size_t index = 0U; index < _controllerCount; ++index) {
        if (_controllers[index].id == controllerId) {
            return &_controllers[index];
        }
    }
    return nullptr;
}

const Domain::BoardDefinition* V4RelayPhysicalBackend::findBoardByTopologyIndex(
    uint8_t boardIndex
) const {
    if (boardIndex >= _boardCount) {
        return nullptr;
    }
    return &_boards[boardIndex];
}

const Domain::PortDefinition* V4RelayPhysicalBackend::findPort(
    const Domain::BoardDefinition& board,
    uint8_t channelIndex
) const {
    const size_t firstPort = board.firstPortIndex;
    const size_t endPort = firstPort + board.portCount;

    if (firstPort >= _portCount || endPort > _portCount) {
        return nullptr;
    }

    for (size_t index = firstPort; index < endPort; ++index) {
        const Domain::PortDefinition& port = _ports[index];
        if (port.boardId == board.id && port.channel == channelIndex &&
            Domain::isBinaryOutputPort(port)) {
            return &port;
        }
    }

    return nullptr;
}

}} // namespace AquaLook::Runtime
