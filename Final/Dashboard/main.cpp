#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "backend.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    OtaProcess otaProcess;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("otaProcess", &otaProcess);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    engine.loadFromModule("Dashboard", "Main");

    return app.exec();
}