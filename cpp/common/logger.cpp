#include "logger.hpp"
#include "config.hpp"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#endif

#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>

#ifdef PARSEC_LITE_ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

// File-scoped thread-local session ID variable
static thread_local std::string t_threadSessionId = "00000000";

Logger::Logger() {
    m_runtimeLogLevel.store(LogLevel::LL_TRACE, std::memory_order_relaxed);
}

Logger::~Logger() {
    shutdown();
}

uint32_t Logger::getCurrentPid() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

uint32_t Logger::getCurrentTid() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
#ifdef __linux__
    return (uint32_t)syscall(SYS_gettid);
#else
    return (uint32_t)(uintptr_t)pthread_self();
#endif
#endif
}

void Logger::init(const std::string& role, const std::string& logDir) {
    std::lock_guard<std::mutex> lock(m_initMutex);

    if (m_initialized && m_role == role) {
        return;
    }

    // Shut down any existing worker thread
    if (m_running) {
        m_running = false;
        m_queueCond.notify_all();
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
    }

    m_role = role;
    m_logDir = logDir;

    try {
        std::filesystem::create_directories(m_logDir);
        std::filesystem::create_directories(m_logDir + "/" + role);
    } catch (...) {
        m_logDir = ".";
    }

    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &now_time);
#else
    localtime_r(&now_time, &timeinfo);
#endif

    std::stringstream ss;
    std::string prefix = "app";
    if (role == "Host") prefix = "host";
    else if (role == "Client") prefix = "client";
    else if (role == "Standalone") prefix = "standalone";

    ss << m_logDir << "/" << role << "/" << prefix << "_"
       << std::put_time(&timeinfo, "%Y-%m-%d_%H-%M-%S")
       << "_PID" << getCurrentPid() << ".log";

    m_baseFilename = ss.str();
    m_initialized = true;

    openFile();
    writeSnapshot();

    m_running = true;
    m_workerThread = std::thread(&Logger::workerLoop, this);
}

void Logger::shutdown() {
    if (m_running) {
        m_running = false;
        m_queueCond.notify_all();
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
    }
    std::lock_guard<std::mutex> lock(m_fileMutex);
    if (m_file.is_open()) {
        m_file.close();
    }
}

void Logger::setThreadSessionId(uint64_t sessionId) {
    if (sessionId == 0) {
        t_threadSessionId = "00000000";
        return;
    }
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << (sessionId & 0xFFFFFFFF);
    t_threadSessionId = ss.str();
}

void Logger::setThreadSessionId(const std::string& sessionIdStr) {
    t_threadSessionId = sessionIdStr;
}

std::string Logger::getThreadSessionId() {
    if (t_threadSessionId.empty()) {
        return "00000000";
    }
    return t_threadSessionId;
}

void Logger::log(LogLevel level, const std::string& module, const std::string& message,
                 const char* file, int line, const char* function) {
    if (level < m_runtimeLogLevel.load(std::memory_order_relaxed)) {
        return;
    }

    // Filter high-frequency streaming events at lower levels to reduce pipeline overhead
    if ((module == "StreamTrace" || module == "ClientTrace") && level < LogLevel::LL_WARN) {
        return;
    }

    LogRecord record;
    record.timestamp = std::chrono::system_clock::now();
    record.processId = getCurrentPid();
    record.threadId = getCurrentTid();
    record.sessionId = getThreadSessionId();
    record.module = module;
    record.level = level;
    record.file = file ? file : "";
    record.line = line;
    record.function = function ? function : "";
    record.message = message;

    if (level >= LogLevel::LL_CRITICAL) {
        // Critical or Fatal logs: write synchronously and flush immediately
        // Uses writeRecord which is protected under m_fileMutex to prevent data races
        writeRecord(record);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_queue.size() < 10000) {
            m_queue.push(std::move(record));
        }
    }
    m_queueCond.notify_one();
}

