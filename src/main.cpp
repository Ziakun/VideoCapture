#include "app/CaptureController.h"
#include "ui/VideoPreviewItem.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setApplicationName(QStringLiteral("MeetVideoCapture"));
    QCoreApplication::setOrganizationName(QStringLiteral("MeetVideoCapture"));

    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    qmlRegisterType<VideoPreviewItem>("MeetVideoCapture", 1, 0, "VideoPreviewItem");

    CaptureController controller;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("captureController"), &controller);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []()
        {
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);

    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    return app.exec();
}
