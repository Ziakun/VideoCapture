#include "CaptureController.h"

#include "utils/FileNameUtils.h"

#include <QDir>
#include <QMetaObject>
#include <QUrl>
#include <QVariantMap>

namespace {

QString xidString(quint64 xid)
{
    return QStringLiteral("0x%1").arg(xid, 0, 16);
}

QString geometryString(const QRect& rect)
{
    if (!rect.isValid()) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

QString captureModeToString(CaptureMode mode)
{
    return mode == CaptureMode::X11WindowById
        ? QStringLiteral("X11WindowById")
        : QStringLiteral("ScreenRegionFallback");
}

QString displayTitle(const X11WindowInfo& info)
{
    return QStringLiteral("%1 | %2 | %3 | %4 | pid %5")
        .arg(info.title)
        .arg(info.className.isEmpty() ? QStringLiteral("-") : info.className)
        .arg(xidString(info.xid))
        .arg(geometryString(info.geometry))
        .arg(info.pid >= 0 ? QString::number(info.pid) : QStringLiteral("-"));
}

} // namespace

CaptureController::CaptureController(QObject* parent)
    : QObject(parent)
    , recorder(this)
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
    connect(
        windowRefreshWorker,
        &WindowRefreshWorker::windowsReady,
        this,
        &CaptureController::handleWindowsReady,
        Qt::QueuedConnection);
    windowRefreshThread.start();

    captureWorker = new CapturePipelineWorker(&previewFrameProvider);
    captureWorker->moveToThread(&captureCommandThread);
    connect(&captureCommandThread, &QThread::started, captureWorker, &CapturePipelineWorker::initialize);
    connect(&captureCommandThread, &QThread::finished, captureWorker, &QObject::deleteLater);

    connect(captureWorker, &CapturePipelineWorker::captureStarted, this, [this]() {
        captureStartPending = false;
        captureStopPending = false;
        setIsCapturing(true);
        setStatusMessage(QStringLiteral("Capture started"));
        updatePreviewMessage();
    }, Qt::QueuedConnection);
    connect(captureWorker, &CapturePipelineWorker::captureStopped, this, [this]() {
        captureStartPending = false;
        captureStopPending = false;
        if (recordingActive || recorder.isRecording()) {
            stopRecording();
        }
        setIsCapturing(false);
        previewFrameProvider.clear();
        setHasPreviewFrame(false);
        currentFps = 0.0;
        emit fpsChanged();
        setStatusMessage(QStringLiteral("Capture stopped"));
        updatePreviewMessage();
    }, Qt::QueuedConnection);
    connect(captureWorker, &CapturePipelineWorker::fpsUpdated, this, [this](double value) {
        if (qFuzzyCompare(currentFps, value)) {
            return;
        }
        currentFps = value;
        emit fpsChanged();
    }, Qt::QueuedConnection);
    connect(captureWorker, &CapturePipelineWorker::warningOccurred, this, [this](const QString& message) {
        setWarningMessage(message);
        updatePreviewMessage();
    }, Qt::QueuedConnection);
    connect(captureWorker, &CapturePipelineWorker::errorOccurred, this, [this](const QString& message) {
        setStatusMessage(message);
        setWarningMessage(message);
        updatePreviewMessage();
    }, Qt::QueuedConnection);
    connect(captureWorker, &CapturePipelineWorker::blackFrameDetected, this, [this]() {
        setWarningMessage(QStringLiteral("Window capture may be black or stale"));
        updatePreviewMessage();
    }, Qt::QueuedConnection);
    connect(captureWorker, &CapturePipelineWorker::staleFramesDetected, this, [this]() {
        setWarningMessage(QStringLiteral("Window capture may be black or stale"));
        updatePreviewMessage();
    }, Qt::QueuedConnection);
    connect(
        captureWorker,
        &CapturePipelineWorker::commandSucceeded,
        this,
        &CaptureController::handleCaptureCommandSucceeded,
        Qt::QueuedConnection);
    connect(
        captureWorker,
        &CapturePipelineWorker::commandFailed,
        this,
        &CaptureController::handleCaptureCommandFailed,
        Qt::QueuedConnection);
    captureCommandThread.start();

