#include "audio_jitter_buffer.hpp"

namespace Client {

AudioJitterBuffer::AudioJitterBuffer(size_t maxSize) : m_maxSize(maxSize) {}

AudioJitterBuffer::~AudioJitterBuffer() {
    Reset();
}

void AudioJitterBuffer::PushFrame(std::unique_ptr<Audio::AudioFrame> frame) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Simple jitter buffer: store by timestamp to handle reordering
    m_frames[frame->timestamp] = std::move(frame);

    // Limit size
    while (m_frames.size() > m_maxSize) {
        m_frames.erase(m_frames.begin());
    }
}

std::unique_ptr<Audio::AudioFrame> AudioJitterBuffer::PopFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frames.empty()) return nullptr;

    // For simplicity, we just pop the oldest frame.
    // A more advanced jitter buffer would wait for a target delay to handle jitter.
    auto it = m_frames.begin();
    auto frame = std::move(it->second);
    m_frames.erase(it);
    return frame;
}

void AudioJitterBuffer::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames.clear();
}

} // namespace Client
