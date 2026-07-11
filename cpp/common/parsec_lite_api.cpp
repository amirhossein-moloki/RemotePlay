#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "parsec_lite_api.h"
#include "session_manager.hpp"
#include "logger.hpp"
#include "config.hpp"
#include "crash_handler.hpp"
#include "profiler.hpp"
#include "network_manager.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <sys/sysinfo.h>
#endif

extern "C" {

PARSEC_API bool Parsec_Initialize() {
    Logger::getInstance().init("App");
    Config::getInstance().load("config.ini");
#ifdef _WIN32
    CrashDiagnostics::CrashHandler::Initialize();
#endif
    return true;
}

PARSEC_API void Parsec_StartSession(ParsecConfig config) {
    SessionManager::getInstance().startSession(config);
}

PARSEC_API void Parsec_StopSession() {
    SessionManager::getInstance().stopSession();
}

PARSEC_API bool Parsec_GetTelemetry(ParsecTelemetry* outTelemetry) {
    return SessionManager::getInstance().getTelemetry(outTelemetry);
}

PARSEC_API void Parsec_Shutdown() {
    SessionManager::getInstance().stopSession();
}

PARSEC_API void Parsec_SetConnectionCallback(ParsecConnectionCallback callback) {
    SessionManager::getInstance().setConnectionCallback(callback);
}

PARSEC_API void Parsec_SetErrorCallback(ParsecErrorCallback callback) {
    SessionManager::getInstance().setErrorCallback(callback);
}

PARSEC_API void Parsec_ApproveConnection(const char* ip, uint16_t port, bool approved) {
    SessionManager::getInstance().approveConnection(ip, port, approved);
}

PARSEC_API void Parsec_HandleMessage(uint32_t msg, uint64_t wParam, int64_t lParam) {
    SessionManager::getInstance().handleMessage(msg, wParam, lParam);
}

PARSEC_API void* Parsec_CreateClientWindow(const char* title, int width, int height) {
#ifdef _WIN32
    WNDCLASSA wc = { 0 };
    HINSTANCE hInstance = GetModuleHandleA(NULL);
    const char* className = "ParsecLiteStreamingWindow";

    if (!GetClassInfoA(hInstance, className, &wc)) {
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (msg == WM_DESTROY) {
                PostQuitMessage(0);
                return 0;
            }
            Parsec_HandleMessage(msg, (uint64_t)wParam, (int64_t)lParam);
            return DefWindowProcA(hwnd, msg, wParam, lParam);
        };
        wc.hInstance = hInstance;
        wc.lpszClassName = className;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassA(&wc);
    }

    HWND hwnd = CreateWindowExA(0, className, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);
    return (void*)hwnd;
#else
    return nullptr;
#endif
}

static bool isSafePath(const std::string& path) {
    if (path.empty()) return false;

    // Strict whitelist of characters permitted in the output path
    const std::string allowed = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-/";
    for (char c : path) {
        if (allowed.find(c) == std::string::npos) {
            return false;
        }
    }

    // Block directory traversal or duplicate slashes
    if (path.find("..") != std::string::npos || path.find("//") != std::string::npos) {
        return false;
    }

    return true;
}

