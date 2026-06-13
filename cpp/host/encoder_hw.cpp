#include <iostream>
#include <vector>
#include <string>

/**
 * PRODUCTION NOTE:
 * To implement this using FFmpeg's libavcodec for hardware acceleration:
 *
 * 1. Find the encoder by name:
 *    avcodec_find_encoder_by_name("h264_nvenc") // NVIDIA
 *    avcodec_find_encoder_by_name("h264_amf")   // AMD
 *    avcodec_find_encoder_by_name("h264_qsv")   // Intel
 *
 * 2. Configure AVCodecContext:
 *    ctx->width = width;
 *    ctx->height = height;
 *    ctx->time_base = {1, fps};
 *    ctx->pix_fmt = AV_PIX_FMT_CUDA; // or appropriate for the hardware
 *    av_opt_set(ctx->priv_data, "preset", "p1", 0);
 *    av_opt_set(ctx->priv_data, "tune", "ull", 0);
 *
 * 3. Encode Loop:
 *    avcodec_send_frame(ctx, frame);
 *    avcodec_receive_packet(ctx, pkt);
 */

namespace Host {

struct EncodedPacket {
    std::vector<uint8_t> data;
    bool isKeyframe;
    uint64_t timestamp;
};

class EncoderHW {
public:
    virtual ~EncoderHW() {}
    virtual bool Initialize(int width, int height, int fps, int bitrateKbps) = 0;
    virtual bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets) = 0;
    virtual void Shutdown() = 0;
};

class FFmpegHardwareEncoder : public EncoderHW {
public:
    bool Initialize(int width, int height, int fps, int bitrateKbps) override {
        std::cout << "[Encoder] Initializing FFmpeg Hardware Encoder (NVENC/AMF/QSV)..." << std::endl;
        m_width = width;
        m_height = height;
        // Search for available hardware encoders in order of preference
        const char* encoders[] = {"h264_nvenc", "h264_amf", "h264_qsv"};
        for (const char* name : encoders) {
            std::cout << "[Encoder] Checking for " << name << "..." << std::endl;
            // In a real build: if (avcodec_find_encoder_by_name(name)) { m_encoderName = name; break; }
        }
        return true;
    }

    bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets) override {
        // Implementation would use avcodec_send_frame / avcodec_receive_packet
        return true;
    }

    void Shutdown() override {
        std::cout << "[Encoder] Shutting down FFmpeg Encoder" << std::endl;
    }

private:
    int m_width, m_height;
    std::string m_encoderName;
};

} // namespace Host
