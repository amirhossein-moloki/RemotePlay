#include "cloud_optimizer.hpp"
#include <algorithm>
#include <numeric>

namespace AI {

CloudOptimizer::CloudOptimizer() {}

ScalingSignal CloudOptimizer::AnalyzeRegionalLoad(uint32_t regionId, const std::vector<NodeStats>& nodes) {
    if (nodes.empty()) return {true, 1, "No nodes active in region"};

    float totalGpuLoad = 0;
    for (const auto& node : nodes) {
        totalGpuLoad += node.gpuUsage;
    }

    float avgLoad = totalGpuLoad / nodes.size();

    // Store history for trend analysis (simplified)
    m_regionalHistory[regionId].push_back(avgLoad);
    if (m_regionalHistory[regionId].size() > 10) {
        m_regionalHistory[regionId].erase(m_regionalHistory[regionId].begin());
    }

    ScalingSignal signal;
    signal.scaleUp = (avgLoad > SCALE_UP_THRESHOLD);

    if (signal.scaleUp) {
        signal.recommendedNodes = (int)(nodes.size() * 0.2f) + 1; // 20% growth
        signal.reason = "Average GPU load (" + std::to_string((int)(avgLoad * 100)) + "%) exceeds threshold";
    } else {
        signal.recommendedNodes = 0;
        signal.reason = "Load stable";
    }

    return signal;
}

std::string CloudOptimizer::SelectOptimalNode(const std::vector<NodeStats>& nodes, int requiredVramMb) {
    // Session Packing AI: Select node with highest load that still fits the session
    // This maximizes utilization and leaves empty nodes for potential scale-down
    std::string bestNodeId = "";
    float highestLoad = -1.0f;

    for (const auto& node : nodes) {
        // Assume 8GB baseline for simplification
        float availableVram = (1.0f - node.vramUsage) * 8192;
        if (availableVram >= requiredVramMb && node.activeSessions < node.maxSessions) {
            if (node.gpuUsage > highestLoad) {
                highestLoad = node.gpuUsage;
                bestNodeId = node.nodeId;
            }
        }
    }

    // Fallback to least loaded if packing fails
    if (bestNodeId.empty()) {
        float lowestLoad = 2.0f;
        for (const auto& node : nodes) {
            if (node.gpuUsage < lowestLoad) {
                lowestLoad = node.gpuUsage;
                bestNodeId = node.nodeId;
            }
        }
    }

    return bestNodeId;
}

float CloudOptimizer::CalculatePackingEfficiency(const std::vector<NodeStats>& nodes) {
    if (nodes.empty()) return 1.0f;

    float totalSessions = 0;
    float totalCapacity = 0;
    for (const auto& node : nodes) {
        totalSessions += node.activeSessions;
        totalCapacity += node.maxSessions;
    }

    return (totalSessions / totalCapacity);
}

} // namespace AI
