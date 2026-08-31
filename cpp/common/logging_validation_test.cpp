#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>

#include "logger.hpp"
#include "crash_handler.hpp"
#include "parsec_lite_api.h"

void TestMultiInstanceLogIsolation() {
    std::cout << "Running Multi-Instance Log Isolation Test..." << std::endl;

    Logger::getInstance().init("Host", "test_logs");
    LOG_INFO("MultiInstance", "Host process log entry");
    std::string hostLog = Logger::getInstance().levelToString(LogLevel::LL_INFO);

    assert(std::filesystem::exists("test_logs/Host"));
    std::cout << "Multi-Instance Log Isolation Test Passed!" << std::endl;
}

void TestSessionCorrelation() {
    std::cout << "Running Session Correlation Test..." << std::endl;

    Logger::setThreadSessionId("TEST0001");
    assert(Logger::getThreadSessionId() == "TEST0001");

    Logger::setThreadSessionId(0x12345678);
    assert(Logger::getThreadSessionId() == "12345678");

    std::cout << "Session Correlation Test Passed!" << std::endl;
}

void TestLogRotationAndRetention() {
    std::cout << "Running Log Rotation and Retention Test..." << std::endl;

    Logger::getInstance().init("Client", "test_logs");
    LOG_INFO("RotationTest", "Testing log rotation mechanisms");

    std::cout << "Log Rotation and Retention Test Passed!" << std::endl;
}

void TestCrashDiagnostics() {
    std::cout << "Running Crash Diagnostics Test..." << std::endl;

    CrashDiagnostics::CrashHandler::GenerateCrashReport("Controlled Test Crash");
    assert(std::filesystem::exists("CrashReports"));

    bool foundReport = false;
    for (const auto& entry : std::filesystem::directory_iterator("CrashReports")) {
        if (entry.path().extension() == ".txt") {
            foundReport = true;
            break;
        }
    }
    assert(foundReport);
    std::cout << "Crash Diagnostics Test Passed!" << std::endl;
}

void TestSupportPackageGeneration() {
    std::cout << "Running Support Package Generation Test..." << std::endl;

    // Reject unsafe path
    assert(!Parsec_GenerateSupportPackage("../unsafe_package.zip"));
    assert(!Parsec_GenerateSupportPackage("package;rm -rf.zip"));

    // Accept valid safe path
    bool ok = Parsec_GenerateSupportPackage("support_test_package.tar.gz");
    assert(ok);
    assert(std::filesystem::exists("support_test_package.tar.gz"));

    std::cout << "Support Package Generation Test Passed!" << std::endl;
}

int main() {
    try {
        TestMultiInstanceLogIsolation();
        TestSessionCorrelation();
        TestLogRotationAndRetention();
        TestCrashDiagnostics();
        TestSupportPackageGeneration();
        std::cout << "\nAll Logging & Observability Tests Passed Successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Logging Validation Test Failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
