#include "launcher.hpp"
#include "ui_system.hpp"
#ifdef _WIN32
#include "third_party/imgui/imgui.h"
#endif
#include <vector>
#include <cstring>

namespace Client {

void Launcher::Render(LauncherConfig& config, const std::vector<Network::InterfaceInfo>& interfaces) {
#ifdef _WIN32
    UI::ApplyTheme();
    const auto validation = UI::ValidateLauncherConfig(config, interfaces);
    const bool hasErrors = UI::HasBlockingErrors(validation);

    ImGui::SetNextWindowPos(ImVec2(80, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 720), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Parsec-Lite", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ImVec4(0.20f, 0.56f, 0.95f, 1.00f), "Parsec-Lite");
        ImGui::SameLine();
        ImGui::TextDisabled("LAN streaming control center");
        ImGui::Separator();
        ImGui::Spacing();

        if (hasErrors) {
            UI::RenderNotification({UI::Severity::Error, "Setup is incomplete", "Resolve the highlighted fields before starting a streaming session."});
        } else {
            UI::RenderNotification({UI::Severity::Success, "Ready to start", config.isHost ? "This device can host once the session starts." : "Connection settings are complete."});
        }

        UI::BeginCard("mode-card", 116.0f);
        UI::SectionHeader("Session mode", "Choose whether this device shares its desktop or connects to another host.");
        if (ImGui::RadioButton("Host desktop", config.isHost)) config.isHost = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("Connect as client", !config.isHost)) config.isHost = false;
        UI::EndCard();

        UI::BeginCard("network-card", config.isHost ? 156.0f : 204.0f);
        UI::SectionHeader("Network", "Select the local interface and enter the host address when connecting as a client.");
        const char* preview = config.selectedIp.empty() ? "Select interface" : config.selectedIp.c_str();
        if (ImGui::BeginCombo("Local interface", preview)) {
            for (const auto& iface : interfaces) {
                bool isSelected = (config.selectedIp == iface.ip);
                std::string label = iface.name + "  —  " + iface.ip;
                if (ImGui::Selectable(label.c_str(), isSelected)) config.selectedIp = iface.ip;
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        UI::RenderValidationMessages(validation, "interface");

        if (!config.isHost) {
            char hostBuf[128] = {};
            strncpy(hostBuf, config.hostIp.c_str(), sizeof(hostBuf) - 1);
            if (ImGui::InputTextWithHint("Host IP address", "Example: 192.168.1.25", hostBuf, sizeof(hostBuf))) config.hostIp = hostBuf;
            UI::RenderValidationMessages(validation, "hostIp");
        } else {
            ImGui::TextDisabled("Clients connect to this device on UDP port 5005.");
        }
        UI::EndCard();

        UI::BeginCard("quality-card", 216.0f);
        UI::SectionHeader("Stream quality", "Balance smoothness, quality, and latency for the current network.");
        if (ImGui::Button("Low latency", ImVec2(0, 34))) { config.bitrate = 5000; config.fps = 60; }
        ImGui::SameLine();
        if (ImGui::Button("Balanced", ImVec2(0, 34))) { config.bitrate = 8000; config.fps = 60; }
        ImGui::SameLine();
        if (ImGui::Button("Quality", ImVec2(0, 34))) { config.bitrate = 12000; config.fps = 90; }
        ImGui::SliderInt("Bitrate", &config.bitrate, 1000, 20000, "%d kbps", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderInt("Target frame rate", &config.fps, 30, 144, "%d FPS", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox("Use hardware encoding", &config.useHardwareEncoding);
        UI::RenderValidationMessages(validation, "quality");
        UI::EndCard();

        ImGui::TextDisabled("Tip: start with Balanced, then increase quality after confirming stable latency in the Performance overlay.");
        ImGui::Spacing();
        if (hasErrors) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.72f, 0.46f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.80f, 0.53f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.17f, 0.58f, 0.35f, 1.00f));
        if (ImGui::Button(config.isHost ? "Start hosting" : "Connect to host", ImVec2(-1, UI::Tokens().primaryActionHeight))) config.startRequested = true;
        ImGui::PopStyleColor(3);
        if (hasErrors) ImGui::EndDisabled();
    }
    ImGui::End();
#endif
}

} // namespace Client