    connect(&previewFrameProvider, &VideoFrameProvider::hasFrameChanged, this, [this]() {
        setHasPreviewFrame(previewFrameProvider.hasFrame());
        updatePreviewMessage();
    }, Qt::QueuedConnection);

    connect(&recorder, &VideoRecorder::recordingStarted, this, [this](const QString& filePath) {
        currentOutputFilePath = filePath;
        emit outputFilePathChanged();
        elapsedRecordingSeconds = 0;
        emit recordingSecondsChanged();
        setIsRecording(true);
        recordingTimer.start();
        setStatusMessage(QStringLiteral("Recording started"));
    });
    connect(&recorder, &VideoRecorder::recordingStopped, this, [this](const QString& filePath) {
        // The recorder feed is active only while recording. Idle preview should
        // not pay a per-frame signal/slot cost for recorder checks.
        disconnectRecorderFrameFeed();
        recordingTimer.stop();
        setIsRecording(false);
        currentOutputFilePath = filePath;
        emit outputFilePathChanged();
        setStatusMessage(QStringLiteral("Recording saved: %1").arg(filePath));
    });
    connect(&recorder, &VideoRecorder::recordingFailed, this, [this](const QString& error) {
        disconnectRecorderFrameFeed();
        recordingTimer.stop();
        setIsRecording(false);
        setWarningMessage(error);
        setStatusMessage(error);
    });
    connect(&recorder, &VideoRecorder::droppedFrameCountChanged, this, [this](quint64 droppedFrames) {
        setWarningMessage(QStringLiteral("Recording is dropping frames: %1").arg(droppedFrames));
    });

    recordingTimer.setInterval(1000);
    connect(&recordingTimer, &QTimer::timeout, this, [this]() {
        ++elapsedRecordingSeconds;
        emit recordingSecondsChanged();
    });

    refreshWindows();
}

CaptureController::~CaptureController()
{
    disconnectRecorderFrameFeed();

    if (recorder.isRecording() || recorder.isStopping()) {
        recorder.stopAsync();
    }

    if (captureWorker && captureCommandThread.isRunning()) {
        QMetaObject::invokeMethod(captureWorker, &CapturePipelineWorker::shutdown, Qt::BlockingQueuedConnection);
        captureCommandThread.quit();
        captureCommandThread.wait();
    }

    if (windowRefreshThread.isRunning()) {
        windowRefreshThread.quit();
        windowRefreshThread.wait();
    }
}

QObject* CaptureController::frameProvider()
{
    return &previewFrameProvider;
}

QVariantList CaptureController::windows() const
{
    return windowModel;
}

bool CaptureController::isCapturing() const
{
    return capturingActive;
}

bool CaptureController::isRecording() const
{
    return recordingActive;
}

bool CaptureController::hasPreviewFrame() const
{
    return previewFrameAvailable;
}

QString CaptureController::selectedWindowTitle() const
{
    return currentSelectedWindowTitle;
}

qulonglong CaptureController::selectedWindowId() const
{
    return static_cast<qulonglong>(currentSelectedWindowId);
}

QString CaptureController::outputFilePath() const
{
    return currentOutputFilePath;
}

int CaptureController::recordingSeconds() const
{
    return elapsedRecordingSeconds;
}

QString CaptureController::recordingTimeText() const
{
    return formatRecordingTime(elapsedRecordingSeconds);
}

double CaptureController::fps() const
{
    return currentFps;
}

QString CaptureController::statusMessage() const
{
    return currentStatusMessage;
}

QString CaptureController::warningMessage() const
{
    return currentWarningMessage;
}

QString CaptureController::previewMessage() const
{
    return currentPreviewMessage;
}

int CaptureController::cropX() const
{
    return captureSettings.cropRect.x();
}

int CaptureController::cropY() const
{
    return captureSettings.cropRect.y();
}

int CaptureController::cropWidth() const
{
    return captureSettings.cropRect.width();
}

int CaptureController::cropHeight() const
{
    return captureSettings.cropRect.height();
}

QString CaptureController::captureModeText() const
{
    return captureModeToString(captureSettings.mode);
}

