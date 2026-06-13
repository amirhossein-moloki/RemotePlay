#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <cstdint>
#include "profiler.hpp"

/**
 * A pool of pre-allocated buffers to avoid heap allocations in the hot path.
 */
class PacketPool {
public:
    struct Packet {
        std::vector<uint8_t> data;
        size_t size = 0;
        bool isKeyframe = false;
        uint64_t timestamp = 0;
        uint32_t frameId = 0;

        Packet(size_t capacity) {
            data.resize(capacity);
        }
    };

    PacketPool(size_t poolSize, size_t packetCapacity) {
        for (size_t i = 0; i < poolSize; ++i) {
            m_pool.push_back(std::make_unique<Packet>(packetCapacity));
        }
    }

    std::unique_ptr<Packet> acquire() {
        Profiler::getInstance().recordCounter("PacketAcquire");
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pool.empty()) {
            Profiler::getInstance().recordCounter("PacketPoolExhausted");
            // Production hardening: Avoid runtime heap allocation in hot path.
            // Returning nullptr forces the caller to handle exhaustion (e.g., drop frame).
            return nullptr;
        }
        auto pkt = std::move(m_pool.back());
        m_pool.pop_back();
        return pkt;
    }

    void release(std::unique_ptr<Packet> pkt) {
        Profiler::getInstance().recordCounter("PacketRelease");
        if (!pkt) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pool.push_back(std::move(pkt));
    }

private:
    std::vector<std::unique_ptr<Packet>> m_pool;
    std::mutex m_mutex;
};
