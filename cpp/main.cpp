#define NOMINMAX
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "common/parsec_lite_api.h"
#include "common/network_manager.hpp"
#include "common/logger.hpp"
#include "common/config.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellscalingapi.h>

void ShowUsage() {
    std::cout << "Usage: parsec-lite.exe [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --host           Start in Host Mode" << std::endl;
    std::cout << "  --client <IP>    Start in Client Mode connecting to <IP>" << std::endl;
    std::cout << "  --list           List available network interfaces" << std::endl;
}

void RunLauncher() {
    // Attempt to launch the modern Qt-based UI first
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t lastSlash = path.find_last_of("\\/");
    std::string dir = (lastSlash != std::string::npos) ? path.substr(0, lastSlash) : ".";

    std::string uiPath = dir + "\\appNexusDash.exe";
    DWORD attribs = GetFileAttributesA(uiPath.c_str());
    if (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        if (CreateProcessA(uiPath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return;
        }
    }
    std::cout << "Modern UI not found. Please run appNexusDash.exe or use CLI options." << std::endl;
}

int main(int argc, char* argv[]) {
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    Parsec_Initialize();

    std::vector<std::string> args(argv, argv + argc);
    auto interfaces = Network::NetworkManager::EnumerateInterfaces();
    std::string selectedIp = "127.0.0.1";

    if (args.size() == 1) {
        RunLauncher();
        return 0;
    }

    ParsecConfig config = {};
    config.bitrate = 5000;
    config.fps = 60;
    config.useHardwareEncoding = true;

    if (args[1] == "--list") {
        for (size_t i = 0; i < interfaces.size(); ++i) {
            std::cout << "[" << i << "] " << interfaces[i].name << " (" << interfaces[i].ip << ")" << std::endl;
        }
        return 0;
    } else if (args[1] == "--host") {
        config.isHost = true;
        strcpy(config.selectedIp, selectedIp.c_str());
        Parsec_StartSession(config);
    } else if (args.size() > 2 && args[1] == "--client") {
        config.isHost = false;
        strcpy(config.selectedIp, selectedIp.c_str());
        strcpy(config.hostIp, args[2].c_str());
        Parsec_StartSession(config);
    } else {
        ShowUsage();
        return 0;
    }

    std::cout << "Session running. Press Enter to stop." << std::endl;
    std::cin.get();
    Parsec_StopSession();
    Parsec_Shutdown();
    return 0;
}
#else
int main() { return 0; }
#endif
