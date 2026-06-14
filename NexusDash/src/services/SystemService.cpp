#include "SystemService.hpp"
#include <QDateTime>
#include <QRandomGenerator>

#ifdef _WIN32
#include <windows.h>
#endif

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
    // Real telemetry from ParsecLiteCore
    if (Parsec_GetTelemetry(&m_stats)) {
        emit statsChanged();
    }

    // Actual system metrics (Simplified for cross-platform demonstration)
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    m_memoryUsage = 100.0 - (100.0 * memInfo.ullAvailPhys / memInfo.ullTotalPhys);

    // CPU usage would require more complex WinAPI (PdhQueries),
    // using a more stable simulation for now or just keeping it at telemetry level.
    m_cpuUsage = QRandomGenerator::global()->generateDouble() * 10.0 + 5.0;
#else
    m_cpuUsage = 5.0;
    m_memoryUsage = 20.0;
#endif

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
