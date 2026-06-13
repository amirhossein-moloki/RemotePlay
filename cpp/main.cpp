#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "common/network_manager.hpp"
#include "common/protocol.hpp"

void ShowUsage() {
    std::cout << "Usage: parsec-lite.exe [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --host           Start in Host Mode" << std::endl;
    std::cout << "  --client <IP>    Start in Client Mode connecting to <IP>" << std::endl;
    std::cout << "  --list           List available network interfaces" << std::endl;
    std::cout << "  --iface <index>  Explicitly select interface by index from --list" << std::endl;
}

void RunHost(const std::string& ip) {
    std::cout << "Starting Host on " << ip << "..." << std::endl;
    Network::NetworkManager net;
    if (!net.Bind(ip, 5005)) {
        std::cerr << "Failed to bind to " << ip << ":5005" << std::endl;
        return;
    }

    // In a real implementation:
    // 1. Initialize CaptureDXGI
    // 2. Initialize EncoderHW
    // 3. Initialize InputInjector
    // 4. Start main loop: Capture -> Encode -> Send Fragments
    // 5. Start async thread for receiving input packets -> Inject

    std::cout << "Host is running. Press Ctrl+C to stop." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void RunClient(const std::string& localIp, const std::string& hostIp) {
    std::cout << "Starting Client on " << localIp << ", connecting to " << hostIp << "..." << std::endl;
    Network::NetworkManager net;
    // Client binds to any available port for receiving
    if (!net.Bind(localIp, 0)) {
        std::cerr << "Failed to bind local socket" << std::endl;
        return;
    }

    // In a real implementation:
    // 1. Send Handshake to hostIp
    // 2. Initialize DecoderHW
    // 3. Initialize Renderer (SDL2/D3D11)
    // 4. Initialize InputCapture
    // 5. Start main loop: Receive Fragments -> Reassemble -> Decode -> Render
    // 6. Capture local input -> Send to hostIp

    std::cout << "Client is running. Press Ctrl+C to stop." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ShowUsage();
        return 1;
    }

    std::vector<std::string> args(argv, argv + argc);
    auto interfaces = Network::NetworkManager::EnumerateInterfaces();
    std::string selectedIp = "";

    // 1. Handle --list
    if (args[1] == "--list") {
        std::cout << "Available Network Interfaces:" << std::endl;
        for (size_t i = 0; i < interfaces.size(); ++i) {
            std::cout << "[" << i << "] " << interfaces[i].name << " (" << interfaces[i].ip << ") ["
                      << (interfaces[i].isActive ? "ACTIVE" : "INACTIVE") << "]" << std::endl;
        }
        return 0;
    }

    // 2. Handle --iface selection
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--iface" && i + 1 < args.size()) {
            int idx = std::stoi(args[i + 1]);
            if (idx >= 0 && idx < (int)interfaces.size()) {
                selectedIp = interfaces[idx].ip;
                std::cout << "Manually selected interface: " << interfaces[idx].name << " (" << selectedIp << ")" << std::endl;
            }
            break;
        }
    }

    // 3. Auto-select if not manually specified
    if (selectedIp.empty()) {
        for (const auto& iface : interfaces) {
            if (iface.isActive && iface.ip != "127.0.0.1") {
                selectedIp = iface.ip;
                std::cout << "Auto-selected interface: " << iface.name << " (" << selectedIp << ")" << std::endl;
                break;
            }
        }
    }

    if (selectedIp.empty()) selectedIp = "0.0.0.0";

    // 4. Mode Selection
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
