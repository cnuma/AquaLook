#include "InputSampler.h"

#include "EventLog.h"

void InputSampler::begin(const RelayTopology::RelayTopologyConfig* topology,
                         Reader reader, bool verbose) {
    _topology = topology;
    _reader = reader;
    _verbose = verbose;
    _count = 0U;
    _lastSampleMs = 0U;

    if (!_topology) return;

    for (uint8_t a = 0U; a < RelayTopology::MAX_RELAY_ASSIGNMENTS && _count < MAX_INPUTS; ++a) {
        const RelayTopology::RelayAssignment& as = _topology->assignments[a];
        if (!as.enabled || !as.isInput() || as.id == 0U) continue;
        Slot& slot = _slots[_count++];
        slot = Slot();
        slot.id = as.id;
    }

    if (_count > 0U && _verbose) {
        EventLog::log(LOG_INFO, "Entrees: %u surveillee(s), stable apres %u ms",
                      (unsigned)_count,
                      (unsigned)(SAMPLE_MS * STABLE_SAMPLES));
    }
}

int8_t InputSampler::indexOf(uint16_t inputId) const {
    for (uint8_t i = 0U; i < _count; ++i) {
        if (_slots[i].id == inputId) return static_cast<int8_t>(i);
    }
    return -1;
}

void InputSampler::update(uint32_t nowMs) {
    if (!_reader || _count == 0U) return;
    if (_lastSampleMs != 0U && (nowMs - _lastSampleMs) < SAMPLE_MS) return;
    _lastSampleMs = nowMs;

    for (uint8_t i = 0U; i < _count; ++i) {
        Slot& slot = _slots[i];
        bool value = false;
        if (!_reader(slot.id, value)) {
            // Carte muette : on ne fabrique pas de valeur. L'ancienne valeur
            // stabilisee est conservee et signalee absente -- inventer un
            // "inactif" ferait croire a une cuve pleine.
            if (slot.present && _verbose) {
                EventLog::log(LOG_WARN, "Entrees: entree %u ne repond plus", (unsigned)slot.id);
            }
            slot.present = false;
            slot.steady = 0U;
            continue;
        }

        if (!slot.present) {
            slot.present = true;
            slot.steady = 0U;
        }

        if (value == slot.raw) {
            if (slot.steady < STABLE_SAMPLES) slot.steady++;
        } else {
            slot.raw = value;
            slot.steady = 1U;
        }

        // La valeur ne devient officielle qu'apres etre restee identique
        // assez longtemps. C'est ce compteur, et rien d'autre, qui empeche
        // un flotteur qui claquette de commander une vanne en rafale.
        if (slot.steady >= STABLE_SAMPLES && slot.stable != slot.raw) {
            slot.stable = slot.raw;
            slot.established = true;
            slot.changes++;
            if (_verbose) {
                EventLog::log(LOG_INFO, "Entrees: entree %u -> %s",
                              (unsigned)slot.id, slot.stable ? "actif" : "inactif");
            }
        } else if (slot.steady >= STABLE_SAMPLES) {
            slot.established = true;
        }
    }
}

bool InputSampler::read(uint16_t inputId, bool& active) const {
    const int8_t i = indexOf(inputId);
    if (i < 0 || !_slots[i].established) return false;
    active = _slots[i].stable;
    return true;
}

bool InputSampler::readRaw(uint16_t inputId, bool& active) const {
    const int8_t i = indexOf(inputId);
    if (i < 0 || !_slots[i].present) return false;
    active = _slots[i].raw;
    return true;
}

uint32_t InputSampler::transitions(uint16_t inputId) const {
    const int8_t i = indexOf(inputId);
    return i < 0 ? 0U : _slots[i].changes;
}
