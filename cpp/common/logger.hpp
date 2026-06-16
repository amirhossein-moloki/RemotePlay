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

enum class LogLevel {
    LL_DEBUG,
    LL_INFO,
    LL_WARN,
    LL_ERROR
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void init(const std::string& filename, size_t maxFileSize = 5 * 1024 * 1024, int maxFiles = 3) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_baseFilename = filename;
        m_maxFileSize = maxFileSize;
        m_maxFiles = maxFiles;
        openFile();
    }

    void log(LogLevel level, const std::string& module, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        struct tm timeinfo;
#ifdef _WIN32
        localtime_s(&timeinfo, &now_time);
#else
        localtime_r(&now_time, &timeinfo);
#endif

        std::stringstream ss;
        ss << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "."
           << std::setfill('0') << std::setw(3) << now_ms.count() << "] "
           << "[" << std::this_thread::get_id() << "] "
           << "[" << levelToString(level) << "] "
           << "[" << module << "] "
           << message << std::endl;

        std::string logLine = ss.str();

        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << logLine;
        if (m_file.is_open()) {
            m_file << logLine;
            m_currentFileSize += logLine.size();

            if (m_currentFileSize >= m_maxFileSize) {
                rotateFiles();
            } else {
                m_file.flush();
            }
        }
    }

private:
    Logger() = default;
    ~Logger() {
        if (m_file.is_open()) m_file.close();
    }

    void openFile() {
        if (m_file.is_open()) m_file.close();
        m_file.open(m_baseFilename, std::ios::app);
        if (m_file.is_open()) {
            m_currentFileSize = (size_t)std::filesystem::file_size(m_baseFilename);
        }
    }

    void rotateFiles() {
        m_file.close();
        for (int i = m_maxFiles - 1; i > 0; --i) {
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

    const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::LL_DEBUG: return "DEBUG";
            case LogLevel::LL_INFO:  return "INFO ";
            case LogLevel::LL_WARN:  return "WARN ";
            case LogLevel::LL_ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    std::string m_baseFilename;
    std::ofstream m_file;
    size_t m_maxFileSize = 5 * 1024 * 1024;
    size_t m_currentFileSize = 0;
    int m_maxFiles = 3;
    std::mutex m_mutex;
};

#define LOG_DEBUG(mod, msg) Logger::getInstance().log(LogLevel::LL_DEBUG, mod, msg)
#define LOG_INFO(mod, msg)  Logger::getInstance().log(LogLevel::LL_INFO, mod, msg)
#define LOG_WARN(mod, msg)  Logger::getInstance().log(LogLevel::LL_WARN, mod, msg)
#define LOG_ERROR(mod, msg) Logger::getInstance().log(LogLevel::LL_ERROR, mod, msg)
