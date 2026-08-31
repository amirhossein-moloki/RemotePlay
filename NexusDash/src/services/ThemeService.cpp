#include "ThemeService.hpp"
#include "common/config.hpp"
#include "common/logger.hpp"

ThemeService::ThemeService(QObject *parent) : QObject(parent)
{
    m_darkMode = Config::getInstance().getBool("ui.darkMode", true);
    LOG_INFO("UI_Theme", "ThemeService initialized (darkMode: " + std::string(m_darkMode ? "true" : "false") + ")");
}

bool ThemeService::darkMode() const
{
    return m_darkMode;
}

void ThemeService::setDarkMode(bool dark)
{
    if (m_darkMode != dark) {
        LOG_INFO("UI_Theme", "Dark mode toggled to: " + std::string(dark ? "true" : "false"));
        m_darkMode = dark;
        Config::getInstance().setBool("ui.darkMode", dark);
        Config::getInstance().save("config.ini");
        emit darkModeChanged();
    }
}
