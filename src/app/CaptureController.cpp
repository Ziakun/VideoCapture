#include "CaptureController.h"

#include "utils/FileNameUtils.h"

#include <QDir>
#include <QImage>
#include <QMetaObject>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

#include <algorithm>
#include <cmath>

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

QString blackCaptureWarning()
{
    return QStringLiteral("Window capture may be black or stale");
}

int pixelDistance(QRgb left, QRgb right)
{
    return std::abs(qRed(left) - qRed(right))
        + std::abs(qGreen(left) - qGreen(right))
        + std::abs(qBlue(left) - qBlue(right));
}

double horizontalBoundaryScore(const QImage& image, int y, int xStart, int xEnd, int step)
{
    if (y <= 0 || y >= image.height()) {
        return 0.0;
    }

    const auto* previousRow = reinterpret_cast<const QRgb*>(image.constScanLine(y - 1));
    const auto* currentRow = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    double total = 0.0;
    int samples = 0;

    for (int x = xStart; x <= xEnd; x += step) {
        total += pixelDistance(previousRow[x], currentRow[x]);
        ++samples;
    }

    return samples > 0 ? total / samples : 0.0;
}

double verticalBoundaryScore(const QImage& image, int x, int yStart, int yEnd, int step)
{
    if (x <= 0 || x >= image.width()) {
        return 0.0;
    }

    double total = 0.0;
    int samples = 0;

    for (int y = yStart; y <= yEnd; y += step) {
        const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        total += pixelDistance(row[x - 1], row[x]);
        ++samples;
    }

    return samples > 0 ? total / samples : 0.0;
}

int strongestHorizontalBoundary(
    const QImage& image,
    int yStart,
    int yEnd,
    int xStart,
    int xEnd,
    int step,
    double* bestScore)
{
    int bestY = -1;
    double best = 0.0;

    for (int y = yStart; y <= yEnd; ++y) {
        const double score = horizontalBoundaryScore(image, y, xStart, xEnd, step);
        if (score > best) {
            best = score;
            bestY = y;
        }
    }

    if (bestScore) {
        *bestScore = best;
    }
    return bestY;
}

int strongestVerticalBoundary(
    const QImage& image,
    int xStart,
    int xEnd,
    int yStart,
    int yEnd,
    int step,
    double* bestScore)
{
    int bestX = -1;
    double best = 0.0;

    for (int x = xStart; x <= xEnd; ++x) {
        const double score = verticalBoundaryScore(image, x, yStart, yEnd, step);
        if (score > best) {
            best = score;
            bestX = x;
        }
    }

    if (bestScore) {
        *bestScore = best;
    }
    return bestX;
}

QRect detectLargeContentRect(const QImage& frame)
{
    if (frame.isNull() || frame.width() < 64 || frame.height() < 64) {
        return {};
    }

    const QImage image = frame.convertToFormat(QImage::Format_RGB32);
    const int width = image.width();
    const int height = image.height();
    const int step = std::clamp(std::min(width, height) / 520, 1, 5);
    const int horizontalXStart = std::clamp(width / 10, 0, width - 1);
    const int horizontalXEnd = std::clamp(width - width / 10, horizontalXStart, width - 1);

    double topScore = 0.0;
    const int top = strongestHorizontalBoundary(
        image,
        std::max(24, height / 10),
        std::max(25, height * 55 / 100),
        horizontalXStart,
        horizontalXEnd,
        step,
        &topScore);

    if (top < 0 || topScore < 12.0) {
        return {};
    }

    double bottomScore = 0.0;
    const int bottom = strongestHorizontalBoundary(
        image,
        std::min(height - 24, std::max(top + 64, height * 55 / 100)),
        height - 24,
        horizontalXStart,
        horizontalXEnd,
        step,
        &bottomScore);

    if (bottom <= top + 64 || bottomScore < 12.0) {
        return {};
    }

    double leftScore = 0.0;
    const int left = strongestVerticalBoundary(
        image,
        4,
        std::max(4, width / 3),
        top,
        bottom,
        step,
        &leftScore);

    double rightScore = 0.0;
    const int right = strongestVerticalBoundary(
        image,
        std::min(width - 5, width * 2 / 3),
        width - 5,
        top,
        bottom,
        step,
        &rightScore);

    if (left < 0 || right <= left + 64 || leftScore < 6.0 || rightScore < 6.0) {
        return {};
    }

    const int margin = std::max(8, std::min(width, height) / 90);
    const QRect detected(
        QPoint(std::max(0, left - margin), std::max(0, top - margin)),
        QPoint(std::min(width - 1, right + margin), std::min(height - 1, bottom + margin)));

    if (detected.width() < 64 || detected.height() < 64) {
        return {};
    }
    if (static_cast<qint64>(detected.width()) * detected.height() < static_cast<qint64>(width) * height / 8) {
        return {};
    }

    return detected;
}

