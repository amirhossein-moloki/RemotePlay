#include "SystemService.hpp"
#include <QDateTime>
#include <QRandomGenerator>

SystemService::SystemService(QObject *parent) : QObject(parent)
{
    m_startTime = QDateTime::currentSecsSinceEpoch();
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SystemService::updateStats);
    m_timer->start(2000); // Update every 2 seconds
    updateStats();
}

void SystemService::updateStats()
{
    // Simulated data
    m_cpuUsage = QRandomGenerator::global()->generateDouble() * 40.0 + 5.0;
    m_memoryUsage = QRandomGenerator::global()->generateDouble() * 60.0 + 20.0;

    emit cpuUsageChanged();
    emit memoryUsageChanged();
    emit uptimeChanged();
}

QString SystemService::uptime() const
{
    qint64 diff = QDateTime::currentSecsSinceEpoch() - m_startTime;
    int hours = diff / 3600;
    int minutes = (diff % 3600) / 60;
    int seconds = diff % 60;
    return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
}
