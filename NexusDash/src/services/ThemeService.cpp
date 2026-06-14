#include "ThemeService.hpp"
#include "common/config.hpp"

ThemeService::ThemeService(QObject *parent) : QObject(parent)
{
    m_darkMode = Config::getInstance().getBool("ui.darkMode", true);
}

bool ThemeService::darkMode() const
{
    return m_darkMode;
}

void ThemeService::setDarkMode(bool dark)
{
    if (m_darkMode != dark) {
        m_darkMode = dark;
        Config::getInstance().setBool("ui.darkMode", dark);
        Config::getInstance().save("config.ini");
        emit darkModeChanged();
    }
}