void Logger::workerLoop() {
    while (m_running) {
        LogRecord record;
        bool hasRecord = false;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCond.wait(lock, [this]() { return !m_queue.empty() || !m_running; });
            if (!m_queue.empty()) {
                record = std::move(m_queue.front());
                m_queue.pop();
                hasRecord = true;
            }
        }

        if (hasRecord) {
            writeRecord(record);
        }
    }

    // Drain queue on termination
    while (true) {
        LogRecord record;
        bool hasRecord = false;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_queue.empty()) {
                record = std::move(m_queue.front());
                m_queue.pop();
                hasRecord = true;
            }
        }
        if (!hasRecord) break;
        writeRecord(record);
    }
}

void Logger::writeRecord(const LogRecord& record) {
    auto now_time = std::chrono::system_clock::to_time_t(record.timestamp);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(record.timestamp.time_since_epoch()) % 1000;

    struct tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &now_time);
#else
    localtime_r(&now_time, &timeinfo);
#endif

    std::string filename = record.file;
    size_t lastSlash = filename.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        filename = filename.substr(lastSlash + 1);
    }

    std::string sessId = record.sessionId;
    if (sessId.empty()) sessId = "00000000";

    std::stringstream ss;
    ss << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "."
       << std::setfill('0') << std::setw(3) << now_ms.count() << "] "
       << "[PID:" << record.processId << "] "
       << "[TID:" << record.threadId << "] "
       << "[Session:" << sessId << "] "
       << "[" << record.module << "] "
       << "[" << levelToString(record.level) << "] "
       << filename << ":" << record.line << " "
       << record.function << "() "
       << record.message << "\n";

    std::string logLine = ss.str();

    if (record.level >= LogLevel::LL_WARN) {
        std::cout << logLine;
    }

    // Acquire lock to fully synchronize concurrent file operations and rotation
    std::lock_guard<std::mutex> lock(m_fileMutex);

    if (m_file.is_open()) {
        m_file << logLine;
        m_currentFileSize += logLine.size();

        if (m_currentFileSize >= m_maxFileSize.load(std::memory_order_relaxed)) {
            rotateFiles();
        } else if (record.level >= LogLevel::LL_WARN) {
            m_file.flush();
        }
    }
}

void Logger::openFile() {
    // Note: Called under m_initMutex or m_fileMutex depending on execution path
    if (m_file.is_open()) m_file.close();
    m_file.open(m_baseFilename, std::ios::app);
    if (m_file.is_open()) {
        m_currentFileSize = (size_t)std::filesystem::file_size(m_baseFilename);
    }
}

void Logger::rotateFiles() {
    // Note: Called under m_fileMutex
    m_file.close();
    int historyLimit = m_maxFiles.load(std::memory_order_relaxed);
    for (int i = historyLimit - 1; i > 0; --i) {
        std::string oldFile = m_baseFilename + "." + std::to_string(i);
        std::string newFile = m_baseFilename + "." + std::to_string(i + 1);
        if (std::filesystem::exists(oldFile)) {
            std::filesystem::rename(oldFile, newFile);
        }
    }
    if (std::filesystem::exists(m_baseFilename)) {
        std::error_code ec;
        std::filesystem::rename(m_baseFilename, m_baseFilename + ".1", ec);
    }
    openFile();
}

void Logger::writeSnapshot() {
    // Note: Called under m_initMutex or m_fileMutex
    if (!m_file.is_open()) return;

    std::stringstream ss;
    ss << "================================================================================\n";
    ss << "                          CONFIGURATION SNAPSHOT\n";
    ss << "================================================================================\n";
    ss << "Application Mode:     " << m_role << "\n";
    ss << "Build Version:        v1.2.0-Production\n";
    ss << "Process ID (PID):     " << getCurrentPid() << "\n";

#ifdef _WIN32
    ss << "Operating System:     Windows 10/11\n";
#else
    ss << "Operating System:     Linux / POSIX\n";
#endif

#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        ss << "Total Physical RAM:   " << (memInfo.ullTotalPhys / (1024 * 1024)) << " MB\n";
        ss << "Available RAM:        " << (memInfo.ullAvailPhys / (1024 * 1024)) << " MB\n";
    }
