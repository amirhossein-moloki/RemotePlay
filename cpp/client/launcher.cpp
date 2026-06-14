#include "launcher.hpp"
#include "third_party/imgui/imgui.h"
#include <vector>

namespace Client {

void Launcher::Render(LauncherConfig& config, const std::vector<Network::InterfaceInfo>& interfaces) {
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Parsec-Lite Launcher", nullptr, ImGuiWindowFlags_NoCollapse)) {

        // Operation Mode
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.2f, 1.0f), "Operation Mode");
        ImGui::Separator();
        if (ImGui::RadioButton("Host Mode (Stream this desktop)", config.isHost)) {
            config.isHost = true;
        }
        if (ImGui::RadioButton("Client Mode (Connect to host)", !config.isHost)) {
            config.isHost = false;
        }
        ImGui::Spacing();

        // Network Settings
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.2f, 1.0f), "Network Settings");
        ImGui::Separator();

        if (ImGui::BeginCombo("Interface", config.selectedIp.c_str())) {
            for (const auto& iface : interfaces) {
                bool isSelected = (config.selectedIp == iface.ip);
                if (ImGui::Selectable(iface.name.c_str(), isSelected)) {
                    config.selectedIp = iface.ip;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (!config.isHost) {
            char hostBuf[128];
            strncpy(hostBuf, config.hostIp.c_str(), sizeof(hostBuf));
            if (ImGui::InputText("Host IP Address", hostBuf, sizeof(hostBuf))) {
                config.hostIp = hostBuf;
            }
        }
        ImGui::Spacing();

        // Stream Config
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.2f, 1.0f), "Stream Config");
        ImGui::Separator();
        ImGui::SliderInt("Bitrate (kbps)", &config.bitrate, 1000, 20000);
        ImGui::SliderInt("Target FPS", &config.fps, 30, 144);
        ImGui::Checkbox("Enable Hardware Encoding", &config.useHardwareEncoding);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Styled Button at the bottom
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.24f, 0.31f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.29f, 0.37f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 0.2f, 1.0f));

        if (ImGui::Button("START SESSION", ImVec2(-1, 40))) {
            config.startRequested = true;
        }

        ImGui::PopStyleColor(3);
    }
    ImGui::End();
}

} // namespace Client
