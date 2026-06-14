#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "core/AppEngine.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("NexusDash");
    app.setApplicationName("NexusDash");

    AppEngine backend;
    QQmlApplicationEngine engine;

    backend.initialize(&engine);

    const QUrl url(u"qrc:/NexusDash/qml/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
