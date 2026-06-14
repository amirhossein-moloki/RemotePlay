#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <atomic>
#include <array>
#include <memory>

/**
 * Optimized Profiler for high-performance pipelines.
 * Uses bounded memory for samples and atomic counters.
 */
class Profiler {
public:
    static constexpr size_t MAX_SAMPLES = 1000;

    struct SampleBuffer {
        std::array<double, MAX_SAMPLES> samples;
        std::atomic<size_t> count{0};
        double min = 1e18;
        double max = 0;
        double sum = 0;
        std::mutex mutex;

        void record(double val) {
            std::lock_guard<std::mutex> lock(mutex);
            if (count < MAX_SAMPLES) {
                samples[count++] = val;
            } else {
                // Reservoir sampling or just replacement for P99 approximation
                samples[count % MAX_SAMPLES] = val;
                count++;
            }
            if (val < min) min = val;
            if (val > max) max = val;
            sum += val;
        }
    };

    struct Stats {
        double avg = 0;
        double min = 0;
        double max = 0;
        double p99 = 0;
        double latest = 0;
        size_t totalCount = 0;
    };

    static Profiler& getInstance() {
        static Profiler instance;
        return instance;
    }

    void recordTime(const std::string& name, double durationUs) {
        getBuffer(m_timeSamples, name).record(durationUs);
    }

    void recordCounter(const std::string& name, size_t count = 1) {
        std::lock_guard<std::mutex> lock(m_counterMutex);
        m_counters[name] += count;
    }

    void recordValue(const std::string& name, double value) {
        getBuffer(m_valueSamples, name).record(value);
    }

    Stats getStats(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        auto it = m_timeSamples.find(name);
        if (it != m_timeSamples.end()) return calculateStats(*it->second);
        it = m_valueSamples.find(name);
        if (it != m_valueSamples.end()) return calculateStats(*it->second);
        return {};
    }

    void report() {
        std::cout << "\n=== Performance Profiling Report (Bounded Samples) ===\n";
        std::cout << std::left << std::setw(30) << "Metric"
                  << std::setw(10) << "Count"
                  << std::setw(12) << "Avg(us)"
                  << std::setw(12) << "Min(us)"
                  << std::setw(12) << "Max(us)"
                  << std::setw(12) << "P99(us)" << "\n";
        std::cout << std::string(88, '-') << "\n";

        std::lock_guard<std::mutex> lock(m_mapMutex);
        for (auto& entry : m_timeSamples) {
            auto stats = calculateStats(*entry.second);
            std::cout << std::left << std::setw(30) << entry.first
                      << std::setw(10) << stats.totalCount
                      << std::fixed << std::setprecision(2)
                      << std::setw(12) << stats.avg
                      << std::setw(12) << stats.min
                      << std::setw(12) << stats.max
                      << std::setw(12) << stats.p99 << "\n";
        }

        if (!m_counters.empty()) {
            std::cout << "\n--- Counters ---\n";
            for (auto const& [name, count] : m_counters) {
                std::cout << std::left << std::setw(30) << name << ": " << count << "\n";
            }
        }
        std::cout << "====================================\n" << std::endl;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        m_timeSamples.clear();
        m_counters.clear();
        m_valueSamples.clear();
    }

private:
    SampleBuffer& getBuffer(std::map<std::string, std::unique_ptr<SampleBuffer>>& map, const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        auto it = map.find(name);
        if (it == map.end()) {
            it = map.emplace(name, std::make_unique<SampleBuffer>()).first;
        }
        return *it->second;
    }

    Stats calculateStats(SampleBuffer& buf) {
        std::lock_guard<std::mutex> lock(buf.mutex);
        size_t n = (std::min)((size_t)buf.count, MAX_SAMPLES);
        if (n == 0) return {};

        std::vector<double> sorted(buf.samples.begin(), buf.samples.begin() + n);
        std::sort(sorted.begin(), sorted.end());

        Stats s;
        s.totalCount = buf.count;
        s.min = buf.min;
        s.max = buf.max;
        s.avg = buf.sum / buf.count;
        s.p99 = sorted[static_cast<size_t>(n * 0.99)];
        s.latest = buf.samples[(buf.count - 1) % MAX_SAMPLES];
        return s;
    }

    std::mutex m_mapMutex;
    std::mutex m_counterMutex;
    std::map<std::string, std::unique_ptr<SampleBuffer>> m_timeSamples;
    std::map<std::string, size_t> m_counters;
    std::map<std::string, std::unique_ptr<SampleBuffer>> m_valueSamples;
};

class ScopeTimer {
public:
    ScopeTimer(const std::string& name) : m_name(name), m_start(std::chrono::high_resolution_clock::now()) {}
    ~ScopeTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();
        Profiler::getInstance().recordTime(m_name, (double)duration);
    }
private:
    std::string m_name;
    std::chrono::high_resolution_clock::time_point m_start;
};
