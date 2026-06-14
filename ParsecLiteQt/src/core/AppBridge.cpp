#include "AppBridge.hpp"
#include <QDebug>

AppBridge::AppBridge(QObject *parent)
    : QObject(parent)
    , m_userName("Jules Engineer")
    , m_darkMode(true)
{
    // Mock data for recent sessions
    m_recentSessions << QVariantMap({{"name", "Gaming PC"}, {"ip", "192.168.1.15"}, {"lastUsed", "2 hours ago"}});
    m_recentSessions << QVariantMap({{"name", "Work Station"}, {"ip", "10.0.0.42"}, {"lastUsed", "Yesterday"}});
    m_recentSessions << QVariantMap({{"name", "Media Server"}, {"ip", "192.168.1.100"}, {"lastUsed", "3 days ago"}});
}

QString AppBridge::userName() const
{
    return m_userName;
}

void AppBridge::setUserName(const QString &name)
{
    if (m_userName != name) {
        m_userName = name;
        emit userNameChanged();
    }
}

bool AppBridge::darkMode() const
{
    return m_darkMode;
}

void AppBridge::setDarkMode(bool enable)
{
    if (m_darkMode != enable) {
        m_darkMode = enable;
        emit darkModeChanged();
    }
}

QVariantList AppBridge::recentSessions() const
{
    return m_recentSessions;
}

void AppBridge::connectToHost(const QString &ip)
{
    qDebug() << "Connecting to host:" << ip;
    emit connectionStarted(ip);
}

void AppBridge::startHosting()
{
    qDebug() << "Starting hosting session...";
    emit hostingStarted();
}

void AppBridge::toggleTheme()
{
    setDarkMode(!m_darkMode);
}
