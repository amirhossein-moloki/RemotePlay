#pragma once

#include <vector>
#include <string>
#include <map>
#include <cstdint>

namespace AI {

struct RouteCandidate {
    std::string id;
    std::string ip;
    uint16_t port;
    float measuredLatencyMs;
    float packetLoss;
    int regionalLoad; // 0-100
    float stabilityTrend; // -1.0 to 1.0 (Positive is improving)
};

struct RouteScore {
    std::string id;
    float score; // Higher is better
};

class IntelligentRouter {
public:
    RouteCandidate SelectBestRoute(const std::vector<RouteCandidate>& candidates);
    std::vector<RouteScore> RankRoutes(const std::vector<RouteCandidate>& candidates);

    // AI Path Switching: Re-evaluate if we should switch routes
    bool ShouldSwitchRoute(const RouteCandidate& current, const RouteCandidate& better, float threshold = 0.20f);

private:
    float CalculateScore(const RouteCandidate& candidate);
};

} // namespace AI
