#pragma once

#include <QObject>
#include <QVariantMap>
#include <QTimer>
#include "common/parsec_lite_api.h"

class SystemService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(double memoryUsage READ memoryUsage NOTIFY memoryUsageChanged)
    Q_PROPERTY(QString uptime READ uptime NOTIFY uptimeChanged)
    Q_PROPERTY(double e2eLatency READ e2eLatency NOTIFY statsChanged)
    Q_PROPERTY(double fps READ fps NOTIFY statsChanged)
    Q_PROPERTY(double bitrate READ bitrate NOTIFY statsChanged)
    Q_PROPERTY(QVariantList networkInterfaces READ networkInterfaces NOTIFY networkInterfacesChanged)
    Q_PROPERTY(bool isSessionActive READ isSessionActive NOTIFY sessionStateChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY sessionStateChanged)
    Q_PROPERTY(QVariantList logs READ logs NOTIFY logsChanged)

public:
    explicit SystemService(QObject *parent = nullptr);

    Q_INVOKABLE void startHost(const QString& interfaceIp, int bitrate, int fps);
    Q_INVOKABLE void startClient(const QString& hostIp, int bitrate, int fps);
    Q_INVOKABLE void stopSession();

    double cpuUsage() const { return m_cpuUsage; }
    double memoryUsage() const { return m_memoryUsage; }
    QString uptime() const;
    double e2eLatency() const { return m_stats.e2eLatency; }
    double fps() const { return m_stats.fps; }
    double bitrate() const { return m_stats.bitrateMbps; }

signals:
    void cpuUsageChanged();
    void memoryUsageChanged();
    void uptimeChanged();
    void statsChanged();
    void networkInterfacesChanged();
    void sessionStateChanged();
    void logsChanged();

public:
    QVariantList networkInterfaces() const { return m_networkInterfaces; }
    bool isSessionActive() const { return m_isSessionActive; }
    QString connectionState() const;
    QVariantList logs() const { return m_logs; }

private:
    void updateStats();
    static void onLogReceived(const char* level, const char* module, const char* message, const char* timestamp);

    double m_cpuUsage = 0.0;
    double m_memoryUsage = 0.0;
    bool m_isSessionActive = false;
    bool m_isHosting = false;
    QVariantList m_networkInterfaces;
    QVariantList m_logs;
    ParsecTelemetry m_stats = {};
    QTimer *m_timer;
    qint64 m_startTime;
};
