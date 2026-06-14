#include "ui_system.hpp"
#include <algorithm>

#ifdef _WIN32
#include "third_party/imgui/imgui.h"
#endif

namespace Client::UI {

namespace {
const DesignTokens kTokens{};

#ifdef _WIN32
constexpr ImVec4 kSurfaceBase = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
constexpr ImVec4 kSurfaceCard = ImVec4(0.11f, 0.13f, 0.16f, 1.00f);
constexpr ImVec4 kTextPrimary = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);
constexpr ImVec4 kTextMuted = ImVec4(0.63f, 0.70f, 0.78f, 1.00f);
constexpr ImVec4 kAccent = ImVec4(0.20f, 0.56f, 0.95f, 1.00f);
constexpr ImVec4 kSuccess = ImVec4(0.24f, 0.72f, 0.46f, 1.00f);
constexpr ImVec4 kWarning = ImVec4(0.95f, 0.62f, 0.25f, 1.00f);
constexpr ImVec4 kError = ImVec4(0.91f, 0.36f, 0.46f, 1.00f);

ImVec4 SeverityColor(Severity severity) {
    switch (severity) {
    case Severity::Success: return kSuccess;
    case Severity::Warning: return kWarning;
    case Severity::Error: return kError;
    case Severity::Info:
    default: return kAccent;
    }
}

const char* SeverityLabel(Severity severity) {
    switch (severity) {
    case Severity::Success: return "Ready";
    case Severity::Warning: return "Warning";
    case Severity::Error: return "Action needed";
    case Severity::Info:
    default: return "Info";
    }
}
#endif
} // namespace

const DesignTokens& Tokens() {
    return kTokens;
}

void ApplyTheme() {
#ifdef _WIN32
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = kTokens.radiusLg;
    style.ChildRounding = kTokens.radiusMd;
    style.FrameRounding = kTokens.radiusSm;
    style.GrabRounding = kTokens.radiusSm;
    style.PopupRounding = kTokens.radiusMd;
    style.ScrollbarRounding = kTokens.radiusMd;
    style.WindowPadding = ImVec2(kTokens.spaceXl, 22.0f);
    style.FramePadding = ImVec2(kTokens.spaceMd, 9.0f);
    style.ItemSpacing = ImVec2(kTokens.spaceMd, kTokens.spaceMd);
    style.ItemInnerSpacing = ImVec2(10.0f, kTokens.spaceSm);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = kSurfaceBase;
    colors[ImGuiCol_ChildBg] = kSurfaceCard;
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.29f, 0.35f, 1.00f);
    colors[ImGuiCol_Text] = kTextPrimary;
    colors[ImGuiCol_TextDisabled] = kTextMuted;
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.19f, 0.23f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.25f, 0.31f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.32f, 0.42f, 1.00f);
    colors[ImGuiCol_CheckMark] = kAccent;
    colors[ImGuiCol_SliderGrab] = kAccent;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.31f, 0.66f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = kAccent;
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.27f, 0.63f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.13f, 0.42f, 0.76f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.16f, 0.29f, 0.43f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.19f, 0.36f, 0.55f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.23f, 0.45f, 0.70f, 1.00f);
#endif
}

std::vector<ValidationMessage> ValidateLauncherConfig(const LauncherConfig& config, const std::vector<Network::InterfaceInfo>& interfaces) {
    std::vector<ValidationMessage> messages;
    if (interfaces.empty()) {
        messages.push_back({"interface", "No network interfaces were found. Check adapter status, VPNs, and firewall permissions, then restart the launcher.", Severity::Error});
    } else {
        const bool selectedExists = std::any_of(interfaces.begin(), interfaces.end(), [&](const Network::InterfaceInfo& iface) {
            return iface.ip == config.selectedIp;
        });
        if (!selectedExists) {
            messages.push_back({"interface", "Select a valid local network interface before starting a session.", Severity::Error});
        }
    }

    if (!config.isHost && config.hostIp.empty()) {
        messages.push_back({"hostIp", "Enter the host IP address before connecting.", Severity::Error});
    }

    if (config.bitrate < 1000 || config.bitrate > 20000) {
        messages.push_back({"quality", "Bitrate must stay between 1,000 and 20,000 kbps.", Severity::Error});
    }
    if (config.fps < 30 || config.fps > 144) {
        messages.push_back({"quality", "Frame rate must stay between 30 and 144 FPS.", Severity::Error});
    }
    if (config.bitrate >= 16000 && config.fps >= 120) {
        messages.push_back({"quality", "High bitrate plus high FPS can increase latency on congested LANs.", Severity::Warning});
    }

    return messages;
}

bool HasBlockingErrors(const std::vector<ValidationMessage>& messages) {
    return std::any_of(messages.begin(), messages.end(), [](const ValidationMessage& message) {
        return message.severity == Severity::Error;
    });
}

#ifdef _WIN32
void SectionHeader(const char* title, const char* description) {
    ImGui::TextColored(kAccent, "%s", title);
    ImGui::TextColored(kTextMuted, "%s", description);
    ImGui::Spacing();
}

void BeginCard(const char* id, float height) {
    ImGui::BeginChild(id, ImVec2(0, height), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
}

void EndCard() {
    ImGui::EndChild();
    ImGui::Spacing();
}

void RenderNotification(const Notification& notification) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.16f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, SeverityColor(notification.severity));
    ImGui::BeginChild(("notification-" + notification.title).c_str(), ImVec2(0, 74.0f), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
    ImGui::TextColored(SeverityColor(notification.severity), "%s", SeverityLabel(notification.severity));
    ImGui::SameLine();
    ImGui::Text("%s", notification.title.c_str());
    ImGui::TextColored(kTextMuted, "%s", notification.message.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
}

void RenderValidationMessages(const std::vector<ValidationMessage>& messages, const char* field) {
    for (const auto& message : messages) {
        if (message.field == field) {
            ImGui::TextColored(SeverityColor(message.severity), "%s: %s", SeverityLabel(message.severity), message.message.c_str());
        }
    }
}
#endif

} // namespace Client::UI
