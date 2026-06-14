#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "core/AppBridge.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("ParsecLite");
    app.setOrganizationDomain("parseclite.io");
    app.setApplicationName("ParsecLiteQt");

    AppBridge bridge;

    QQmlApplicationEngine engine;

    // Expose the bridge to QML
    engine.rootContext()->setContextProperty("appBridge", &bridge);

    // In Qt 6, qt_add_qml_module puts resources under /qt/qml/URI/
    const QUrl url(u"qrc:/qt/qml/ParsecLite/UI/src/ui/main.qml"_qs);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
