#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>
#include <filesystem>
#include <queue>
#include <condition_variable>
#include <algorithm>
#include <atomic>

#include "parsec_lite_api.h"

enum class LogLevel {
    LL_TRACE = 0,
    LL_DEBUG,
    LL_INFO,
    LL_WARN,
    LL_ERROR,
    LL_CRITICAL,
    LL_FATAL
};

// Compile-time filtering limit (Release build optimizes away lower log levels)
#ifndef LOG_LEVEL_LIMIT
#ifdef NDEBUG
#define LOG_LEVEL_LIMIT LogLevel::LL_INFO
#else
#define LOG_LEVEL_LIMIT LogLevel::LL_TRACE
#endif
#endif

struct LogRecord {
    std::chrono::system_clock::time_point timestamp;
    uint32_t processId;
    uint32_t threadId;
    std::string sessionId;
    std::string module;
    LogLevel level;
    std::string file;
    int line;
    std::string function;
    std::string message;
};

class PARSEC_API Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // Initialize the logger for a specific role (Host, Client, Standalone, App)
    void init(const std::string& role = "App", const std::string& logDir = "logs");

    // Dynamic load configuration from config.ini
    void loadConfig();

    // Sets thread-local session ID
    static void setThreadSessionId(uint64_t sessionId);
    static void setThreadSessionId(const std::string& sessionIdStr);
    static std::string getThreadSessionId();

    // Logging function
    void log(LogLevel level, const std::string& module, const std::string& message,
             const char* file = "", int line = 0, const char* function = "");

    // Shutdown the logging thread gracefully
    void shutdown();

    // Check if performance logging is enabled (thread-safe lock-free read)
    bool isPerformanceLoggingEnabled() const { return m_enablePerformanceLogging.load(std::memory_order_relaxed); }

    // Converts log level to a clean string
    static const char* levelToString(LogLevel level);

    // Converts string to LogLevel
    static LogLevel stringToLevel(std::string str);

private:
    Logger();
    ~Logger();

    void openFile();
    void rotateFiles();
    void workerLoop();
    void writeRecord(const LogRecord& record);
    void writeSnapshot();

    // Platform-dependent ID getters
    static uint32_t getCurrentPid();
    static uint32_t getCurrentTid();

    // Instance metadata
    std::string m_role = "App";
    std::string m_logDir = "logs";
    std::string m_baseFilename;
    std::ofstream m_file;
    size_t m_currentFileSize = 0;

    // Thread-safe config parameters (Atomics to avoid data races)
    std::atomic<size_t> m_maxFileSize{100 * 1024 * 1024}; // Default 100 MB
    std::atomic<int> m_maxFiles{5}; // Default history limit
    std::atomic<bool> m_enablePerformanceLogging{false};
    std::atomic<LogLevel> m_runtimeLogLevel{LogLevel::LL_TRACE};

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;

    // Queue synchronization
    std::queue<LogRecord> m_queue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCond;

    // Mutexes
    std::mutex m_initMutex;
    std::mutex m_configMutex;
    std::mutex m_fileMutex; // Protects file writing & rotation from races
};

// Thread-safe Macros with Compile-Time Filtering
#define LOG_TRACE(mod, msg)    do { if (LogLevel::LL_TRACE >= LOG_LEVEL_LIMIT) Logger::getInstance().log(LogLevel::LL_TRACE, mod, msg, __FILE__, __LINE__, __FUNCTION__); } while(0)
#define LOG_DEBUG(mod, msg)    do { if (LogLevel::LL_DEBUG >= LOG_LEVEL_LIMIT) Logger::getInstance().log(LogLevel::LL_DEBUG, mod, msg, __FILE__, __LINE__, __FUNCTION__); } while(0)
#define LOG_INFO(mod, msg)     do { if (LogLevel::LL_INFO >= LOG_LEVEL_LIMIT)  Logger::getInstance().log(LogLevel::LL_INFO, mod, msg, __FILE__, __LINE__, __FUNCTION__); } while(0)
#define LOG_WARN(mod, msg)     do { if (LogLevel::LL_WARN >= LOG_LEVEL_LIMIT)  Logger::getInstance().log(LogLevel::LL_WARN, mod, msg, __FILE__, __LINE__, __FUNCTION__); } while(0)
#define LOG_ERROR(mod, msg)    do { if (LogLevel::LL_ERROR >= LOG_LEVEL_LIMIT) Logger::getInstance().log(LogLevel::LL_ERROR, mod, msg, __FILE__, __LINE__, __FUNCTION__); } while(0)
#define LOG_CRITICAL(mod, msg) do { if (LogLevel::LL_CRITICAL >= LOG_LEVEL_LIMIT) Logger::getInstance().log(LogLevel::LL_CRITICAL, mod, msg, __FILE__, __LINE__, __FUNCTION__); } while(0)
#define LOG_FATAL(mod, msg)    do { if (LogLevel::LL_FATAL >= LOG_LEVEL_LIMIT) Logger::getInstance().log(LogLevel::LL_FATAL, mod, msg, __FILE__, __LINE__, __FUNCTION__); } while(0)