#else
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        ss << "Total Physical RAM:   " << (si.totalram * si.mem_unit / (1024 * 1024)) << " MB\n";
        ss << "Available RAM:        " << (si.freeram * si.mem_unit / (1024 * 1024)) << " MB\n";
    }
#endif

#ifdef PARSEC_LITE_ENABLE_FFMPEG
    ss << "FFmpeg Version:       " << av_version_info() << "\n";
#else
    ss << "FFmpeg Version:       Not Enabled / Mocked\n";
#endif

#ifdef QT_VERSION_STR
    ss << "Qt Version:           " << QT_VERSION_STR << "\n";
#else
    ss << "Qt Version:           Not Linked / Static Engine\n";
#endif

    ss << "\n--- active config.ini parameters ---\n";
    ss << "log_level=" << Config::getInstance().getString("log_level", "INFO") << "\n";
    ss << "max_log_file_size_mb=" << Config::getInstance().getInt("max_log_file_size_mb", 100) << "\n";
    ss << "max_log_files=" << Config::getInstance().getInt("max_log_files", 5) << "\n";
    ss << "enable_performance_logging=" << (Config::getInstance().getBool("enable_performance_logging", false) ? "true" : "false") << "\n";
    ss << "username=" << Config::getInstance().getString("username", "User") << "\n";
    ss << "useHardwareEncoding=" << (Config::getInstance().getBool("useHardwareEncoding", true) ? "true" : "false") << "\n";
    ss << "encoderPreset=" << Config::getInstance().getInt("encoderPreset", 0) << "\n";
    ss << "resolutionScale=" << Config::getInstance().getDouble("resolutionScale", 1.0) << "\n";
    ss << "================================================================================\n\n";

    m_file << ss.str();
    m_file.flush();
}

void Logger::loadConfig() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    std::string levelStr = Config::getInstance().getString("log_level", "INFO");
    m_runtimeLogLevel.store(stringToLevel(levelStr), std::memory_order_relaxed);

    size_t sizeLimit = (size_t)Config::getInstance().getInt("max_log_file_size_mb", 100) * 1024 * 1024;
    m_maxFileSize.store(sizeLimit, std::memory_order_relaxed);

    int filesLimit = Config::getInstance().getInt("max_log_files", 5);
    m_maxFiles.store(filesLimit, std::memory_order_relaxed);

    bool perfLogging = Config::getInstance().getBool("enable_performance_logging", false);
    m_enablePerformanceLogging.store(perfLogging, std::memory_order_relaxed);
}

LogLevel Logger::stringToLevel(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    if (str == "TRACE") return LogLevel::LL_TRACE;
    if (str == "DEBUG") return LogLevel::LL_DEBUG;
    if (str == "INFO") return LogLevel::LL_INFO;
    if (str == "WARN" || str == "WARNING") return LogLevel::LL_WARN;
    if (str == "ERROR") return LogLevel::LL_ERROR;
    if (str == "CRITICAL") return LogLevel::LL_CRITICAL;
    if (str == "FATAL") return LogLevel::LL_FATAL;
    return LogLevel::LL_INFO;
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::LL_TRACE:    return "TRACE";
        case LogLevel::LL_DEBUG:    return "DEBUG";
        case LogLevel::LL_INFO:     return "INFO";
        case LogLevel::LL_WARN:     return "WARN";
        case LogLevel::LL_ERROR:    return "ERROR";
        case LogLevel::LL_CRITICAL: return "CRIT";
        case LogLevel::LL_FATAL:    return "FATAL";
        default: return "UNKN";
    }
}
