#pragma once

#include "common/audio_interfaces.hpp"
#include "common/lock_free_queue.hpp"
#include <memory>
#include <mutex>
#include <map>

namespace Client {

class AudioJitterBuffer {
public:
    AudioJitterBuffer(size_t maxSize = 50);
    ~AudioJitterBuffer();

    void PushFrame(std::unique_ptr<Audio::AudioFrame> frame);
    std::unique_ptr<Audio::AudioFrame> PopFrame();

    void Reset();

private:
    size_t m_maxSize;
    std::mutex m_mutex;
    std::map<uint64_t, std::unique_ptr<Audio::AudioFrame>> m_frames; // Keyed by timestamp
};

} // namespace Client
