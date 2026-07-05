#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include "latency_predictor.hpp"
#include "stream_engine.hpp"
#include "intelligent_router.hpp"
#include "input_predictor.hpp"

void testLatencyPredictor() {
    std::cout << "Testing LatencyPredictor..." << std::endl;
    AI::LatencyPredictor predictor(10);

    // Simulate increasing RTT
    for (int i = 0; i < 20; ++i) {
        AI::NetworkMetrics m;
        m.rttMs = 20.0f + i * 5.0f;
        m.packetLossRate = 0.01f;
        m.timestamp = i * 100;
        predictor.RecordMetrics(m);
    }

    auto pred = predictor.Predict(200);
    std::cout << "  Predicted RTT: " << pred.predictedRttMs << "ms (Expected > 100ms)" << std::endl;
    assert(pred.predictedRttMs > 100.0f);
    assert(pred.confidenceScore > 0.7f);
    std::cout << "  LatencyPredictor passed." << std::endl;
}

void testStreamEngine() {
    std::cout << "Testing StreamEngine..." << std::endl;
    AI::StreamEngine engine;
    AI::StreamState state = { 5000, 1920, 1080, 60, 0.0f, 30.0f };

    // Normal state
    AI::LatencyPrediction pred = { 30.0f, 2.0f, 0.0f, 1.0f };
    auto decision = engine.Analyze(state, pred);
    assert(decision.action == AI::AdaptationAction::Maintain);

    // Predicted Congestion
    pred.predictedLossRate = 0.20f;
    decision = engine.Analyze(state, pred);
    assert(decision.action == AI::AdaptationAction::EmergencyThrottle);
    assert(decision.targetBitrateKbps < 5000);

    std::cout << "  StreamEngine passed." << std::endl;
}

void testIntelligentRouter() {
    std::cout << "Testing IntelligentRouter..." << std::endl;
    AI::IntelligentRouter router;

    std::vector<AI::RouteCandidate> candidates = {
        {"region-1", "1.1.1.1", 5005, 50.0f, 0.0f, 10},
        {"region-2", "2.2.2.2", 5005, 20.0f, 0.0f, 80},
        {"region-3", "3.3.3.3", 5005, 150.0f, 0.05f, 5}
    };

    auto best = router.SelectBestRoute(candidates);
    std::cout << "  Best Route: " << best.id << " (Expected region-2 due to low latency)" << std::endl;
    assert(best.id == "region-2");

    std::cout << "  IntelligentRouter passed." << std::endl;
}

void testInputPredictor() {
    std::cout << "Testing InputPredictor..." << std::endl;
    AI::InputPredictor predictor;

    predictor.RecordInput(100, 100, 1000);
    predictor.RecordInput(110, 110, 1010);

    auto pred = predictor.Predict(1020);
    std::cout << "  Predicted X: " << pred.x << " (Expected 120)" << std::endl;
    assert(pred.x == 120);
    assert(pred.y == 120);

    std::cout << "  InputPredictor passed." << std::endl;
}

int main() {
    try {
        testLatencyPredictor();
        testStreamEngine();
        testIntelligentRouter();
        testInputPredictor();
        std::cout << "\nAll AI Unit Tests Passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
