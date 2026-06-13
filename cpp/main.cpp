#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <set>
#include <mutex>
#include <algorithm>
#include <chrono>

#include "common/network_manager.hpp"
#include "common/protocol.hpp"
#include "common/safe_queue.hpp"

#ifdef _WIN32
#include <windows.h>
#include "host/capture_dxgi.hpp"
#include "host/encoder_hw.hpp"
#include "host/input_injector.hpp"
#include "client/receiver.hpp"
#include "client/decoder_hw.hpp"
#include "client/input_capture.hpp"
#include "client/renderer_d3d11.hpp"
#endif

void ShowUsage() {
    std::cout << "Usage: parsec-lite.exe [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --host           Start in Host Mode" << std::endl;
    std::cout << "  --client <IP>    Start in Client Mode connecting to <IP>" << std::endl;
    std::cout << "  --list           List available network interfaces" << std::endl;
    std::cout << "  --iface <index>  Explicitly select interface by index from --list" << std::endl;
}

#ifdef _WIN32
struct HostContext {
    std::atomic<bool> running{true};
    Network::NetworkManager net;
    Host::CaptureDXGI capture;
    Host::FFmpegHardwareEncoder encoder;
    Host::InputInjector injector;
    std::mutex clientsMutex;
    std::set<std::pair<std::string, uint16_t>> clients;
    SafeQueue<ID3D11Texture2D*> captureQueue;
    SafeQueue<std::vector<Host::EncodedPacket>> encodeQueue;
};

