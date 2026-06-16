#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QDebug>
#include "core/AppEngine.hpp"
#include "common/parsec_lite_api.h"
#include "common/logger.hpp"

int main(int argc, char *argv[])
{
    Parsec_Initialize();
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);

    app.setOrganizationName("NexusDash");
    app.setApplicationName("NexusDash");

    AppEngine backend;
    QQmlApplicationEngine engine;

    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &warn : warnings) {
            LOG_ERROR("QML", warn.toString().toStdString());
        }
    });

    backend.initialize(&engine);

    const QUrl url(u"qrc:/qt/qml/NexusDash/qml/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    int result = 0;
    try {
        result = app.exec();
    } catch (const std::exception& e) {
        LOG_ERROR("Global", std::string("Unhandled C++ exception: ") + e.what());
        result = -1;
    } catch (...) {
        LOG_ERROR("Global", "Unknown unhandled exception occurred.");
        result = -1;
    }

    Parsec_Shutdown();
    return result;
}
