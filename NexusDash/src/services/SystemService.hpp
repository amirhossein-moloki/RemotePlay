#pragma once

#include <QObject>
#include <QVariantMap>
#include <QTimer>
#include <QAbstractListModel>
#include <QDateTime>
#include "common/parsec_lite_api.h"

struct LogEntry {
    QString level;
    QString event;
    QString desc;
    QString time;
};

class LogModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        LevelRole = Qt::UserRole + 1,
        EventRole,
        DescRole,
        TimeRole
    };

    LogModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    void addLog(const QString& level, const QString& event, const QString& desc) {
        beginInsertRows(QModelIndex(), 0, 0);
        m_logs.prepend({level, event, desc, QDateTime::currentDateTime().toString("hh:mm:ss")});
        if (m_logs.size() > 100) m_logs.removeLast();
        endInsertRows();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override { return m_logs.size(); }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= m_logs.size()) return QVariant();
        const auto& entry = m_logs.at(index.row());
        switch (role) {
            case LevelRole: return entry.level;
            case EventRole: return entry.event;
            case DescRole: return entry.desc;
            case TimeRole: return entry.time;
        }
        return QVariant();
    }
    QHash<int, QByteArray> roleNames() const override {
        return {
            {LevelRole, "level"},
            {EventRole, "event"},
            {DescRole, "desc"},
            {TimeRole, "time"}
        };
    }

private:
    QList<LogEntry> m_logs;
};

class SystemService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(double memoryUsage READ memoryUsage NOTIFY memoryUsageChanged)
    Q_PROPERTY(QString uptime READ uptime NOTIFY uptimeChanged)
    Q_PROPERTY(double e2eLatency READ e2eLatency NOTIFY statsChanged)
    Q_PROPERTY(double fps READ fps NOTIFY statsChanged)
    Q_PROPERTY(double bitrate READ bitrate NOTIFY statsChanged)
    Q_PROPERTY(double lossRate READ lossRate NOTIFY statsChanged)
    Q_PROPERTY(double rtt READ rtt NOTIFY statsChanged)
    Q_PROPERTY(QStringList networkInterfaces READ networkInterfaces NOTIFY networkInterfacesChanged)
    Q_PROPERTY(bool isSessionActive READ isSessionActive NOTIFY sessionStateChanged)
    Q_PROPERTY(QAbstractListModel* logModel READ logModel CONSTANT)

    // Historical data for graphs
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList memoryHistory READ memoryHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList fpsHistory READ fpsHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList latencyHistory READ latencyHistory NOTIFY historyChanged)

public:
    explicit SystemService(QObject *parent = nullptr);

    Q_INVOKABLE void startHost(const QString& interfaceInfo, int bitrate, int fps);
    Q_INVOKABLE void startClient(const QString& interfaceInfo, const QString& hostIp, int bitrate, int fps);
    Q_INVOKABLE void stopSession();

    double cpuUsage() const { return m_cpuUsage; }
    double memoryUsage() const { return m_memoryUsage; }
    QString uptime() const;
    double e2eLatency() const { return m_stats.e2eLatency; }
    double fps() const { return m_stats.fps; }
    double bitrate() const { return m_stats.bitrateMbps; }
    double lossRate() const { return m_stats.lossRate; }
    double rtt() const { return m_stats.rtt; }
    QStringList networkInterfaces() const { return m_networkInterfaces; }
    bool isSessionActive() const { return m_isSessionActive; }

    QAbstractListModel* logModel() const { return m_logModel; }

    QVariantList cpuHistory() const { return m_cpuHistory; }
    QVariantList memoryHistory() const { return m_memoryHistory; }
    QVariantList fpsHistory() const { return m_fpsHistory; }
    QVariantList latencyHistory() const { return m_latencyHistory; }

signals:
    void cpuUsageChanged();
    void memoryUsageChanged();
    void uptimeChanged();
    void statsChanged();
    void networkInterfacesChanged();
    void sessionStateChanged();
    void historyChanged();

private:
    void updateStats();
    void addLog(const QString& level, const QString& event, const QString& desc);
    void updateHistory(QVariantList& list, double value);

    double m_cpuUsage = 0.0;
    double m_memoryUsage = 0.0;
    bool m_isSessionActive = false;
    QStringList m_networkInterfaces;
    ParsecTelemetry m_stats = {};
    QTimer *m_timer;
    qint64 m_startTime;
    LogModel* m_logModel;

    QVariantList m_cpuHistory;
    QVariantList m_memoryHistory;
    QVariantList m_fpsHistory;
    QVariantList m_latencyHistory;
    const int m_maxHistory = 60;
};
