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

public:
    explicit SystemService(QObject *parent = nullptr);

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

private:
    void updateStats();

    double m_cpuUsage = 0.0;
    double m_memoryUsage = 0.0;
    ParsecTelemetry m_stats = {};
    QTimer *m_timer;
    qint64 m_startTime;
};
