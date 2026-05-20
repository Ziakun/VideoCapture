#include "CaptureController.h"

#include "app/CaptureFormatting.h"
#include "config/VideoCaptureConstants.h"
#include "utils/FileNameUtils.h"

#include <QMetaObject>

CaptureController::CaptureController(QObject* parent) : QObject(parent), recorder(this)
{
    qRegisterMetaType<VideoFrame>("VideoFrame");
    qRegisterMetaType<FrameStats>("FrameStats");
    qRegisterMetaType<CaptureSettings>("CaptureSettings");
    qRegisterMetaType<QVector<X11WindowInfo>>("QVector<X11WindowInfo>");

    outputDirectory = FileNameUtils::defaultOutputDirectory();
    captureSettings.mode = CaptureMode::X11WindowById;

    windowRefreshWorker = new WindowRefreshWorker();
    windowRefreshWorker->moveToThread(&windowRefreshThread);
    connect(&windowRefreshThread, &QThread::finished, windowRefreshWorker, &QObject::deleteLater);
    connect(windowRefreshWorker, &WindowRefreshWorker::windowsReady, this, &CaptureController::handleWindowsReady,
            Qt::QueuedConnection);

    windowRefreshThread.start();

    captureWorker = new CapturePipelineWorker(&previewFrameProvider);
    captureWorker->moveToThread(&captureCommandThread);
    connect(&captureCommandThread, &QThread::started, captureWorker, &CapturePipelineWorker::initialize);
    connect(&captureCommandThread, &QThread::finished, captureWorker, &QObject::deleteLater);

    connect(
        captureWorker, &CapturePipelineWorker::captureStarted, this,
        [this]()
        {
            captureStartPending = false;
            captureStopPending = false;
            setIsCapturing(true);
            setStatusMessage(QStringLiteral("Capture started"));
            tryPendingAutoCrop();
            updatePreviewMessage();
        },
        Qt::QueuedConnection);

    connect(
        captureWorker, &CapturePipelineWorker::captureStopped, this,
        [this]()
        {
            captureStartPending = false;
            captureStopPending = false;

            if (recordingActive)
            {
                stopRecording();
            }

            setIsCapturing(false);
            autoCropPending = false;
            autoCropAttemptsRemaining = 0;
            previewFrameProvider.clear();
            setHasPreviewFrame(false);
            currentFps = 0.0;
            emit fpsChanged();
            setStatusMessage(QStringLiteral("Capture stopped"));
            updatePreviewMessage();
        },
        Qt::QueuedConnection);

    connect(
        captureWorker, &CapturePipelineWorker::fpsUpdated, this,
        [this](double value)
        {
            if (qFuzzyCompare(currentFps, value))
            {
                return;
            }

            currentFps = value;
            emit fpsChanged();
        },
        Qt::QueuedConnection);

    connect(
        captureWorker, &CapturePipelineWorker::warningOccurred, this,
        [this](const QString& message)
        {
            setWarningMessage(message);
            updatePreviewMessage();
        },
        Qt::QueuedConnection);

    connect(
        captureWorker, &CapturePipelineWorker::errorOccurred, this,
        [this](const QString& message)
        {
            setStatusMessage(message);
            setWarningMessage(message);
            updatePreviewMessage();
        },
        Qt::QueuedConnection);

    connect(
        captureWorker, &CapturePipelineWorker::blackFrameDetected, this,
        [this]()
        {
            setWarningMessage(CaptureFormatting::blackCaptureWarning());
            updatePreviewMessage();
        },
        Qt::QueuedConnection);

    connect(captureWorker, &CapturePipelineWorker::commandSucceeded, this,
            &CaptureController::handleCaptureCommandSucceeded, Qt::QueuedConnection);
    connect(captureWorker, &CapturePipelineWorker::commandFailed, this, &CaptureController::handleCaptureCommandFailed,
            Qt::QueuedConnection);

    captureCommandThread.start();

    connect(
        &previewFrameProvider, &VideoFrameProvider::hasFrameChanged, this,
        [this]()
        {
            setHasPreviewFrame(previewFrameProvider.hasFrame());
            tryPendingAutoCrop();
            updatePreviewMessage();
        },
        Qt::QueuedConnection);

    connect(
        &previewFrameProvider, &VideoFrameProvider::frameChanged, this,
        [this]()
        {
            tryPendingAutoCrop();
        },
        Qt::QueuedConnection);

    connect(&recorder, &VideoRecorder::recordingStarted, this,
            [this](const QString& filePath)
            {
                currentOutputFilePath = filePath;
                emit outputFilePathChanged();
                elapsedRecordingSeconds = 0;
                currentRecordingTimeText = CaptureFormatting::recordingTimeText(elapsedRecordingSeconds);
                emit recordingSecondsChanged();
                setIsRecording(true);
                recordingTimer.start();
                setStatusMessage(QStringLiteral("Recording started"));
            });
    connect(&recorder, &VideoRecorder::recordingStopped, this,
            [this](const QString& filePath)
            {
                // The recorder feed is active only while recording. Idle preview should
                // not pay a per-frame signal/slot cost for recorder checks.
                disconnectRecorderFrameFeed();
                recordingTimer.stop();
                setIsRecording(false);
                currentOutputFilePath = filePath;
                emit outputFilePathChanged();
                setStatusMessage(QStringLiteral("Recording saved: %1").arg(filePath));
            });
    connect(&recorder, &VideoRecorder::recordingFailed, this,
            [this](const QString& error)
            {
                disconnectRecorderFrameFeed();
                recordingTimer.stop();
                setIsRecording(false);
                setWarningMessage(error);
                setStatusMessage(error);
            });
    recordingTimer.setInterval(VideoCaptureConstants::millisecondsPerSecond);
    connect(&recordingTimer, &QTimer::timeout, this,
            [this]()
            {
                ++elapsedRecordingSeconds;
                currentRecordingTimeText = CaptureFormatting::recordingTimeText(elapsedRecordingSeconds);
                emit recordingSecondsChanged();
            });

    refreshWindows();
}

CaptureController::~CaptureController()
{
    disconnectRecorderFrameFeed();
    recorder.stopAsync();

    if (captureWorker && captureCommandThread.isRunning())
    {
        QMetaObject::invokeMethod(captureWorker, &CapturePipelineWorker::shutdown, Qt::BlockingQueuedConnection);

        captureCommandThread.quit();
        captureCommandThread.wait();
    }

    if (windowRefreshThread.isRunning())
    {
        windowRefreshThread.quit();
        windowRefreshThread.wait();
    }
}
