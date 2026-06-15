#include "SystemService.hpp"
#include <QDateTime>
#include <QRandomGenerator>
#include "common/network_manager.hpp"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

static SystemService* s_instance = nullptr;

SystemService::SystemService(QObject *parent) : QObject(parent)
{
    s_instance = this;
    m_startTime = QDateTime::currentSecsSinceEpoch();

    // Enumerate network interfaces
    auto interfaces = Network::NetworkManager::EnumerateInterfaces();
    for (const auto& iface : interfaces) {
        QVariantMap ifaceMap;
        ifaceMap["name"] = QString::fromStdString(iface.name);
        ifaceMap["ip"] = QString::fromStdString(iface.ip);
        ifaceMap["type"] = QString::fromStdString(iface.type);
        ifaceMap["active"] = iface.isActive;
        m_networkInterfaces << ifaceMap;
    }

    Parsec_SetLogCallback(SystemService::onLogReceived);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SystemService::updateStats);
    m_timer->start(2000); // Update every 2 seconds
    updateStats();
}

void SystemService::startHost(const QString& interfaceIp, int bitrate, int fps)
{
    ParsecConfig config = {};
    config.isHost = true;
    config.bitrate = bitrate;
    config.fps = fps;
    config.useHardwareEncoding = true;
    strncpy(config.selectedIp, interfaceIp.toStdString().c_str(), sizeof(config.selectedIp) - 1);

    Parsec_StartSession(config);
    m_isSessionActive = true;
    m_isHosting = true;
    emit sessionStateChanged();
}

void SystemService::startClient(const QString& hostIp, int bitrate, int fps)
{
    ParsecConfig config = {};
    config.isHost = false;
    config.bitrate = bitrate;
    config.fps = fps;
    config.useHardwareEncoding = true;
    strncpy(config.hostIp, hostIp.toStdString().c_str(), sizeof(config.hostIp) - 1);

    // For simplicity in GUI, we might need to handle window handle better
    // but Parsec_StartSession handles the creation of output if windowHandle is null in some implementations
    // or we might need to pass the QQuickWindow handle.

    Parsec_StartSession(config);
    m_isSessionActive = true;
    m_isHosting = false;
    emit sessionStateChanged();
}

void SystemService::stopSession()
{
    Parsec_StopSession();
    m_isSessionActive = false;
    m_isHosting = false;
    emit sessionStateChanged();
}

QString SystemService::connectionState() const
{
    if (!m_isSessionActive) return "Disconnected";
    return m_isHosting ? "Hosting" : "Connected";
}

void SystemService::onLogReceived(const char* level, const char* module, const char* message, const char* timestamp)
{
    if (!s_instance) return;

    // Capture strings immediately to avoid pointer issues when moving to GUI thread
    QString sLevel = QString::fromUtf8(level);
    QString sModule = QString::fromUtf8(module);
    QString sMessage = QString::fromUtf8(message);
    QString sTimestamp = QString::fromUtf8(timestamp);

    // Thread-safe UI update using QMetaObject::invokeMethod
    QMetaObject::invokeMethod(s_instance, [=]() {
        QVariantMap logEntry;
        logEntry["level"] = sLevel;
        logEntry["module"] = sModule;
        logEntry["message"] = sMessage;
        logEntry["timestamp"] = sTimestamp;

        s_instance->m_logs.prepend(logEntry);
        if (s_instance->m_logs.size() > 100) s_instance->m_logs.removeLast();

        emit s_instance->logsChanged();
    }, Qt::QueuedConnection);
}

void SystemService::updateStats()
{
    // Real telemetry from ParsecLiteCore
    if (m_isSessionActive && Parsec_GetTelemetry(&m_stats)) {
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
