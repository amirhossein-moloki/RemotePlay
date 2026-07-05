#include "intelligent_router.hpp"
#include <algorithm>

namespace AI {

RouteCandidate IntelligentRouter::SelectBestRoute(const std::vector<RouteCandidate>& candidates) {
    if (candidates.empty()) return {};

    auto ranked = RankRoutes(candidates);
    std::string bestId = ranked[0].id;

    for (const auto& c : candidates) {
        if (c.id == bestId) return c;
    }
    return candidates[0];
}

std::vector<RouteScore> IntelligentRouter::RankRoutes(const std::vector<RouteCandidate>& candidates) {
    std::vector<RouteScore> scores;
    for (const auto& c : candidates) {
        scores.push_back({c.id, CalculateScore(c)});
    }

    std::sort(scores.begin(), scores.end(), [](const RouteScore& a, const RouteScore& b) {
        return a.score > b.score;
    });

    return scores;
}

float IntelligentRouter::CalculateScore(const RouteCandidate& c) {
    // Scoring weights (Hardened for WAN - Aligned with docs/PHASE2_WAN_HARDENING.md)
    const float LATENCY_WEIGHT = 0.6f;
    const float LOSS_WEIGHT = 0.3f;
    const float STABILITY_WEIGHT = 0.1f;

    // Normalize latency (assuming 250ms is "worst" acceptable for WAN)
    float latScore = std::max(0.0f, 1.0f - (c.measuredLatencyMs / 250.0f));

    // Normalize loss (assuming 15% is "worst" for WAN with FEC)
    float lossScore = std::max(0.0f, 1.0f - (c.packetLoss / 0.15f));

    // Stability score: Based on regional load as a proxy for shared-medium congestion
    float stabilityScore = 1.0f - (c.regionalLoad / 100.0f);

    return (latScore * LATENCY_WEIGHT) +
           (lossScore * LOSS_WEIGHT) +
           (stabilityScore * STABILITY_WEIGHT);
}

} // namespace AI
