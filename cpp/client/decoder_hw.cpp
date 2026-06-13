#include "decoder_hw.hpp"
#include <iostream>

namespace Client {

DecoderHW::DecoderHW() {}
DecoderHW::~DecoderHW() { Shutdown(); }

bool DecoderHW::Initialize() {
    std::cout << "[Decoder] Initializing FFmpeg Hardware Decoder (D3D11VA/DXVA2)..." << std::endl;
    return true;
}

bool DecoderHW::DecodeFrame(const uint8_t* data, size_t size, void** outTexture) {
    // Implementation would use avcodec_send_packet / avcodec_receive_frame
    return true;
}

void DecoderHW::Shutdown() {
    std::cout << "[Decoder] Shutting down FFmpeg Decoder" << std::endl;
}

} // namespace Client
