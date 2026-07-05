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
};

struct RouteScore {
    std::string id;
    float score; // Higher is better
};

class IntelligentRouter {
public:
    RouteCandidate SelectBestRoute(const std::vector<RouteCandidate>& candidates);
    std::vector<RouteScore> RankRoutes(const std::vector<RouteCandidate>& candidates);

private:
    float CalculateScore(const RouteCandidate& candidate);
};

} // namespace AI
