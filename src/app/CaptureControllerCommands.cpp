#include "CaptureController.h"

#include "app/CaptureFormatting.h"
#include "config/VideoCaptureConstants.h"
#include "utils/FileNameUtils.h"

#include <QMetaObject>

void CaptureController::refreshWindows()
{
    if (!windowRefreshWorker)
    {
        setWarningMessage(QStringLiteral("Window refresh worker is not available."));

        return;
    }

    const int requestId = ++latestWindowRefreshRequestId;

    setStatusMessage(QStringLiteral("Refreshing windows..."));

    QMetaObject::invokeMethod(windowRefreshWorker, "refresh", Qt::QueuedConnection, Q_ARG(int, requestId));
}

void CaptureController::selectWindow(qulonglong xid)
{
    if (captureStartPending || captureStopPending)
    {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));

        return;
    }

    const X11WindowInfo* info = windowListModelBuilder.findByXid(windowInfos, static_cast<quint64>(xid));

    if (!info)
    {
        setWarningMessage(QStringLiteral("Selected X11 window is not available."));

        return;
    }

    if (recordingActive)
    {
        setWarningMessage(QStringLiteral("Stop recording before changing source window."));

        return;
    }

    if (capturingActive)
    {
        stopCapture();
    }

    currentSelectedWindowId = info->xid;
    currentSelectedWindowTitle = info->title;
    currentSelectedSourceType = info->sourceType;
    captureSettings.mode = CaptureMode::X11WindowById;
    currentCaptureModeText = CaptureFormatting::captureModeText(captureSettings.mode);
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
    if (captureStartPending || captureStopPending)
    {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));

        return;
    }

    if (currentSelectedWindowId == 0)
    {
        setWarningMessage(QStringLiteral("No video source selected"));
        updatePreviewMessage();

        return;
    }

    if (capturingActive)
    {
        return;
    }

    QString cropWarning;
    const QRect crop = validatedCropRect(0, 0, currentSourceWidth, currentSourceHeight, &cropWarning);

    applyCropRect(crop);
    automaticCropRect = crop;

    if (!cropWarning.isEmpty())
    {
        setWarningMessage(cropWarning);
    }

    if (captureSettings.mode == CaptureMode::ScreenRegionFallback)
    {
        captureSettings.fallbackScreenRect = currentFallbackScreenRect();
    }

    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    autoCropPending = currentSelectedSourceType == QLatin1String("Browser");

    constexpr int browserAutoCropAttemptLimit = 90;

    autoCropAttemptsRemaining = autoCropPending ? browserAutoCropAttemptLimit : 0;
    currentFps = 0.0;
    emit fpsChanged();
    setStatusMessage(QStringLiteral("Starting capture..."));

    captureStartPending = true;

    QMetaObject::invokeMethod(captureWorker, "startCapture", Qt::QueuedConnection,
                              Q_ARG(CaptureSettings, captureSettings), Q_ARG(quint64, nextCaptureCommandId()));

    updatePreviewMessage();
}

void CaptureController::stopCapture()
{
    if (captureStopPending)
    {
        return;
    }

    if (recordingActive)
    {
        stopRecording();
    }

    if (!capturingActive && !captureStartPending)
    {
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

    QMetaObject::invokeMethod(captureWorker, "stopCapture", Qt::QueuedConnection,
                              Q_ARG(quint64, nextCaptureCommandId()));

    updatePreviewMessage();
}

void CaptureController::toggleRecording()
{
    if (recordingActive)
    {
        stopRecording();
    }
    else
    {
        startRecording();
    }
}

void CaptureController::startRecording()
{
    if (captureStartPending || captureStopPending)
    {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));

        return;
    }

    if (!capturingActive)
    {
        setWarningMessage(QStringLiteral("Start capture before recording."));

        return;
    }

    if (recordingActive)
    {
        return;
    }

    QString error;

    if (!FileNameUtils::ensureDirectory(outputDirectory, &error))
    {
        setWarningMessage(error);
        setStatusMessage(error);

        return;
    }

    const QString filePrefix =
        CaptureFormatting::recordingPrefixForSource(currentSelectedSourceType, currentSelectedWindowTitle);
    const QString filePath = FileNameUtils::uniqueRecordingFilePath(outputDirectory, filePrefix);

    RecordingSettings settings;

    settings.outputDirectory = outputDirectory;
    settings.filePath = filePath;
    settings.fps = captureSettings.fps;
    settings.width = captureSettings.cropRect.width();
    settings.height = captureSettings.cropRect.height();
    settings.bitrateKbps = VideoCaptureConstants::defaultRecordingBitrateKbps;

    if (!recorder.start(settings, &error))
    {
        disconnectRecorderFrameFeed();
        setWarningMessage(error);
        setStatusMessage(error);

        return;
    }

    connectRecorderFrameFeed();
}

void CaptureController::stopRecording()
{
    if (!recordingActive)
    {
        return;
    }

    setStatusMessage(QStringLiteral("Finalizing recording..."));
    disconnectRecorderFrameFeed();
    recorder.stopAsync();
}
