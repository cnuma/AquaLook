#pragma once

#include <stddef.h>

namespace AquaLook { namespace Domain {

template <typename T>
class BoundedRegistry {
public:
    constexpr BoundedRegistry()
        : items_(nullptr), size_(0U), capacity_(0U) {}

    constexpr BoundedRegistry(T* items, size_t capacity)
        : items_(items), size_(0U), capacity_(items ? capacity : 0U) {}

    constexpr size_t size() const { return size_; }
    constexpr size_t capacity() const { return capacity_; }
    constexpr bool empty() const { return size_ == 0U; }
    constexpr bool full() const { return size_ >= capacity_; }

    T* data() { return items_; }
    const T* data() const { return items_; }

    T* at(size_t index) {
        return index < size_ ? &items_[index] : nullptr;
    }

    const T* at(size_t index) const {
        return index < size_ ? &items_[index] : nullptr;
    }

    bool append(const T& value) {
        if (!items_ || full()) return false;
        items_[size_++] = value;
        return true;
    }

    template <typename Predicate>
    T* findIf(Predicate predicate) {
        for (size_t i = 0U; i < size_; ++i) {
            if (predicate(items_[i])) return &items_[i];
        }
        return nullptr;
    }

    template <typename Predicate>
    const T* findIf(Predicate predicate) const {
        for (size_t i = 0U; i < size_; ++i) {
            if (predicate(items_[i])) return &items_[i];
        }
        return nullptr;
    }

    bool removeAt(size_t index) {
        if (!items_ || index >= size_) return false;
        for (size_t i = index + 1U; i < size_; ++i) {
            items_[i - 1U] = items_[i];
        }
        --size_;
        items_[size_] = T();
        return true;
    }

    void clear() {
        if (items_) {
            for (size_t i = 0U; i < size_; ++i) items_[i] = T();
        }
        size_ = 0U;
    }

private:
    T* items_;
    size_t size_;
    size_t capacity_;
};

}} // namespace AquaLook::Domain
