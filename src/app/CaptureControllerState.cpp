#include "CaptureController.h"

#include "app/CaptureFormatting.h"

#include <QDir>
#include <QUrl>

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

    if (url.isValid() && url.isLocalFile())
    {
        localPath = url.toLocalFile();
    }

    if (localPath.isEmpty())
    {
        return;
    }

    outputDirectory = QDir::cleanPath(localPath);
    setStatusMessage(QStringLiteral("Output directory: %1").arg(outputDirectory));
}

void CaptureController::setIsCapturing(bool value)
{
    if (capturingActive == value)
    {
        return;
    }

    capturingActive = value;
    emit isCapturingChanged();
}

void CaptureController::setIsRecording(bool value)
{
    if (recordingActive == value)
    {
        return;
    }

    recordingActive = value;
    emit isRecordingChanged();
}

void CaptureController::setHasPreviewFrame(bool value)
{
    if (previewFrameAvailable == value)
    {
        return;
    }

    previewFrameAvailable = value;
    emit hasPreviewFrameChanged();
}

void CaptureController::setStatusMessage(const QString& message)
{
    if (currentStatusMessage == message)
    {
        return;
    }

    currentStatusMessage = message;
    emit statusMessageChanged();
}

void CaptureController::setWarningMessage(const QString& message)
{
    if (currentWarningMessage == message)
    {
        return;
    }

    currentWarningMessage = message;
    emit warningMessageChanged();
}

void CaptureController::setPreviewMessage(const QString& message)
{
    if (currentPreviewMessage == message)
    {
        return;
    }

    currentPreviewMessage = message;
    emit previewMessageChanged();
}

void CaptureController::updatePreviewMessage()
{
    if (currentSelectedWindowId == 0)
    {
        setPreviewMessage(QStringLiteral("No video source selected"));
    }
    else if (!capturingActive)
    {
        setPreviewMessage(QStringLiteral("Capture is not running"));
    }
    else if (!previewFrameAvailable)
    {
        setPreviewMessage(QStringLiteral("Waiting for frames..."));
    }
    else if (currentWarningMessage == CaptureFormatting::blackCaptureWarning())
    {
        setPreviewMessage(CaptureFormatting::blackCaptureWarning());
    }
    else
    {
        setPreviewMessage(QString());
    }
}

void CaptureController::handleWindowsReady(int requestId, const QVector<X11WindowInfo>& windows, const QString& error)
{
    if (requestId != latestWindowRefreshRequestId)
    {
        return;
    }

    windowInfos = windows;
    windowModel = windowListModelBuilder.build(windowInfos);
    emit windowsChanged();

    if (!error.isEmpty())
    {
        setWarningMessage(error);
        setStatusMessage(error);
        updatePreviewMessage();

        return;
    }

    if (currentSelectedWindowId != 0)
    {
        if (const X11WindowInfo* info = windowListModelBuilder.findByXid(windowInfos, currentSelectedWindowId))
        {
            updateSourceFromSelectedWindow(*info);
        }
        else
        {
            currentSelectedWindowId = 0;
            currentSelectedWindowTitle.clear();
            currentSelectedSourceType = QStringLiteral("Window");
            setStatusMessage(QStringLiteral("Previously selected window is no longer available"));
            emit selectedWindowChanged();
            updatePreviewMessage();
        }
    }
    else if (windowInfos.isEmpty())
    {
        setStatusMessage(QStringLiteral("No top-level X11 windows found"));
    }
    else
    {
        setStatusMessage(QStringLiteral("Select a source window"));
    }
}

void CaptureController::handleCaptureCommandFailed(quint64 commandId, const QString& error)
{
    Q_UNUSED(commandId)

    captureStartPending = false;
    captureStopPending = false;

    if (recordingActive)
    {
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

    if (captureStopPending && !capturingActive)
    {
        captureStopPending = false;
    }
}

void CaptureController::updateSourceFromSelectedWindow(const X11WindowInfo& info)
{
    const bool selectedMetadataChanged =
        currentSelectedWindowTitle != info.title || currentSelectedSourceType != info.sourceType;
    const bool selectedSourceChanged = captureSettings.windowId != info.xid ||
                                       captureSettings.sourceGeometry != info.geometry ||
                                       captureSettings.sourceCaptureOffset != info.captureOffset;

    currentSelectedWindowTitle = info.title;
    currentSelectedSourceType = info.sourceType;
    captureSettings.windowId = info.xid;
    captureSettings.windowTitle = info.title;
    captureSettings.sourceGeometry = info.geometry;
    captureSettings.sourceCaptureOffset = info.captureOffset;
    syncSourceGeometryProperties();

    if (selectedSourceChanged)
    {
        automaticCropRect = QRect();
    }

    if (selectedMetadataChanged)
    {
        emit selectedWindowChanged();
    }

    emit sourceGeometryChanged();
}

quint64 CaptureController::nextCaptureCommandId()
{
    return ++nextCaptureCommandSequence;
}

void CaptureController::connectRecorderFrameFeed()
{
    // Use a direct connection while recording so recorder enqueue happens from
    // the capture callback without an extra queued UI-thread hop.
    if (recorderFrameConnection)
    {
        return;
    }

    recorderFrameConnection = connect(captureWorker, &CapturePipelineWorker::frameReady, &recorder,
                                      &VideoRecorder::enqueueFrame, Qt::DirectConnection);
}

void CaptureController::disconnectRecorderFrameFeed()
{
    if (!recorderFrameConnection)
    {
        return;
    }

    disconnect(recorderFrameConnection);
    recorderFrameConnection = {};
}