QString displayTitle(const X11WindowInfo& info)
{
    return QStringLiteral("%1 | %2 | %3 | %4 | %5 | pid %6")
        .arg(info.title)
        .arg(info.sourceType)
        .arg(info.className.isEmpty() ? QStringLiteral("-") : info.className)
        .arg(xidString(info.xid))
        .arg(geometryString(info.geometry))
        .arg(info.pid >= 0 ? QString::number(info.pid) : QStringLiteral("-"));
}

QString recordingPrefixForSource(const QString& sourceType, const QString& title)
{
    if (sourceType == QLatin1String("Zoom")) {
        return QStringLiteral("zoom-recording");
    }

    const QString titleLower = title.toLower();
    if (titleLower.contains(QStringLiteral("google meet")) || titleLower.contains(QStringLiteral("meet.google"))) {
        return QStringLiteral("meet-recording");
    }
    if (sourceType == QLatin1String("Browser")) {
        return QStringLiteral("browser-recording");
    }

    return QStringLiteral("window-recording");
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
        tryPendingAutoCrop();
        updatePreviewMessage();
    }, Qt::QueuedConnection);
    connect(captureWorker, &CapturePipelineWorker::captureStopped, this, [this]() {
        captureStartPending = false;
        captureStopPending = false;
        if (recordingActive || recorder.isRecording()) {
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
        setWarningMessage(blackCaptureWarning());
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
        tryPendingAutoCrop();
        updatePreviewMessage();
    }, Qt::QueuedConnection);
    connect(&previewFrameProvider, &VideoFrameProvider::frameChanged, this, [this]() {
        tryPendingAutoCrop();
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

QString CaptureController::selectedSourceType() const
{
    return currentSelectedSourceType;
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
    currentSelectedSourceType = info->sourceType;
    captureSettings.mode = CaptureMode::X11WindowById;
    updateSourceFromSelectedWindow(*info);
    applyCropRect(QRect(0, 0, info->geometry.width(), info->geometry.height()));
    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    setWarningMessage(QString());
    setStatusMessage(QStringLiteral("Selected %1 source: %2").arg(info->sourceType, info->title));
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
    const QRect crop = validatedCropRect(0, 0, sourceWidth(), sourceHeight(), &cropWarning);
    applyCropRect(crop);
    automaticCropRect = crop;
    if (!cropWarning.isEmpty()) {
        setWarningMessage(cropWarning);
    }

    if (captureSettings.mode == CaptureMode::ScreenRegionFallback) {
        captureSettings.fallbackScreenRect = currentFallbackScreenRect();
    }

    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    autoCropPending = currentSelectedSourceType == QLatin1String("Browser");
    autoCropAttemptsRemaining = autoCropPending ? 90 : 0;
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
    autoCropPending = false;
    autoCropAttemptsRemaining = 0;
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

    const QString filePrefix = recordingPrefixForSource(currentSelectedSourceType, currentSelectedWindowTitle);
    const QString filePath = FileNameUtils::uniqueRecordingFilePath(outputDirectory, filePrefix);

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

    const int right = std::max(0, captureSettings.sourceGeometry.width() - rect.x() - rect.width());
    const int bottom = std::max(0, captureSettings.sourceGeometry.height() - rect.y() - rect.height());
    setStatusMessage(
        QStringLiteral("Crop: left %1 right %2 top %3 bottom %4")
            .arg(rect.x())
            .arg(right)
            .arg(rect.y())
            .arg(bottom));
    updatePreviewMessage();
}

void CaptureController::resetCropToAutoState()
{
    QRect target = automaticCropRect;
    if (!target.isValid() || target.width() <= 0 || target.height() <= 0) {
        target = QRect(0, 0, sourceWidth(), sourceHeight());
    }

    setCropRect(target.x(), target.y(), target.width(), target.height());
}

void CaptureController::tryPendingAutoCrop()
{
    if (!autoCropPending) {
        return;
    }
    if (currentSelectedSourceType != QLatin1String("Browser")) {
        autoCropPending = false;
        autoCropAttemptsRemaining = 0;
        return;
    }
    if (autoCropAttemptsRemaining <= 0) {
        autoCropPending = false;
        return;
    }
    if (captureStartPending || captureStopPending || !capturingActive || !previewFrameProvider.hasFrame()) {
        return;
    }

    --autoCropAttemptsRemaining;
    if (autoCropToVideoArea() || autoCropAttemptsRemaining <= 0) {
        autoCropPending = false;
    }
}

bool CaptureController::autoCropToVideoArea()
{
    if (captureStartPending || captureStopPending) {
        return false;
    }
    if (currentSelectedWindowId == 0) {
        return false;
    }
    if (!capturingActive || !previewFrameProvider.hasFrame()) {
        return false;
    }

    const QImage frame = previewFrameProvider.currentFrame();
    const QRect detected = detectLargeContentRect(frame);
    if (!detected.isValid()) {
        return false;
    }

    QRect baseCrop = captureSettings.cropRect;
    if (!baseCrop.isValid() || baseCrop.width() <= 0 || baseCrop.height() <= 0) {
        baseCrop = QRect(0, 0, captureSettings.sourceGeometry.width(), captureSettings.sourceGeometry.height());
    }

    const double scaleX = frame.width() > 0 ? static_cast<double>(baseCrop.width()) / frame.width() : 1.0;
    const double scaleY = frame.height() > 0 ? static_cast<double>(baseCrop.height()) / frame.height() : 1.0;
    const QRect target(
        baseCrop.x() + static_cast<int>(std::round(detected.x() * scaleX)),
        baseCrop.y() + static_cast<int>(std::round(detected.y() * scaleY)),
        static_cast<int>(std::round(detected.width() * scaleX)),
        static_cast<int>(std::round(detected.height() * scaleY)));

    QString warning;
    automaticCropRect = validatedCropRect(target.x(), target.y(), target.width(), target.height(), &warning);
    setStatusMessage(QStringLiteral("Auto crop: %1,%2 %3x%4").arg(target.x()).arg(target.y()).arg(target.width()).arg(target.height()));
    setCropRect(automaticCropRect.x(), automaticCropRect.y(), automaticCropRect.width(), automaticCropRect.height());
    return true;
}

void CaptureController::dismissCurrentMessage()
{
    setWarningMessage(QString());
    setStatusMessage(QString());
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
        "Fallback captures visible screen pixels. If another window covers the selected video region, it will be captured too.");
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
        item.insert(QStringLiteral("sourceType"), info.sourceType);
        item.insert(QStringLiteral("sourceHint"), info.sourceHint);
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
    } else if (currentWarningMessage == blackCaptureWarning()) {
        setPreviewMessage(blackCaptureWarning());
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
            currentSelectedSourceType = QStringLiteral("Window");
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
    const bool selectedMetadataChanged = currentSelectedWindowTitle != info.title
        || currentSelectedSourceType != info.sourceType;
    const bool selectedSourceChanged = captureSettings.windowId != info.xid
        || captureSettings.sourceGeometry != info.geometry
        || captureSettings.sourceCaptureOffset != info.captureOffset;

    currentSelectedWindowTitle = info.title;
    currentSelectedSourceType = info.sourceType;
    captureSettings.windowId = info.xid;
    captureSettings.windowTitle = info.title;
    captureSettings.sourceGeometry = info.geometry;
    captureSettings.sourceCaptureOffset = info.captureOffset;
    if (selectedSourceChanged) {
        automaticCropRect = QRect();
    }
    if (selectedMetadataChanged) {
        emit selectedWindowChanged();
    }
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

    // Keep the crop valid for videocrop and H.264/I420 encoding. Tiny or
    // out-of-bounds rectangles are clamped; odd sizes are rounded down because
    // x264 cannot reliably encode 4:2:0 frames with odd dimensions.
    constexpr int minimumSize = 64;
    QRect requested(x, y, width, height);
    QRect bounded = requested;

    bounded.setX(std::max(0, bounded.x()));
    bounded.setY(std::max(0, bounded.y()));
    bounded.setWidth(std::max(minimumSize, bounded.width()));
    bounded.setHeight(std::max(minimumSize, bounded.height()));

    if (bounded.width() > source.width()) {
        bounded.setWidth(source.width());
    }
    if (bounded.height() > source.height()) {
        bounded.setHeight(source.height());
    }
    if (bounded.x() + bounded.width() > source.width()) {
        bounded.moveLeft(std::max(0, source.width() - bounded.width()));
    }
    if (bounded.y() + bounded.height() > source.height()) {
        bounded.moveTop(std::max(0, source.height() - bounded.height()));
    }

    const int minimumEvenWidth = source.width() >= minimumSize
        ? minimumSize
        : std::max(2, source.width() - source.width() % 2);
    const int minimumEvenHeight = source.height() >= minimumSize
        ? minimumSize
        : std::max(2, source.height() - source.height() % 2);

    QRect clamped = bounded;
    if (clamped.width() % 2 != 0) {
        clamped.setWidth(std::max(minimumEvenWidth, clamped.width() - 1));
    }
    if (clamped.height() % 2 != 0) {
        clamped.setHeight(std::max(minimumEvenHeight, clamped.height() - 1));
    }

    if (warningMessage && bounded != requested) {
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
