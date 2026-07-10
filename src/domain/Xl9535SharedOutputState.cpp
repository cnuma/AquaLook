#include "domain/Xl9535SharedOutputState.h"

namespace AquaLook { namespace Domain {

Xl9535SharedOutputState::Xl9535SharedOutputState() {
    clear();
}

void Xl9535SharedOutputState::clear() {
    for (size_t i = 0U; i < MAX_DEVICES; ++i) {
        _entries[i] = Entry();
    }
}

bool Xl9535SharedOutputState::seed(uint8_t address, uint16_t value) {
    Entry* entry = findOrCreate(address);
    if (!entry) return false;
    entry->value = value;
    return true;
}

bool Xl9535SharedOutputState::read(uint8_t address, uint16_t& value) const {
    const Entry* entry = find(address);
    if (!entry) return false;
    value = entry->value;
    return true;
}

bool Xl9535SharedOutputState::updateChannel(
    uint8_t address,
    uint8_t channel,
    bool high,
    uint16_t& value
) {
    if (channel >= 16U) return false;

    Entry* entry = findOrCreate(address);
    if (!entry) return false;

    const uint16_t mask = static_cast<uint16_t>(1U << channel);
    if (high) entry->value = static_cast<uint16_t>(entry->value | mask);
    else entry->value = static_cast<uint16_t>(entry->value & ~mask);

    value = entry->value;
    return true;
}

Xl9535SharedOutputState::Entry* Xl9535SharedOutputState::find(uint8_t address) {
    for (size_t i = 0U; i < MAX_DEVICES; ++i) {
        if (_entries[i].used != 0U && _entries[i].address == address) {
            return &_entries[i];
        }
    }
    return nullptr;
}

const Xl9535SharedOutputState::Entry* Xl9535SharedOutputState::find(
    uint8_t address
) const {
    for (size_t i = 0U; i < MAX_DEVICES; ++i) {
        if (_entries[i].used != 0U && _entries[i].address == address) {
            return &_entries[i];
        }
    }
    return nullptr;
}

Xl9535SharedOutputState::Entry* Xl9535SharedOutputState::findOrCreate(
    uint8_t address
) {
    if (Entry* existing = find(address)) return existing;

    for (size_t i = 0U; i < MAX_DEVICES; ++i) {
        if (_entries[i].used == 0U) {
            _entries[i].used = 1U;
            _entries[i].address = address;
            _entries[i].value = 0U;
            return &_entries[i];
        }
    }
    return nullptr;
}

}} // namespace AquaLook::Domain
