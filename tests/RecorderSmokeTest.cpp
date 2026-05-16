#include "capture/VideoFrame.h"
#include "recording/RecordingSettings.h"
#include "recording/VideoRecorder.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTimer>

#include <algorithm>

namespace {

QString defaultOutputPath()
{
    return QDir::current().absoluteFilePath(QStringLiteral("build/recorder-smoke/recorder-smoke.mkv"));
}

VideoFrame makeFrame(int frameIndex, int width, int height, int fps)
{
    QImage image(width, height, QImage::Format_ARGB32);

    // Use a moving deterministic pattern so the encoder receives non-identical
    // frames and the resulting file is easy to validate with frame counters.
    for (int y = 0; y < height; ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const int red = (x + frameIndex * 3) % 256;
            const int green = (y * 2 + frameIndex * 5) % 256;
            const int blue = (x / 2 + y / 2 + frameIndex * 7) % 256;
            line[x] = qRgba(red, green, blue, 255);
        }
    }

    VideoFrame frame;
    frame.image = std::move(image);
    frame.timestampNs = static_cast<qint64>(frameIndex) * 1000000000LL / std::max(1, fps);
    return frame;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString outputPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : defaultOutputPath();
    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        qCritical() << "Could not create output directory:" << outputInfo.absolutePath();
        return 2;
    }
    QFile::remove(outputPath);

    RecordingSettings settings;
    settings.outputDirectory = outputInfo.absolutePath();
    settings.filePath = outputPath;
    settings.width = 320;
    settings.height = 180;
    settings.fps = 30;
    settings.bitrateKbps = 1200;

    VideoRecorder recorder;
    int exitCode = 0;

    QObject::connect(&recorder, &VideoRecorder::recordingStopped, &app, [&](const QString& filePath) {
        qInfo() << "Recording finalized:" << filePath;
        exitCode = 0;
        app.quit();
    });
    QObject::connect(&recorder, &VideoRecorder::recordingFailed, &app, [&](const QString& error) {
        qCritical() << "Recording failed:" << error;
        exitCode = 3;
        app.quit();
    });

    QString startError;
    if (!recorder.start(settings, &startError)) {
        qCritical() << "Could not start recorder:" << startError;
        return 2;
    }

    constexpr int targetFrames = 90;
    int frameIndex = 0;
    auto* frameTimer = new QTimer(&app);
    frameTimer->setInterval(1000 / settings.fps);

    QObject::connect(frameTimer, &QTimer::timeout, &app, [&]() {
        if (frameIndex >= targetFrames) {
            frameTimer->stop();

            // Give the worker a short drain window before stopAsync clears the
            // bounded queue and sends EOS. This keeps the smoke test stable on
            // slower machines without changing production recorder behavior.
            QTimer::singleShot(700, &app, [&]() {
                recorder.stopAsync();
            });
            return;
        }

        if (!recorder.pushFrame(makeFrame(frameIndex, settings.width, settings.height, settings.fps))) {
            qWarning() << "Frame was not accepted by recorder:" << frameIndex;
        }
        ++frameIndex;
    });

    QTimer::singleShot(15000, &app, [&]() {
        qCritical() << "Timed out waiting for recorder finalization.";
        exitCode = 4;
        recorder.stopAsync();
        app.quit();
    });

    frameTimer->start();
    app.exec();
    return exitCode;
}
