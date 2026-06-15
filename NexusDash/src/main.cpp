#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "core/AppEngine.hpp"
#include "common/parsec_lite_api.h"

int main(int argc, char *argv[])
{
    Parsec_Initialize();
    QGuiApplication app(argc, argv);

    app.setOrganizationName("NexusDash");
    app.setApplicationName("NexusDash");

    AppEngine backend;
    QQmlApplicationEngine engine;

    backend.initialize(&engine);

    const QUrl url(u"qrc:/qt/qml/NexusDash/qml/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    int result = app.exec();
    Parsec_Shutdown();
    return result;
}
