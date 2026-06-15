#pragma once

#include <string>
#include <map>
#include <mutex>
#include <chrono>

#include "parsec_lite_api.h"

class PARSEC_API Config {
public:
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    bool load(const std::string& filename);
    bool save(const std::string& filename);
    void checkHotReload();

    std::string getString(const std::string& key, const std::string& defaultValue);
    int getInt(const std::string& key, int defaultValue);
    float getFloat(const std::string& key, float defaultValue);
    bool getBool(const std::string& key, bool defaultValue);

    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setFloat(const std::string& key, float value);
    void setBool(const std::string& key, bool value);

private:
    Config() = default;
    std::map<std::string, std::string> m_settings;
    std::string m_filename;
    std::chrono::system_clock::time_point m_lastLoadTime;
    std::mutex m_mutex;
};
