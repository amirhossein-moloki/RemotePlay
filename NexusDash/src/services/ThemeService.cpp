#include "ThemeService.hpp"

ThemeService::ThemeService(QObject *parent) : QObject(parent)
{
}

bool ThemeService::darkMode() const
{
    return m_darkMode;
}

void ThemeService::setDarkMode(bool dark)
{
    if (m_darkMode != dark) {
        m_darkMode = dark;
        emit darkModeChanged();
    }
}
