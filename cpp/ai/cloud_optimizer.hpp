#pragma once

#include <vector>
#include <string>
#include <map>
#include <cstdint>

namespace AI {

struct NodeStats {
    std::string nodeId;
    float gpuUsage;       // 0.0 to 1.0
    float vramUsage;      // 0.0 to 1.0
    int activeSessions;
    int maxSessions;
    uint32_t regionId;
};

struct ScalingSignal {
    bool scaleUp;
    int recommendedNodes;
    std::string reason;
};

class CloudOptimizer {
public:
    CloudOptimizer();

    // Infrastructure Optimization
    ScalingSignal AnalyzeRegionalLoad(uint32_t regionId, const std::vector<NodeStats>& nodes);
    std::string SelectOptimalNode(const std::vector<NodeStats>& nodes, int requiredVramMb);

    // Session Packing
    float CalculatePackingEfficiency(const std::vector<NodeStats>& nodes);

private:
    std::map<uint32_t, std::vector<float>> m_regionalHistory;
    const float SCALE_UP_THRESHOLD = 0.75f; // Scale up if average load > 75%
};

} // namespace AI
