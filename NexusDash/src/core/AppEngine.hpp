#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include "../services/ThemeService.hpp"
#include "../services/SystemService.hpp"

class AppEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ThemeService* theme READ theme CONSTANT)
    Q_PROPERTY(SystemService* system READ system CONSTANT)

public:
    explicit AppEngine(QObject *parent = nullptr);
    virtual ~AppEngine() = default;

    static AppEngine* instance();

    ThemeService* theme() const { return m_theme; }
    SystemService* system() const { return m_system; }

    void initialize(QQmlApplicationEngine *engine);

private:
    ThemeService *m_theme;
    SystemService *m_system;
};
