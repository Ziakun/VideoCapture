#include "capture/VideoFrame.h"
#include "config/VideoCaptureConstants.h"
#include "recording/RecordingSettings.h"
#include "recording/VideoRecorder.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTimer>

#include <algorithm>

namespace
{

constexpr int smokeFrameWidth = 320;
constexpr int smokeFrameHeight = 180;
constexpr int smokeBitrateKbps = 1200;
constexpr int smokeTargetFrames = 90;
constexpr int redFrameStep = 3;
constexpr int greenFrameStep = 5;
constexpr int blueFrameStep = 7;
constexpr int greenYMultiplier = 2;
constexpr int patternDivisor = 2;
constexpr int colorModulo = 256;
constexpr int opaqueAlpha = 255;
constexpr int recorderDrainDelayMs = 700;
constexpr int smokeTimeoutMs = 15000;

QString defaultOutputPath()
{
    return QDir::current().absoluteFilePath(QStringLiteral("build/recorder-smoke/recorder-smoke.mkv"));
}

VideoFrame makeFrame(int frameIndex, int width, int height, int fps)
{
    QImage image(width, height, QImage::Format_ARGB32);

    // Use a moving deterministic pattern so the encoder receives non-identical
    // frames and the resulting file is easy to validate with frame counters.
    for (int y = 0; y < height; ++y)
    {
        auto* const line = reinterpret_cast<QRgb*>(image.scanLine(y));

        for (int x = 0; x < width; ++x)
        {
            const int red = (x + frameIndex * redFrameStep) % colorModulo;
            const int green = (y * greenYMultiplier + frameIndex * greenFrameStep) % colorModulo;
            const int blue = (x / patternDivisor + y / patternDivisor + frameIndex * blueFrameStep) % colorModulo;

            line[x] = qRgba(red, green, blue, opaqueAlpha);
        }
    }

    VideoFrame frame;

    frame.image = std::move(image);
    frame.timestampNs =
        static_cast<qint64>(frameIndex) * VideoCaptureConstants::nanosecondsPerSecond / std::max(1, fps);

    return frame;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString outputPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : defaultOutputPath();

    const QFileInfo outputInfo(outputPath);

    if (!QDir().mkpath(outputInfo.absolutePath()))
    {
        qCritical() << "Could not create output directory:" << outputInfo.absolutePath();

        return 2;
    }

    QFile::remove(outputPath);

    RecordingSettings settings;

    settings.outputDirectory = outputInfo.absolutePath();
    settings.filePath = outputPath;
    settings.width = smokeFrameWidth;
    settings.height = smokeFrameHeight;
    settings.fps = VideoCaptureConstants::defaultFps;
    settings.bitrateKbps = smokeBitrateKbps;

    VideoRecorder recorder;
    int exitCode = 0;

    QObject::connect(&recorder, &VideoRecorder::recordingStopped, &app,
                     [&](const QString& filePath)
                     {
                         qInfo() << "Recording finalized:" << filePath;
                         exitCode = 0;
                         app.quit();
                     });
    QObject::connect(&recorder, &VideoRecorder::recordingFailed, &app,
                     [&](const QString& error)
                     {
                         qCritical() << "Recording failed:" << error;
                         exitCode = 3;
                         app.quit();
                     });

    QString startError;

    if (!recorder.start(settings, &startError))
    {
        qCritical() << "Could not start recorder:" << startError;

        return 2;
    }

    int frameIndex = 0;

    auto* const frameTimer = new QTimer(&app);

    frameTimer->setInterval(VideoCaptureConstants::millisecondsPerSecond / settings.fps);

    QObject::connect(frameTimer, &QTimer::timeout, &app,
                     [&]()
                     {
                         if (frameIndex >= smokeTargetFrames)
                         {
                             frameTimer->stop();

                             // Give the worker a short drain window before stopAsync clears the
                             // bounded queue and sends EOS. This keeps the smoke test stable on
                             // slower machines without changing production recorder behavior.
                             QTimer::singleShot(recorderDrainDelayMs, &app,
                                                [&]()
                                                {
                                                    recorder.stopAsync();
                                                });

                             return;
                         }

                         if (!recorder.pushFrame(makeFrame(frameIndex, settings.width, settings.height, settings.fps)))
                         {
                             qWarning() << "Frame was not accepted by recorder:" << frameIndex;
                         }

                         ++frameIndex;
                     });

    QTimer::singleShot(smokeTimeoutMs, &app,
                       [&]()
                       {
                           qCritical() << "Timed out waiting for recorder finalization.";
                           exitCode = 4;
                           recorder.stopAsync();
                           app.quit();
                       });

    frameTimer->start();
    app.exec();

    return exitCode;
}
