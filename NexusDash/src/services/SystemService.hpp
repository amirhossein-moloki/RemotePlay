#pragma once

#include <QObject>
#include <QVariantMap>
#include <QTimer>

class SystemService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(double memoryUsage READ memoryUsage NOTIFY memoryUsageChanged)
    Q_PROPERTY(QString uptime READ uptime NOTIFY uptimeChanged)

public:
    explicit SystemService(QObject *parent = nullptr);

    double cpuUsage() const { return m_cpuUsage; }
    double memoryUsage() const { return m_memoryUsage; }
    QString uptime() const;

signals:
    void cpuUsageChanged();
    void memoryUsageChanged();
    void uptimeChanged();

private:
    void updateStats();

    double m_cpuUsage = 0.0;
    double m_memoryUsage = 0.0;
    QTimer *m_timer;
    qint64 m_startTime;
};