PARSEC_API bool Parsec_GenerateSupportPackage(const char* outputPath) {
    if (!outputPath) return false;

    std::string outPathStr = outputPath;
    if (!isSafePath(outPathStr)) {
        LOG_ERROR("Support", "Rejected support package generation: unsafe path characters or command injection signature: " + outPathStr);
        return false;
    }

    std::string reportDir = "SupportReport";

    try {
        std::filesystem::remove_all(reportDir);
        std::filesystem::create_directories(reportDir);
        std::filesystem::create_directories(reportDir + "/Logs");
        std::filesystem::create_directories(reportDir + "/Crash");
        std::filesystem::create_directories(reportDir + "/Configuration");
        std::filesystem::create_directories(reportDir + "/SystemInfo");
        std::filesystem::create_directories(reportDir + "/Telemetry");
        std::filesystem::create_directories(reportDir + "/Network");

        // 1. Copy Logs
        if (std::filesystem::exists("logs")) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator("logs")) {
                if (entry.is_regular_file()) {
                    auto relPath = std::filesystem::relative(entry.path(), "logs");
                    std::filesystem::create_directories(reportDir + "/Logs/" + relPath.parent_path().string());
                    std::filesystem::copy_file(entry.path(), reportDir + "/Logs/" + relPath.string(), std::filesystem::copy_options::overwrite_existing);
                }
            }
        }

        // 2. Copy Crashes
        if (std::filesystem::exists("CrashReports")) {
            for (const auto& entry : std::filesystem::directory_iterator("CrashReports")) {
                if (entry.is_regular_file()) {
                    std::filesystem::copy_file(entry.path(), reportDir + "/Crash/" + entry.path().filename().string(), std::filesystem::copy_options::overwrite_existing);
                }
            }
        }

        // 3. Copy Configuration
        if (std::filesystem::exists("config.ini")) {
            std::filesystem::copy_file("config.ini", reportDir + "/Configuration/config.ini", std::filesystem::copy_options::overwrite_existing);
        }

        // 4. Generate SystemInfo/system_info.txt
        {
            std::ofstream sysInfo(reportDir + "/SystemInfo/system_info.txt");
            if (sysInfo.is_open()) {
                sysInfo << "==================================================\n";
                sysInfo << "           SUPPORT DIAGNOSTICS SYSTEM INFO        \n";
                sysInfo << "==================================================\n";
#ifdef _WIN32
                sysInfo << "OS:                  Windows\n";
#else
                sysInfo << "OS:                  Linux / POSIX\n";
#endif

#ifdef _WIN32
                MEMORYSTATUSEX memInfo;
                memInfo.dwLength = sizeof(MEMORYSTATUSEX);
                if (GlobalMemoryStatusEx(&memInfo)) {
                    sysInfo << "Total Memory:        " << (memInfo.ullTotalPhys / 1024 / 1024) << " MB\n";
                    sysInfo << "Available Memory:    " << (memInfo.ullAvailPhys / 1024 / 1024) << " MB\n";
                }
#else
                struct sysinfo si;
                if (sysinfo(&si) == 0) {
                    sysInfo << "Total Memory:        " << (si.totalram * si.mem_unit / 1024 / 1024) << " MB\n";
                    sysInfo << "Available Memory:    " << (si.freeram * si.mem_unit / 1024 / 1024) << " MB\n";
                }
#endif
                sysInfo << "==================================================\n";
                sysInfo.close();
            }
        }

        // 5. Generate Telemetry/telemetry_history.txt from Profiler
        {
            std::ofstream telFile(reportDir + "/Telemetry/telemetry_history.txt");
            if (telFile.is_open()) {
                auto& profiler = Profiler::getInstance();
                telFile << "=== Profiler Telemetry Dump ===\n";
                telFile << "FPS:                 " << profiler.getStats("FPS").latest << " (p99: " << profiler.getStats("FPS").p99 << ")\n";
                telFile << "RTT:                 " << profiler.getStats("Network_RTT").latest << " ms\n";
                telFile << "Packet Loss Rate:    " << profiler.getStats("Network_LossRate").latest << " %\n";
                telFile << "Host Bitrate:        " << profiler.getStats("Host_Bitrate").latest << " Mbps\n";
                telFile << "EndToEnd Latency:    " << profiler.getStats("EndToEnd_Latency").avg << " us\n";
                telFile << "Capture Time:        " << profiler.getStats("Capture_Time").avg << " us\n";
                telFile << "Encode Time:         " << profiler.getStats("Encode_Time").avg << " us\n";
                telFile << "Decode Time:         " << profiler.getStats("Decode_Time").avg << " us\n";
                telFile.close();
            }
        }

        // 6. Generate Network/network_diagnostics.txt
        {
            std::ofstream netFile(reportDir + "/Network/network_diagnostics.txt");
            if (netFile.is_open()) {
                netFile << "=== Network Interfaces ===\n";
                auto interfaces = Network::NetworkManager::EnumerateInterfaces();
                for (const auto& iface : interfaces) {
                    netFile << "  Interface: " << iface.name << " (" << iface.ip << ")\n";
                }
                netFile.close();
            }
        }

        // Compress SupportReport
        LOG_INFO("Support", "Compressing SupportReport into support diagnostics package: " + outPathStr);
#ifdef _WIN32
        std::string zipCmd = "powershell -NoProfile -Command \"Compress-Archive -Path '" + reportDir + "' -DestinationPath '" + outPathStr + "' -Force\"";
        (void)system(zipCmd.c_str());
#else
        std::string tarCmd = "tar -czf " + outPathStr + " " + reportDir;
        (void)system(tarCmd.c_str());
#endif

        // Clean up temporary report directory
        std::filesystem::remove_all(reportDir);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Support", "Failed to generate support package: " + std::string(e.what()));
        return false;
    }
}

}
