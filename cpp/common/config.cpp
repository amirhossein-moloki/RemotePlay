#include "config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

bool Config::load(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filename = filename;
    m_lastLoadTime = std::chrono::system_clock::now();

    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        // Remove comments
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) line = line.substr(0, commentPos);

        size_t sep = line.find('=');
        if (sep != std::string::npos) {
            std::string key = line.substr(0, sep);
            std::string value = line.substr(sep + 1);

            // Trim
            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t\r\n"));
                s.erase(s.find_last_not_of(" \t\r\n") + 1);
            };
            trim(key);
            trim(value);

            if (!key.empty()) m_settings[key] = value;
        }
    }
    return true;
}

void Config::checkHotReload() {
    if (m_filename.empty()) return;
    try {
        auto lastWriteTime = std::filesystem::last_write_time(m_filename);
        auto lastWriteTimeClock = std::chrono::time_point_cast<std::chrono::system_clock::duration>(lastWriteTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());

        if (lastWriteTimeClock > m_lastLoadTime) {
            load(m_filename);
        }
    } catch (...) {}
}

bool Config::save(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    for (const auto& [key, value] : m_settings) {
        file << key << "=" << value << std::endl;
    }
    return true;
}

std::string Config::getString(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_settings.find(key);
    return (it != m_settings.end()) ? it->second : defaultValue;
}

int Config::getInt(const std::string& key, int defaultValue) {
    std::string val = getString(key, "");
    if (val.empty()) return defaultValue;
    try { return std::stoi(val); } catch (...) { return defaultValue; }
}

float Config::getFloat(const std::string& key, float defaultValue) {
    std::string val = getString(key, "");
    if (val.empty()) return defaultValue;
    try { return std::stof(val); } catch (...) { return defaultValue; }
}

bool Config::getBool(const std::string& key, bool defaultValue) {
    std::string val = getString(key, "");
    if (val.empty()) return defaultValue;
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    return (val == "true" || val == "1" || val == "yes");
}

void Config::setString(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings[key] = value;
}

void Config::setInt(const std::string& key, int value) {
    setString(key, std::to_string(value));
}

void Config::setFloat(const std::string& key, float value) {
    setString(key, std::to_string(value));
}

void Config::setBool(const std::string& key, bool value) {
    setString(key, value ? "true" : "false");
}
