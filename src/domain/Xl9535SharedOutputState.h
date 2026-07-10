#pragma once

#include <stddef.h>
#include <stdint.h>

namespace AquaLook { namespace Domain {

class Xl9535SharedOutputState {
public:
    static constexpr size_t MAX_DEVICES = 8U;

    Xl9535SharedOutputState();

    void clear();
    bool seed(uint8_t address, uint16_t value);
    bool read(uint8_t address, uint16_t& value) const;
    bool updateChannel(
        uint8_t address,
        uint8_t channel,
        bool high,
        uint16_t& value
    );

private:
    struct Entry {
        uint8_t address;
        uint8_t used;
        uint16_t value;

        Entry() : address(0U), used(0U), value(0U) {}
    };

    Entry* find(uint8_t address);
    const Entry* find(uint8_t address) const;
    Entry* findOrCreate(uint8_t address);

    Entry _entries[MAX_DEVICES];
};

}} // namespace AquaLook::Domain
