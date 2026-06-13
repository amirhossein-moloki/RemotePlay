#include "overlay.hpp"
#include "third_party/imgui/imgui.h"
#include "common/profiler.hpp"
#include <iomanip>
#include <sstream>

namespace Client {

bool Overlay::m_visible = true;

void Overlay::Render() {
    if (!m_visible) return;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Parsec-Lite Telemetry", &m_visible, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        auto& profiler = Profiler::getInstance();

        auto e2e = profiler.getStats("EndToEnd_Latency");
        auto capture = profiler.getStats("Capture_Time");
        auto encode = profiler.getStats("Encode_Time");
        auto decode = profiler.getStats("Decode_Time");
        auto present = profiler.getStats("Present_Time");

        ImGui::Text("Total E2E Latency: %.2f ms", e2e.avg / 1000.0f);
        ImGui::Separator();
        ImGui::Text("Capture Time: %.2f ms", capture.avg / 1000.0f);
        ImGui::Text("Encode Time: %.2f ms", encode.avg / 1000.0f);
        ImGui::Text("Decode Time: %.2f ms", decode.avg / 1000.0f);
        ImGui::Text("Present Time: %.2f ms", present.avg / 1000.0f);

        static float lastTime = 0;
        static int frames = 0;
        static float fps = 0;
        frames++;
        float currentTime = ImGui::GetTime();
        if (currentTime - lastTime >= 1.0f) {
            fps = (float)frames / (currentTime - lastTime);
            frames = 0;
            lastTime = currentTime;
        }
        ImGui::Text("FPS: %.1f", fps);

        auto loss = profiler.getStats("Network_LossRate");
        ImGui::Text("Packet Loss: %.2f%%", loss.latest * 100.0f);

        auto rtt = profiler.getStats("Network_RTT");
        ImGui::Text("RTT: %.0f ms", rtt.latest);

        auto bitrate = profiler.getStats("Host_Bitrate");
        ImGui::Text("Bitrate: %.2f Mbps", bitrate.latest / 1000.0f);

        ImGui::Separator();
        if (ImGui::Button("Close Overlay")) m_visible = false;
    }
    ImGui::End();
}

} // namespace Client
