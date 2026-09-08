#include "EquipmentOutputRuntimeAdapter.h"

#include "EventLog.h"
#include "NotificationManager.h"
#include "RelaisManager.h"
#include "RelayPhysicalBackend.h"
#include "config.h"

namespace AquaLook { namespace Runtime {

void EquipmentOutputRuntimeAdapter::bind(RelaisManager* relayManager) {
    _relayManager = relayManager;
}

void EquipmentOutputRuntimeAdapter::setPhysicalBackend(
    RelayPhysicalBackend* physicalBackend
) {
    _physicalBackend = physicalBackend;
}

bool EquipmentOutputRuntimeAdapter::isBound() const {
    return _relayManager != nullptr || _physicalBackend != nullptr;
}

EquipmentOutputRuntimeAdapter::ExecutionPath
EquipmentOutputRuntimeAdapter::lastExecutionPath() const {
    return _lastExecutionPath;
}

const EquipmentOutputRuntimeAdapter::ExecutionCounters&
EquipmentOutputRuntimeAdapter::executionCounters() const {
    return _executionCounters;
}

void EquipmentOutputRuntimeAdapter::recordExecutionPath(ExecutionPath path) {
    _lastExecutionPath = path;
    switch (path) {
        case ExecutionPath::PHYSICAL_BACKEND:
            ++_executionCounters.physicalBackend;
            break;
        case ExecutionPath::FAILED:
            ++_executionCounters.failed;
            break;
        case ExecutionPath::NONE:
        default:
            break;
    }
}

const char* EquipmentOutputRuntimeAdapter::executionPathName(ExecutionPath path) {
    switch (path) {
        case ExecutionPath::PHYSICAL_BACKEND: return "physical_backend";
        case ExecutionPath::FAILED: return "failed";
        case ExecutionPath::NONE:
        default:
            return "none";
    }
}

Domain::ExecutionId EquipmentOutputRuntimeAdapter::nextExecutionId() {
    const uint16_t current = _nextExecutionValue;
    _nextExecutionValue = static_cast<uint16_t>(_nextExecutionValue + 1U);
    if (_nextExecutionValue == 0U || _nextExecutionValue == 0xFFFFU) {
        _nextExecutionValue = 1U;
    }
    return Domain::ExecutionId(current);
}

Domain::OperationResult EquipmentOutputRuntimeAdapter::rejected(
    Domain::EquipmentId equipmentId,
    Domain::OperationError error,
    uint32_t nowMs
) {
    Domain::OperationResult result;
    result.equipmentId = equipmentId;
    result.status = Domain::OperationStatus::REJECTED;
    result.stage = Domain::OperationStage::REQUEST;
    result.error = error;
    result.completedAtMs = nowMs;
    return result;
}

Domain::OperationResult EquipmentOutputRuntimeAdapter::command(
    const Domain::EquipmentOutputCommand& requested,
    uint32_t nowMs
) {
    if (requested.kind != Domain::EquipmentOutputKind::BINARY) {
        recordExecutionPath(ExecutionPath::FAILED);
        return rejected(
            Domain::EquipmentId(),
            Domain::OperationError::CAPABILITY_NOT_SUPPORTED,
            nowMs
        );
    }

    switch (requested.output.role) {
        case Domain::EquipmentOutputRole::ZONE_VALVE:
            return setZoneValve(requested.output.targetIndex, requested.active, nowMs);

        default:
            recordExecutionPath(ExecutionPath::FAILED);
            return rejected(
                Domain::EquipmentId(),
                Domain::OperationError::CAPABILITY_NOT_SUPPORTED,
                nowMs
            );
    }
}

Domain::OperationResult EquipmentOutputRuntimeAdapter::setZoneValve(
    uint8_t zoneIndex,
    bool active,
    uint32_t nowMs
) {
    const Domain::EquipmentId equipmentId =
        Domain::equipmentIdForZoneValve(zoneIndex);

    if (zoneIndex >= MAX_ZONES) {
        recordExecutionPath(ExecutionPath::FAILED);
        EventLog::log(
            LOG_ERROR,
            "Equipment: zone %u %s path=failed error=invalid_target",
            zoneIndex + 1U,
            active ? "ON" : "OFF"
        );
        return rejected(equipmentId, Domain::OperationError::INVALID_TARGET, nowMs);
    }

    bool applied = false;
    bool appliedByPhysicalBackend = false;
    const Domain::EquipmentStateValue previousState = getZoneValveState(zoneIndex);
    const bool previousActive =
        previousState.validity == Domain::StateValidity::VALID &&
        previousState.kind == Domain::StateValueKind::BINARY &&
        previousState.value != 0;

    if (_physicalBackend) {
        applied = _physicalBackend->setZoneValve(zoneIndex, active, nowMs);
        appliedByPhysicalBackend = applied;
    }

    // L'etat de zone reste tenu par RelaisManager : l'ecran, le Web, le
    // planificateur et l'expandeur d'E/S le lisent tous la. C'est du MODELE,
    // pas du pilotage -- rien n'y touche le bus I2C.
    if (appliedByPhysicalBackend && _relayManager) {
        _relayManager->mirrorZoneState(zoneIndex, active, nowMs);
    }

    // Profil V4 : AUCUN repli. Un echec de V4 doit se voir, pas se faire
    // rattraper en silence par le moteur historique.
    //
    // Ce repli avait cache pendant des semaines que deux cartes sur trois
    // n'etaient pas pilotees par V4 : tout fonctionnait, donc personne ne
    // cherchait. Un moteur qu'on ne peut pas prendre en defaut est un moteur
    // qu'on ne peut pas valider.
    if (appliedByPhysicalBackend) {
        recordExecutionPath(ExecutionPath::PHYSICAL_BACKEND);
    }

    if (!applied) {
        recordExecutionPath(ExecutionPath::FAILED);
        // Deux echecs de nature opposee arrivaient ici sous la meme etiquette
        // rouge : une zone qu'on n'a pas encore raccordee, et une sortie qui
        // refuse de repondre. Le premier est un etat de CONFIGURATION -- le
        // module se comporte correctement en ne pilotant rien -- le second est
        // une panne. Les confondre noyait les pannes sous un flot de lignes
        // ERR repetees a chaque creneau.
        const bool unmapped = _relayManager && !_relayManager->zoneHasOutput(zoneIndex);
        EventLog::log(
            unmapped ? LOG_WARN : LOG_ERROR,
            unmapped ? "Equipment: zone %u %s ignore, aucune sortie affectee"
                     : "Equipment: zone %u %s path=failed error=dependency_unavailable",
            zoneIndex + 1U,
            active ? "ON" : "OFF"
        );
        return rejected(
            equipmentId,
            Domain::OperationError::DEPENDENCY_UNAVAILABLE,
            nowMs
        );
    }

    if (previousActive != active) {
        NotificationManager::enqueueZoneEvent(zoneIndex, active);
    }

    Domain::OperationResult result;
    result.executionId = nextExecutionId();
    result.equipmentId = equipmentId;
    result.status = Domain::OperationStatus::APPLIED;
    result.stage = Domain::OperationStage::APPLICATION;
    result.error = Domain::OperationError::NONE;
    result.completedAtMs = nowMs;

    EventLog::log(
        LOG_INFO,
        "Equipment: zone %u %s path=%s exec=%u ok=%lu ko=%lu",
        zoneIndex + 1U,
        active ? "ON" : "OFF",
        executionPathName(_lastExecutionPath),
        static_cast<unsigned>(result.executionId.value),
        static_cast<unsigned long>(_executionCounters.physicalBackend),
        static_cast<unsigned long>(_executionCounters.failed)
    );

    return result;
}

Domain::EquipmentStateValue EquipmentOutputRuntimeAdapter::getZoneValveState(
    uint8_t zoneIndex
) const {
    if (zoneIndex >= MAX_ZONES) {
        return Domain::EquipmentStateValue(
            0,
            Domain::StateValueKind::BINARY,
            Domain::StateValidity::INVALID
        );
    }

    bool active = false;

    if (_physicalBackend && _physicalBackend->getZoneValveState(zoneIndex, active)) {
        return Domain::EquipmentStateValue(
            active ? 1 : 0,
            Domain::StateValueKind::BINARY,
            Domain::StateValidity::VALID
        );
    }

    if (_relayManager) {
        return Domain::EquipmentStateValue(
            _relayManager->getState(zoneIndex) ? 1 : 0,
            Domain::StateValueKind::BINARY,
            Domain::StateValidity::VALID
        );
    }

    return Domain::EquipmentStateValue(
        0,
        Domain::StateValueKind::BINARY,
        Domain::StateValidity::UNKNOWN
    );
}

}} // namespace AquaLook::Runtime