QString CaptureController::sourceGeometryText() const
{
    return geometryString(captureSettings.sourceGeometry);
}

int CaptureController::sourceWidth() const
{
    return captureSettings.sourceGeometry.width();
}

int CaptureController::sourceHeight() const
{
    return captureSettings.sourceGeometry.height();
}

void CaptureController::refreshWindows()
{
    if (!windowRefreshWorker) {
        setWarningMessage(QStringLiteral("Window refresh worker is not available."));
        return;
    }

    const int requestId = ++latestWindowRefreshRequestId;
    setStatusMessage(QStringLiteral("Refreshing windows..."));
    QMetaObject::invokeMethod(
        windowRefreshWorker,
        "refresh",
        Qt::QueuedConnection,
        Q_ARG(int, requestId));
}

void CaptureController::selectWindow(qulonglong xid)
{
    if (captureStartPending || captureStopPending) {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));
        return;
    }

    const X11WindowInfo* info = findWindow(static_cast<quint64>(xid));
    if (!info) {
        setWarningMessage(QStringLiteral("Selected X11 window is not available."));
        return;
    }

    if (recordingActive) {
        setWarningMessage(QStringLiteral("Stop recording before changing source window."));
        return;
    }

    if (capturingActive) {
        stopCapture();
    }

    currentSelectedWindowId = info->xid;
    currentSelectedWindowTitle = info->title;
    captureSettings.mode = CaptureMode::X11WindowById;
    updateSourceFromSelectedWindow(*info);
    applyCropRect(QRect(0, 0, info->geometry.width(), info->geometry.height()));
    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    setWarningMessage(QString());
    setStatusMessage(QStringLiteral("Selected window: %1").arg(info->title));
    emit selectedWindowChanged();
    emit captureModeChanged();
    updatePreviewMessage();
}

void CaptureController::startCapture()
{
    if (captureStartPending || captureStopPending) {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));
        return;
    }

    if (currentSelectedWindowId == 0) {
        setWarningMessage(QStringLiteral("No video source selected"));
        updatePreviewMessage();
        return;
    }

    if (capturingActive) {
        return;
    }

    QString cropWarning;
    const QRect crop = validatedCropRect(cropX(), cropY(), cropWidth(), cropHeight(), &cropWarning);
    applyCropRect(crop);
    if (!cropWarning.isEmpty()) {
        setWarningMessage(cropWarning);
    }

    if (captureSettings.mode == CaptureMode::ScreenRegionFallback) {
        captureSettings.fallbackScreenRect = currentFallbackScreenRect();
    }

    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    currentFps = 0.0;
    emit fpsChanged();
    setStatusMessage(QStringLiteral("Starting capture..."));

    captureStartPending = true;
    QMetaObject::invokeMethod(
        captureWorker,
        "startCapture",
        Qt::QueuedConnection,
        Q_ARG(CaptureSettings, captureSettings),
        Q_ARG(quint64, nextCaptureCommandId()));

    updatePreviewMessage();
}

void CaptureController::stopCapture()
{
    if (captureStopPending) {
        return;
    }

    if (recordingActive) {
        stopRecording();
    }

    if (!capturingActive && !captureStartPending) {
        return;
    }

    captureStopPending = true;
    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    currentFps = 0.0;
    emit fpsChanged();
    setStatusMessage(QStringLiteral("Stopping capture..."));
    QMetaObject::invokeMethod(
        captureWorker,
        "stopCapture",
        Qt::QueuedConnection,
        Q_ARG(quint64, nextCaptureCommandId()));
    updatePreviewMessage();
}

void CaptureController::toggleRecording()
{
    if (recordingActive) {
        stopRecording();
    } else {
        startRecording();
    }
}

