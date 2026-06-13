#pragma once

#include <atomic>
#include <vector>
#include <memory>
#include <optional>

/**
 * A Single-Producer Single-Consumer (SPSC) Lock-Free Queue.
 * Optimized for high-throughput, low-latency data passing between two threads.
 */
template<typename T, size_t Capacity>
class LockFreeQueue {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

    LockFreeQueue() : m_head(0), m_tail(0) {
        m_buffer.resize(Capacity);
    }

    /**
     * Pushes an item into the queue.
     * Returns true if successful, false if the queue is full.
     */
    bool push(T&& item) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        size_t head = m_head.load(std::memory_order_acquire);

        if (tail - head >= Capacity) {
            return false; // Queue is full
        }

        m_buffer[tail & (Capacity - 1)] = std::move(item);
        m_tail.store(tail + 1, std::memory_order_release);
        return true;
    }

    /**
     * Pops an item from the queue.
     * Returns true if successful, false if the queue is empty.
     */
    bool pop(T& item) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t tail = m_tail.load(std::memory_order_acquire);

        if (head == tail) {
            return false; // Queue is empty
        }

        item = std::move(m_buffer[head & (Capacity - 1)]);
        m_head.store(head + 1, std::memory_order_release);
        return true;
    }

    /**
     * Checks if the queue is empty.
     */
    bool empty() const {
        return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
    }

    /**
     * Returns the approximate size of the queue.
     */
    size_t size() const {
        return m_tail.load(std::memory_order_relaxed) - m_head.load(std::memory_order_relaxed);
    }

private:
    std::vector<T> m_buffer;
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};
