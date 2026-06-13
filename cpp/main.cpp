#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <set>
#include <mutex>
#include <algorithm>

#include "common/network_manager.hpp"
#include "common/protocol.hpp"

// Proper headers
#ifdef _WIN32
#include "host/capture_dxgi.hpp"
#include "host/encoder_hw.hpp"
#include "host/input_injector.hpp"
#include "client/receiver.hpp"
#include "client/decoder_hw.hpp"
#include "client/input_capture.hpp"
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
void RunHost(const std::string& ip) {
    std::cout << "Starting Host on " << ip << "..." << std::endl;
    Network::NetworkManager net;
    if (!net.Bind(ip, 5005)) {
        std::cerr << "Failed to bind to " << ip << ":5005" << std::endl;
        return;
    }

    Host::CaptureDXGI capture;
    Host::FFmpegHardwareEncoder encoder;
    Host::InputInjector injector;

    if (!capture.Initialize()) {
        std::cerr << "Failed to initialize DXGI Capture" << std::endl;
        return;
    }

    if (!encoder.Initialize(1920, 1080, 60, 5000)) {
        std::cerr << "Failed to initialize Hardware Encoder" << std::endl;
        return;
    }

    std::atomic<bool> running{true};
    std::set<std::pair<std::string, uint16_t>> clients;
    std::mutex clientsMutex;

    // Async thread for receiving control packets (Handshake/Input)
    std::thread networkThread([&]() {
        char buf[2048];
        std::string senderIp;
        uint16_t senderPort;
        while (running) {
            int len = net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len > 0) {
                uint8_t type = (uint8_t)buf[0];
                if (type == (uint8_t)Protocol::PacketType::Handshake) {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    clients.insert({senderIp, senderPort});
                    std::cout << "Client connected: " << senderIp << ":" << senderPort << std::endl;
                } else if (type == (uint8_t)Protocol::PacketType::Input) {
                    // Forward to injector logic here
                }
            }
        }
    });

    uint32_t sequence = 0;
    std::cout << "Host is running. Press Ctrl+C to stop." << std::endl;

    while (running) {
        auto start = std::chrono::steady_clock::now();
        ID3D11Texture2D* texture = nullptr;
        if (capture.AcquireFrame(&texture)) {
            std::vector<Host::EncodedPacket> packets;
            if (encoder.EncodeFrame(texture, packets)) {
                for (const auto& p : packets) {
                    uint16_t totalFrags = (uint16_t)((p.data.size() + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);
                    for (uint16_t i = 0; i < totalFrags; ++i) {
                        Protocol::VideoHeader header;
                        header.type = (uint8_t)Protocol::PacketType::Video;
                        header.sequence = sequence;
                        header.fragmentIndex = i;
                        header.totalFragments = totalFrags;
                        header.dataSize = (uint16_t)std::min((uint32_t)Protocol::MAX_UDP_PAYLOAD, (uint32_t)(p.data.size() - i * Protocol::MAX_UDP_PAYLOAD));

                        std::vector<uint8_t> udpBuf(sizeof(header) + header.dataSize);
                        memcpy(udpBuf.data(), &header, sizeof(header));
                        memcpy(udpBuf.data() + sizeof(header), p.data.data() + (i * Protocol::MAX_UDP_PAYLOAD), header.dataSize);

                        std::lock_guard<std::mutex> lock(clientsMutex);
                        for (const auto& client : clients) {
                            net.SendTo(udpBuf.data(), udpBuf.size(), client.first, client.second);
                        }
                    }
                }
                sequence++;
            }
            capture.ReleaseFrame();
        }

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto sleepTime = std::chrono::milliseconds(16) - elapsed;
        if (sleepTime > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleepTime);
        }
    }

    running = false;
    if (networkThread.joinable()) networkThread.join();
}

void RunClient(const std::string& localIp, const std::string& hostIp) {
    std::cout << "Starting Client on " << localIp << ", connecting to " << hostIp << "..." << std::endl;
    Network::NetworkManager net;
    if (!net.Bind(localIp, 0)) return;

    Client::Receiver receiver;
    Client::DecoderHW decoder;

    if (!decoder.Initialize()) return;

    // Send Handshake
    uint8_t handshake = (uint8_t)Protocol::PacketType::Handshake;
    net.SendTo(&handshake, 1, hostIp, 5005);

    std::atomic<bool> running{true};
    std::thread networkThread([&]() {
        uint8_t buf[65535];
        std::string senderIp;
        uint16_t senderPort;
        while (running) {
            int len = net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len >= (int)sizeof(Protocol::VideoHeader)) {
                Protocol::VideoHeader* header = (Protocol::VideoHeader*)buf;
                if (header->type == (uint8_t)Protocol::PacketType::Video) {
                    receiver.ProcessPacket(*header, buf + sizeof(Protocol::VideoHeader));
                }
            }
        }
    });

    while (running) {
        auto frame = receiver.GetNextFrame();
        if (frame) {
            void* texture = nullptr;
            if (decoder.DecodeFrame(frame->buffer.data(), frame->totalSize, &texture)) {
                // Render texture logic
            }
            receiver.ReturnToPool(std::move(frame));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    running = false;
    if (networkThread.joinable()) networkThread.join();
}
#else
void RunHost(const std::string& ip) {
    std::cout << "Host Mode is only supported on Windows (DXGI/NVENC required)." << std::endl;
}
void RunClient(const std::string& localIp, const std::string& hostIp) {
    std::cout << "Client Mode is only supported on Windows in this prototype." << std::endl;
}
#endif

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ShowUsage();
        return 1;
    }

    std::vector<std::string> args(argv, argv + argc);
    auto interfaces = Network::NetworkManager::EnumerateInterfaces();
    std::string selectedIp = "";

    if (args[1] == "--list") {
        std::cout << "Available Network Interfaces:" << std::endl;
        for (size_t i = 0; i < interfaces.size(); ++i) {
            std::cout << "[" << i << "] " << interfaces[i].name << " (" << interfaces[i].ip << ") ["
                      << (interfaces[i].isActive ? "ACTIVE" : "INACTIVE") << "]" << std::endl;
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--iface" && i + 1 < args.size()) {
            int idx = std::stoi(args[i + 1]);
            if (idx >= 0 && idx < (int)interfaces.size()) {
                selectedIp = interfaces[idx].ip;
            }
            break;
        }
    }

    if (selectedIp.empty()) {
        for (const auto& iface : interfaces) {
            if (iface.isActive && iface.ip != "127.0.0.1") {
                selectedIp = iface.ip;
                break;
            }
        }
    }
    if (selectedIp.empty()) selectedIp = "127.0.0.1";

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--host") {
            RunHost(selectedIp);
            return 0;
        } else if (args[i] == "--client" && i + 1 < args.size()) {
            RunClient(selectedIp, args[i + 1]);
            return 0;
        }
    }

    ShowUsage();
    return 1;
}
