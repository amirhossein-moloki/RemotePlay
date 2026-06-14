#pragma once

#include <QObject>

class ThemeService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)

public:
    explicit ThemeService(QObject *parent = nullptr);

    bool darkMode() const;
    void setDarkMode(bool dark);

signals:
    void darkModeChanged();

private:
    bool m_darkMode = true;
};
