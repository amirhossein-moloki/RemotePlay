#pragma once
#include <string>
#include <vector>
#include "../common/network_manager.hpp"

namespace Client {

struct LauncherConfig {
    bool isHost = true;
    std::string selectedIp = "127.0.0.1";
    std::string hostIp = "127.0.0.1";
    int bitrate = 8000;
    int fps = 60;
    bool useHardwareEncoding = true;
    bool startRequested = false;
};

class Launcher {
public:
    static void Render(LauncherConfig& config, const std::vector<Network::InterfaceInfo>& interfaces);
};

} // namespace Client