void CaptureController::startRecording()
{
    if (captureStartPending || captureStopPending) {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));
        return;
    }

    if (!capturingActive) {
        setWarningMessage(QStringLiteral("Start capture before recording."));
        return;
    }
    if (recordingActive || recorder.isStopping()) {
        return;
    }

    QString error;
    if (!FileNameUtils::ensureDirectory(outputDirectory, &error)) {
        setWarningMessage(error);
        setStatusMessage(error);
        return;
    }

    const QString filePath = FileNameUtils::uniqueRecordingFilePath(outputDirectory);

    RecordingSettings settings;
    settings.outputDirectory = outputDirectory;
    settings.filePath = filePath;
    settings.fps = captureSettings.fps;
    settings.width = captureSettings.cropRect.width();
    settings.height = captureSettings.cropRect.height();
    settings.bitrateKbps = 2500;

    if (!recorder.start(settings, &error)) {
        disconnectRecorderFrameFeed();
        setWarningMessage(error);
        setStatusMessage(error);
        return;
    }

    connectRecorderFrameFeed();
}

void CaptureController::stopRecording()
{
    if (!recordingActive && !recorder.isRecording()) {
        return;
    }

    setStatusMessage(QStringLiteral("Finalizing recording..."));
    disconnectRecorderFrameFeed();
    recorder.stopAsync();
}

void CaptureController::setCropRect(int x, int y, int width, int height)
{
    if (captureStartPending || captureStopPending) {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));
        return;
    }

    QString warning;
    const QRect rect = validatedCropRect(x, y, width, height, &warning);

    if (recordingActive && rect.size() != captureSettings.cropRect.size()) {
        setWarningMessage(QStringLiteral("Stop recording before changing crop size."));
        return;
    }
    if (recordingActive && captureSettings.mode == CaptureMode::ScreenRegionFallback && rect != captureSettings.cropRect) {
        setWarningMessage(QStringLiteral("Stop recording before changing fallback crop."));
        return;
    }

    applyCropRect(rect);

    if (!warning.isEmpty()) {
        setWarningMessage(warning);
    } else if (currentWarningMessage.startsWith(QStringLiteral("Crop rectangle"))) {
        setWarningMessage(QString());
    }

    if (capturingActive) {
        if (captureSettings.mode == CaptureMode::X11WindowById) {
            QMetaObject::invokeMethod(
                captureWorker,
                "updateCropRect",
                Qt::QueuedConnection,
                Q_ARG(QRect, rect),
                Q_ARG(CaptureSettings, captureSettings),
                Q_ARG(quint64, nextCaptureCommandId()));
        } else {
            captureSettings.fallbackScreenRect = currentFallbackScreenRect();
            restartCaptureForCurrentSettings();
        }
    }

    setStatusMessage(QStringLiteral("Crop: %1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
    updatePreviewMessage();
}

void CaptureController::chooseOutputDirectory(const QString& path)
{
    QString localPath = path;
    const QUrl url(path);
    if (url.isValid() && url.isLocalFile()) {
        localPath = url.toLocalFile();
    }

    if (localPath.isEmpty()) {
        return;
    }

    outputDirectory = QDir::cleanPath(localPath);
    setStatusMessage(QStringLiteral("Output directory: %1").arg(outputDirectory));
}

void CaptureController::switchToScreenRegionFallback()
{
    if (captureStartPending || captureStopPending) {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));
        return;
    }

    if (currentSelectedWindowId == 0) {
        setWarningMessage(QStringLiteral("No video source selected"));
        updatePreviewMessage();
        return;
    }
    if (recordingActive) {
        setWarningMessage(QStringLiteral("Stop recording before switching capture mode."));
        return;
    }

    captureSettings.mode = CaptureMode::ScreenRegionFallback;
    captureSettings.fallbackScreenRect = currentFallbackScreenRect();
    emit captureModeChanged();

    const QString fallbackWarning = QStringLiteral(
        "Fallback captures visible screen pixels. If another window covers Meet, it will be captured too.");
    setWarningMessage(fallbackWarning);

    if (capturingActive) {
        restartCaptureForCurrentSettings();
    } else {
        setStatusMessage(QStringLiteral("Screen region fallback selected"));
    }

    updatePreviewMessage();
}

const X11WindowInfo* CaptureController::findWindow(quint64 xid) const
{
    for (const X11WindowInfo& info : windowInfos) {
        if (info.xid == xid) {
            return &info;
        }
    }
    return nullptr;
}

