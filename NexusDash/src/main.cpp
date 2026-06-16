#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QDebug>
#include <QString>
#include "core/AppEngine.hpp"
#include "common/parsec_lite_api.h"
#include "common/logger.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

void showFatalError(const QString& title, const QString& message) {
    LOG_ERROR("Fatal", message.toStdString());
#ifdef _WIN32
    MessageBoxW(NULL, (LPCWSTR)message.utf16(), (LPCWSTR)title.utf16(), MB_OK | MB_ICONERROR);
#else
    qCritical() << title << ":" << message;
#endif
}

int main(int argc, char *argv[])
{
    try {
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
            if (!obj && url == objUrl) {
                showFatalError("NexusDash Startup Error",
                    "Failed to load user interface components. Please ensure the application is correctly installed.");
                QCoreApplication::exit(-1);
            }
        }, Qt::QueuedConnection);

        engine.load(url);

        int result = app.exec();
        Parsec_Shutdown();
        return result;

    } catch (const std::exception& e) {
        QString errorMsg = QString("Unhandled C++ exception: %1").arg(e.what());
        showFatalError("NexusDash Fatal Error", errorMsg);
        Parsec_Shutdown();
        return -1;
    } catch (...) {
        showFatalError("NexusDash Fatal Error", "An unknown fatal error occurred.");
        Parsec_Shutdown();
        return -1;
    }
}
