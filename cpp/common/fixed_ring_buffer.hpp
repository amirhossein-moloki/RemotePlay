#pragma once

#include <vector>
#include <atomic>
#include <optional>

/**
 * A simple, fixed-size circular buffer.
 * Not thread-safe for multiple producers/consumers without external locking,
 * but good for O(1) lookups by index/sequence.
 */
template<typename T, size_t Size>
class FixedRingBuffer {
public:
    FixedRingBuffer() {
        m_buffer.resize(Size);
    }

    void insert(uint32_t index, T value) {
        m_buffer[index % Size] = std::move(value);
    }

    T* get(uint32_t index) {
        return &m_buffer[index % Size];
    }

    const T* get(uint32_t index) const {
        return &m_buffer[index % Size];
    }

    size_t capacity() const { return Size; }

private:
    std::vector<T> m_buffer;
};