void CaptureController::rebuildWindowVariantList()
{
    windowModel.clear();
    windowModel.reserve(windowInfos.size());

    for (const X11WindowInfo& info : windowInfos) {
        QVariantMap item;
        item.insert(QStringLiteral("display"), displayTitle(info));
        item.insert(QStringLiteral("title"), info.title);
        item.insert(QStringLiteral("className"), info.className);
        item.insert(QStringLiteral("xid"), QVariant::fromValue<qulonglong>(static_cast<qulonglong>(info.xid)));
        item.insert(QStringLiteral("xidText"), xidString(info.xid));
        item.insert(QStringLiteral("geometry"), geometryString(info.geometry));
        item.insert(QStringLiteral("pid"), info.pid >= 0 ? QString::number(info.pid) : QStringLiteral("-"));
        item.insert(QStringLiteral("isVisible"), info.isVisible);
        windowModel.push_back(item);
    }
}

void CaptureController::setIsCapturing(bool value)
{
    if (capturingActive == value) {
        return;
    }
    capturingActive = value;
    emit isCapturingChanged();
}

void CaptureController::setIsRecording(bool value)
{
    if (recordingActive == value) {
        return;
    }
    recordingActive = value;
    emit isRecordingChanged();
}

void CaptureController::setHasPreviewFrame(bool value)
{
    if (previewFrameAvailable == value) {
        return;
    }
    previewFrameAvailable = value;
    emit hasPreviewFrameChanged();
}

void CaptureController::setStatusMessage(const QString& message)
{
    if (currentStatusMessage == message) {
        return;
    }
    currentStatusMessage = message;
    emit statusMessageChanged();
}

void CaptureController::setWarningMessage(const QString& message)
{
    if (currentWarningMessage == message) {
        return;
    }
    currentWarningMessage = message;
    emit warningMessageChanged();
}

void CaptureController::setPreviewMessage(const QString& message)
{
    if (currentPreviewMessage == message) {
        return;
    }
    currentPreviewMessage = message;
    emit previewMessageChanged();
}

void CaptureController::updatePreviewMessage()
{
    if (currentSelectedWindowId == 0) {
        setPreviewMessage(QStringLiteral("No video source selected"));
    } else if (!capturingActive) {
        setPreviewMessage(QStringLiteral("Capture is not running"));
    } else if (!previewFrameAvailable) {
        setPreviewMessage(QStringLiteral("Waiting for frames..."));
    } else if (currentWarningMessage == QStringLiteral("Window capture may be black or stale")) {
        setPreviewMessage(QStringLiteral("Window capture may be black or stale"));
    } else {
        setPreviewMessage(QString());
    }
}

void CaptureController::handleWindowsReady(int requestId, const QVector<X11WindowInfo>& windows, const QString& error)
{
    if (requestId != latestWindowRefreshRequestId) {
        return;
    }

    windowInfos = windows;
    rebuildWindowVariantList();
    emit windowsChanged();

    if (!error.isEmpty()) {
        setWarningMessage(error);
        setStatusMessage(error);
        updatePreviewMessage();
        return;
    }

    if (currentSelectedWindowId != 0) {
        if (const X11WindowInfo* info = findWindow(currentSelectedWindowId)) {
            updateSourceFromSelectedWindow(*info);
        } else {
            currentSelectedWindowId = 0;
            currentSelectedWindowTitle.clear();
            setStatusMessage(QStringLiteral("Previously selected window is no longer available"));
            emit selectedWindowChanged();
            updatePreviewMessage();
        }
    } else if (windowInfos.isEmpty()) {
        setStatusMessage(QStringLiteral("No top-level X11 windows found"));
    } else {
        setStatusMessage(QStringLiteral("Select a source window"));
    }
}

void CaptureController::handleCaptureCommandFailed(quint64 commandId, const QString& error)
{
    Q_UNUSED(commandId)

    captureStartPending = false;
    captureStopPending = false;
    if (recordingActive || recorder.isRecording()) {
        stopRecording();
    }
    setIsCapturing(false);
    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    currentFps = 0.0;
    emit fpsChanged();
    setStatusMessage(error);
    setWarningMessage(error);
    updatePreviewMessage();
}

