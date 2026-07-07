#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <new>

namespace AquaLook { namespace Domain {

class BoundedArena {
public:
    static constexpr uint32_t INVALID_OFFSET = 0xFFFFFFFFUL;

    BoundedArena(void* buffer, size_t capacity)
        : _buffer(static_cast<uint8_t*>(buffer)),
          _capacity(buffer ? capacity : 0U),
          _used(0U) {}

    void reset() { _used = 0U; }

    size_t capacity() const { return _capacity; }
    size_t used() const { return _used; }
    size_t remaining() const { return _capacity - _used; }

    void* allocate(size_t size, size_t alignment = alignof(max_align_t)) {
        if (size == 0U || alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
            return nullptr;
        }

        const size_t aligned = alignUp(_used, alignment);
        if (aligned > _capacity || size > (_capacity - aligned)) {
            return nullptr;
        }

        void* result = _buffer + aligned;
        _used = aligned + size;
        return result;
    }

    template <typename T>
    T* create() {
        void* memory = allocate(sizeof(T), alignof(T));
        return memory ? new (memory) T() : nullptr;
    }

    template <typename T>
    T* create(const T& value) {
        void* memory = allocate(sizeof(T), alignof(T));
        return memory ? new (memory) T(value) : nullptr;
    }

    uint32_t appendBytes(const void* data, size_t size, size_t alignment = 1U) {
        if ((data == nullptr && size != 0U) || size > UINT32_MAX) return INVALID_OFFSET;
        void* destination = allocate(size, alignment);
        if (!destination) return INVALID_OFFSET;
        if (size != 0U) memcpy(destination, data, size);
        return offsetOf(destination);
    }

    uint32_t offsetOf(const void* pointer) const {
        if (!_buffer || !pointer) return INVALID_OFFSET;
        const uint8_t* bytes = static_cast<const uint8_t*>(pointer);
        if (bytes < _buffer || bytes >= (_buffer + _capacity)) return INVALID_OFFSET;
        const size_t offset = static_cast<size_t>(bytes - _buffer);
        return offset <= UINT32_MAX ? static_cast<uint32_t>(offset) : INVALID_OFFSET;
    }

    void* pointerAt(uint32_t offset, size_t size = 1U) {
        if (offset == INVALID_OFFSET || offset > _used || size > (_used - offset)) return nullptr;
        return _buffer + offset;
    }

    const void* pointerAt(uint32_t offset, size_t size = 1U) const {
        if (offset == INVALID_OFFSET || offset > _used || size > (_used - offset)) return nullptr;
        return _buffer + offset;
    }

private:
    static size_t alignUp(size_t value, size_t alignment) {
        return (value + alignment - 1U) & ~(alignment - 1U);
    }

    uint8_t* _buffer;
    size_t _capacity;
    size_t _used;
};

}} // namespace AquaLook::Domain
