#include "overlay.hpp"
#include "ui_system.hpp"
#ifdef _WIN32
#include "third_party/imgui/imgui.h"
#endif
#include "common/profiler.hpp"
#include <iomanip>
#include <sstream>

namespace Client {

bool Overlay::m_visible = true;

void Overlay::Render() {
#ifdef _WIN32
    if (!m_visible) return;

    UI::ApplyTheme();
    ImGui::SetNextWindowPos(ImVec2(18, 18), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Performance", &m_visible, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
        auto& profiler = Profiler::getInstance();

        auto e2e = profiler.getStats("EndToEnd_Latency");
        auto capture = profiler.getStats("Capture_Time");
        auto encode = profiler.getStats("Encode_Time");
        auto network = profiler.getStats("Network_Latency");
        auto decode = profiler.getStats("Decode_Time");
        auto present = profiler.getStats("Present_Time");

        ImGui::TextColored(ImVec4(0.20f, 0.56f, 0.95f, 1.00f), "Session health");
        ImGui::Separator();
        ImGui::Text("End-to-end latency");
        ImGui::SameLine();
        ImGui::TextColored(e2e.avg / 1000.0f <= 20.0f ? ImVec4(0.24f, 0.72f, 0.46f, 1.0f) : ImVec4(0.95f, 0.62f, 0.25f, 1.0f), "%.2f ms", e2e.avg / 1000.0f);
        ImGui::TextDisabled("P99 %.2f ms", e2e.p99 / 1000.0f);
        ImGui::Spacing();

        ImGui::Text("Pipeline");
        ImGui::BulletText("Capture  %.2f ms", capture.avg / 1000.0f);
        ImGui::BulletText("Encode   %.2f ms", encode.avg / 1000.0f);
        ImGui::BulletText("Network  %.2f ms", network.avg / 1000.0f);
        ImGui::BulletText("Decode   %.2f ms", decode.avg / 1000.0f);
        ImGui::BulletText("Present  %.2f ms", present.avg / 1000.0f);

        static float lastTime = 0;
        static int frames = 0;
        static float fps = 0;
        frames++;
        float currentTime = (float)ImGui::GetTime();
        if (currentTime - lastTime >= 1.0f) {
            fps = (float)frames / (currentTime - lastTime);
            frames = 0;
            lastTime = currentTime;
        }

        auto loss = profiler.getStats("Network_LossRate");
        auto rtt = profiler.getStats("Network_RTT");
        auto bitrate = profiler.getStats("Host_Bitrate");

        ImGui::Separator();
        ImGui::Text("Network");
        ImGui::BulletText("FPS %.1f", fps);
        ImGui::BulletText("Packet loss %.2f%%", loss.latest * 100.0f);
        ImGui::BulletText("RTT %.0f ms", rtt.latest);
        ImGui::BulletText("Bitrate %.2f Mbps", bitrate.latest / 1000.0f);

        ImGui::Separator();
        if (ImGui::Button("Hide telemetry", ImVec2(-1, 34))) m_visible = false;
    }
    ImGui::End();
#endif
}

} // namespace Client