void CaptureController::handleCaptureCommandSucceeded(quint64 commandId)
{
    Q_UNUSED(commandId)

    if (captureStopPending && !capturingActive) {
        captureStopPending = false;
    }
}

void CaptureController::updateSourceFromSelectedWindow(const X11WindowInfo& info)
{
    captureSettings.windowId = info.xid;
    captureSettings.windowTitle = info.title;
    captureSettings.sourceGeometry = info.geometry;
    emit sourceGeometryChanged();
}

QRect CaptureController::validatedCropRect(int x, int y, int width, int height, QString* warningMessage) const
{
    QRect source = captureSettings.sourceGeometry;
    if (!source.isValid() || source.width() <= 0 || source.height() <= 0) {
        if (warningMessage) {
            *warningMessage = QStringLiteral("No source geometry available.");
        }
        return QRect(0, 0, 0, 0);
    }

    // Keep the crop valid for videocrop and encoders. Tiny or out-of-bounds
    // rectangles are clamped instead of failing the capture session.
    constexpr int minimumSize = 64;
    QRect requested(x, y, width, height);
    QRect clamped = requested;

    clamped.setX(std::max(0, clamped.x()));
    clamped.setY(std::max(0, clamped.y()));
    clamped.setWidth(std::max(minimumSize, clamped.width()));
    clamped.setHeight(std::max(minimumSize, clamped.height()));

    if (clamped.width() > source.width()) {
        clamped.setWidth(source.width());
    }
    if (clamped.height() > source.height()) {
        clamped.setHeight(source.height());
    }
    if (clamped.x() + clamped.width() > source.width()) {
        clamped.moveLeft(std::max(0, source.width() - clamped.width()));
    }
    if (clamped.y() + clamped.height() > source.height()) {
        clamped.moveTop(std::max(0, source.height() - clamped.height()));
    }

    if (warningMessage && clamped != requested) {
        *warningMessage = QStringLiteral("Crop rectangle was clamped to fit inside the source window.");
    }

    return clamped;
}

void CaptureController::applyCropRect(const QRect& rect)
{
    if (captureSettings.cropRect == rect) {
        return;
    }

    captureSettings.cropRect = rect;
    emit cropChanged();
}

QRect CaptureController::currentFallbackScreenRect() const
{
    // Fallback mode uses absolute screen coordinates because ximagesrc captures
    // the visible desktop region, not pixels relative to a window.
    const QPoint topLeft = captureSettings.sourceGeometry.topLeft() + captureSettings.cropRect.topLeft();
    return QRect(topLeft, captureSettings.cropRect.size());
}

void CaptureController::restartCaptureForCurrentSettings()
{
    if (!capturingActive) {
        return;
    }

    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    currentFps = 0.0;
    emit fpsChanged();
    setStatusMessage(QStringLiteral("Restarting capture..."));

    QMetaObject::invokeMethod(
        captureWorker,
        "restartCapture",
        Qt::QueuedConnection,
        Q_ARG(CaptureSettings, captureSettings),
        Q_ARG(quint64, nextCaptureCommandId()));
}

quint64 CaptureController::nextCaptureCommandId()
{
    return ++nextCaptureCommandSequence;
}

void CaptureController::connectRecorderFrameFeed()
{
    // Use a direct connection while recording so recorder enqueue happens from
    // the capture callback without an extra queued UI-thread hop.
    if (recorderFrameConnection) {
        return;
    }

    recorderFrameConnection = connect(
        captureWorker,
        &CapturePipelineWorker::frameReady,
        &recorder,
        &VideoRecorder::enqueueFrame,
        Qt::DirectConnection);
}

void CaptureController::disconnectRecorderFrameFeed()
{
    if (!recorderFrameConnection) {
        return;
    }

    disconnect(recorderFrameConnection);
    recorderFrameConnection = {};
}

QString CaptureController::formatRecordingTime(int seconds)
{
    const int hours = seconds / 3600;
    const int minutes = (seconds / 60) % 60;
    const int remainingSeconds = seconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainingSeconds, 2, 10, QLatin1Char('0'));
}
