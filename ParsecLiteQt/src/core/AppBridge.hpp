#ifndef APPBRIDGE_HPP
#define APPBRIDGE_HPP

#include <QObject>
#include <QString>
#include <QVariantList>

class AppBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(QVariantList recentSessions READ recentSessions NOTIFY recentSessionsChanged)

public:
    explicit AppBridge(QObject *parent = nullptr);

    QString userName() const;
    void setUserName(const QString &name);

    bool darkMode() const;
    void setDarkMode(bool enable);

    QVariantList recentSessions() const;

    Q_INVOKABLE void connectToHost(const QString &ip);
    Q_INVOKABLE void startHosting();
    Q_INVOKABLE void toggleTheme();

signals:
    void userNameChanged();
    void darkModeChanged();
    void recentSessionsChanged();
    void connectionStarted(const QString &ip);
    void hostingStarted();

private:
    QString m_userName;
    bool m_darkMode;
    QVariantList m_recentSessions;
};

#endif // APPBRIDGE_HPP
