#include "AppEngine.hpp"
#include <QQmlContext>

static AppEngine* s_instance = nullptr;

AppEngine::AppEngine(QObject *parent) : QObject(parent)
{
    s_instance = this;
    m_theme = new ThemeService(this);
    m_system = new SystemService(this);
}

AppEngine* AppEngine::instance()
{
    return s_instance;
}

void AppEngine::initialize(QQmlApplicationEngine *engine)
{
    engine->rootContext()->setContextProperty("backend", this);
}