void RunHost(const std::string& ip) {
    HostContext ctx;
    if (!ctx.net.Bind(ip, 5005)) return;
    if (!ctx.capture.Initialize()) return;
    if (!ctx.encoder.Initialize(1920, 1080, 60, 5000)) return;

    std::thread captureThread([&]() {
        while (ctx.running) {
            ID3D11Texture2D* tex = nullptr;
            if (ctx.capture.AcquireFrame(&tex)) {
                ctx.captureQueue.push(tex);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::thread encoderThread([&]() {
        ID3D11Texture2D* tex = nullptr;
        while (ctx.running && ctx.captureQueue.wait_and_pop(tex)) {
            std::vector<Host::EncodedPacket> packets;
            if (ctx.encoder.EncodeFrame(tex, packets)) {
                ctx.encodeQueue.push(std::move(packets));
            }
            ctx.capture.ReleaseFrame();
        }
    });

    // Optimization: Reuse a static buffer for UDP packet fragments to avoid heap allocations
    std::thread senderThread([&]() {
        std::vector<Host::EncodedPacket> packets;
        uint32_t sequence = 0;
        uint8_t udpBuf[Protocol::MAX_UDP_PAYLOAD + sizeof(Protocol::VideoHeader)];

        while (ctx.running && ctx.encodeQueue.wait_and_pop(packets)) {
            for (const auto& p : packets) {
                uint16_t totalFrags = (uint16_t)((p.data.size() + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);
                for (uint16_t i = 0; i < totalFrags; ++i) {
                    Protocol::VideoHeader header;
                    header.type = (uint8_t)Protocol::PacketType::Video;
                    header.sequence = sequence;
                    header.fragmentIndex = i;
                    header.totalFragments = totalFrags;
                    header.timestamp = p.timestamp;
                    header.flags = p.isKeyframe ? 0x01 : 0x00;
                    header.dataSize = (uint16_t)std::min((uint32_t)Protocol::MAX_UDP_PAYLOAD, (uint32_t)(p.data.size() - i * Protocol::MAX_UDP_PAYLOAD));

                    memcpy(udpBuf, &header, sizeof(header));
                    memcpy(udpBuf + sizeof(header), p.data.data() + (i * Protocol::MAX_UDP_PAYLOAD), header.dataSize);

                    std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                    for (const auto& client : ctx.clients) {
                        ctx.net.SendTo(udpBuf, sizeof(header) + header.dataSize, client.first, client.second);
                    }
                }
            }
            sequence++;
        }
    });

    std::thread receiverThread([&]() {
        uint8_t buf[2048];
        std::string senderIp;
        uint16_t senderPort;
        while (ctx.running) {
            int len = ctx.net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len > 0) {
                Protocol::PacketType type = (Protocol::PacketType)buf[0];
                if (type == Protocol::PacketType::Handshake) {
                    std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                    ctx.clients.insert({senderIp, senderPort});
                } else if (type == Protocol::PacketType::Input && len >= (int)sizeof(Protocol::InputHeader)) {
                    Protocol::InputHeader* ih = (Protocol::InputHeader*)buf;
                    if (ih->inputType == (uint8_t)Protocol::InputType::Keyboard && len >= (int)(sizeof(Protocol::InputHeader) + sizeof(Protocol::KeyboardEvent))) {
                        ctx.injector.InjectKeyboard(*(Protocol::KeyboardEvent*)(buf + sizeof(Protocol::InputHeader)));
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseMove && len >= (int)(sizeof(Protocol::InputHeader) + sizeof(Protocol::MouseMoveEvent))) {
                        ctx.injector.InjectMouseMove(*(Protocol::MouseMoveEvent*)(buf + sizeof(Protocol::InputHeader)));
                    }
                }
            }
        }
    });

    std::cout << "Host running on " << ip << ". Press Enter to stop." << std::endl;
    std::cin.get();

    ctx.running = false;
    ctx.captureQueue.stop();
    ctx.encodeQueue.stop();

    if (captureThread.joinable()) captureThread.join();
    if (encoderThread.joinable()) encoderThread.join();
    if (senderThread.joinable()) senderThread.join();
    if (receiverThread.joinable()) receiverThread.join();
}

void RunClient(const std::string& localIp, const std::string& hostIp) {
    std::atomic<bool> running{true};
    Network::NetworkManager net;
    if (!net.Bind(localIp, 0)) return;

    Client::Receiver receiver;
    Client::DecoderHW decoder;
    Client::RendererD3D11 renderer;
    HWND hwnd = GetConsoleWindow();

    if (!renderer.Initialize(hwnd, 1920, 1080)) return;
    if (!decoder.Initialize(renderer.GetDevice())) return;

    std::thread netThread([&]() {
        uint8_t buf[65535];
        std::string senderIp;
        uint16_t senderPort;
        while (running) {
            int len = net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len >= (int)sizeof(Protocol::VideoHeader)) {
                Protocol::VideoHeader* vh = (Protocol::VideoHeader*)buf;
                if (vh->type == (uint8_t)Protocol::PacketType::Video && len >= (int)(sizeof(Protocol::VideoHeader) + vh->dataSize)) {
                    receiver.ProcessPacket(*vh, buf + sizeof(Protocol::VideoHeader));
                }
            }
        }
    });

    Client::InputCapture inputCap([&](const std::vector<uint8_t>& data) {
        net.SendTo(data.data(), data.size(), hostIp, 5005);
    });
    std::thread inputThread([&]() {
        while (running) {
            inputCap.PollGamepads();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    uint8_t handshake = (uint8_t)Protocol::PacketType::Handshake;
    net.SendTo(&handshake, 1, hostIp, 5005);

    while (running) {
        auto frame = receiver.GetNextFrame();
        if (frame) {
            void* texture = nullptr;
            if (decoder.DecodeFrame(frame->buffer.data(), frame->totalSize, &texture)) {
                renderer.Render((ID3D11Texture2D*)texture);
            }
            receiver.ReturnToPool(std::move(frame));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    running = false;
    if (netThread.joinable()) netThread.join();
    if (inputThread.joinable()) inputThread.join();
}
#else
void RunHost(const std::string& ip) {}
void RunClient(const std::string& localIp, const std::string& hostIp) {}
#endif

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    if (args.size() < 2) { ShowUsage(); return 1; }

    auto interfaces = Network::NetworkManager::EnumerateInterfaces();
    std::string selectedIp = "127.0.0.1";

    if (args[1] == "--list") {
        for (size_t i = 0; i < interfaces.size(); ++i) {
            std::cout << "[" << i << "] " << interfaces[i].name << " (" << interfaces[i].ip << ")" << std::endl;
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--iface" && i + 1 < args.size()) {
            int idx = std::stoi(args[i + 1]);
            if (idx >= 0 && idx < (int)interfaces.size()) selectedIp = interfaces[idx].ip;
        }
    }

    if (args[1] == "--host") {
        RunHost(selectedIp);
    } else if (args[1] == "--client" && args.size() > 2) {
        RunClient(selectedIp, args[2]);
    } else {
        ShowUsage();
    }

    return 0;
}
