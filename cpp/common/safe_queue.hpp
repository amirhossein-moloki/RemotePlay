#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include "profiler.hpp"

template<typename T>
class SafeQueue {
public:
    SafeQueue(std::string name = "unnamed") : m_name(name) {}

    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        Profiler::getInstance().recordValue("QueueSize_" + m_name, (double)m_queue.size());
        m_cond.notify_one();
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty()) return false;
        value = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    bool wait_and_pop(T& value) {
        auto start = std::chrono::high_resolution_clock::now();
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this] { return !m_queue.empty() || m_stopped; });

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        Profiler::getInstance().recordTime("QueueWait_" + m_name, (double)duration);

        if (m_stopped && m_queue.empty()) return false;
        value = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopped = true;
        m_cond.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

private:
    std::string m_name;
    std::queue<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_stopped = false;
};
