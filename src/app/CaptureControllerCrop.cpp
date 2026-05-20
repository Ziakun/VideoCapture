#include "CaptureController.h"

#include "app/CaptureFormatting.h"

#include <QImage>
#include <QMetaObject>

#include <algorithm>
#include <cmath>

void CaptureController::setCropRect(int x, int y, int width, int height)
{
    if (captureStartPending || captureStopPending)
    {
        setWarningMessage(QStringLiteral("Wait for the current capture command to finish."));

        return;
    }

    QString warning;
    const QRect rect = validatedCropRect(x, y, width, height, &warning);

    if (recordingActive && rect.size() != captureSettings.cropRect.size())
    {
        setWarningMessage(QStringLiteral("Stop recording before changing crop size."));

        return;
    }

    if (recordingActive && captureSettings.mode == CaptureMode::ScreenRegionFallback &&
        rect != captureSettings.cropRect)
    {
        setWarningMessage(QStringLiteral("Stop recording before changing fallback crop."));

        return;
    }

    applyCropRect(rect);

    if (!warning.isEmpty())
    {
        setWarningMessage(warning);
    }
    else if (currentWarningMessage.startsWith(QStringLiteral("Crop rectangle")))
    {
        setWarningMessage(QString());
    }

    if (capturingActive)
    {
        if (captureSettings.mode == CaptureMode::X11WindowById)
        {
            QMetaObject::invokeMethod(captureWorker, "updateCropRect", Qt::QueuedConnection, Q_ARG(QRect, rect),
                                      Q_ARG(CaptureSettings, captureSettings), Q_ARG(quint64, nextCaptureCommandId()));
        }
        else
        {
            captureSettings.fallbackScreenRect = currentFallbackScreenRect();
            restartCaptureForCurrentSettings();
        }
    }

    const int right = std::max(0, captureSettings.sourceGeometry.width() - rect.x() - rect.width());
    const int bottom = std::max(0, captureSettings.sourceGeometry.height() - rect.y() - rect.height());

    setStatusMessage(
        QStringLiteral("Crop: left %1 right %2 top %3 bottom %4").arg(rect.x()).arg(right).arg(rect.y()).arg(bottom));

    updatePreviewMessage();
}

void CaptureController::resetCropToAutoState()
{
    QRect target = automaticCropRect;

    if (!target.isValid() || target.width() <= 0 || target.height() <= 0)
    {
        target = QRect(0, 0, currentSourceWidth, currentSourceHeight);
    }

    setCropRect(target.x(), target.y(), target.width(), target.height());
}

void CaptureController::tryPendingAutoCrop()
{
    if (!autoCropPending)
    {
        return;
    }

    if (currentSelectedSourceType != QLatin1String("Browser"))
    {
        autoCropPending = false;
        autoCropAttemptsRemaining = 0;

        return;
    }

    if (autoCropAttemptsRemaining <= 0)
    {
        autoCropPending = false;

        return;
    }

    if (captureStartPending || captureStopPending || !capturingActive || !previewFrameProvider.hasFrame())
    {
        return;
    }

    --autoCropAttemptsRemaining;

    if (autoCropToVideoArea() || autoCropAttemptsRemaining <= 0)
    {
        autoCropPending = false;
    }
}

bool CaptureController::autoCropToVideoArea()
{
    if (captureStartPending || captureStopPending)
    {
        return false;
    }

    if (currentSelectedWindowId == 0)
    {
        return false;
    }

    if (!capturingActive || !previewFrameProvider.hasFrame())
    {
        return false;
    }

    const QImage frame = previewFrameProvider.currentFrame();
    const QRect detected = videoContentDetector.detectLargeContentRect(frame);

    if (!detected.isValid())
    {
        return false;
    }

    QRect baseCrop = captureSettings.cropRect;

    if (!baseCrop.isValid() || baseCrop.width() <= 0 || baseCrop.height() <= 0)
    {
        baseCrop = QRect(0, 0, captureSettings.sourceGeometry.width(), captureSettings.sourceGeometry.height());
    }

    const double scaleX = frame.width() > 0 ? static_cast<double>(baseCrop.width()) / frame.width() : 1.0;
    const double scaleY = frame.height() > 0 ? static_cast<double>(baseCrop.height()) / frame.height() : 1.0;
    const QRect target(baseCrop.x() + static_cast<int>(std::round(detected.x() * scaleX)),
                       baseCrop.y() + static_cast<int>(std::round(detected.y() * scaleY)),
                       static_cast<int>(std::round(detected.width() * scaleX)),
                       static_cast<int>(std::round(detected.height() * scaleY)));

    QString warning;

    automaticCropRect = validatedCropRect(target.x(), target.y(), target.width(), target.height(), &warning);
    setStatusMessage(QStringLiteral("Auto crop: %1,%2 %3x%4")
                         .arg(target.x())
                         .arg(target.y())
                         .arg(target.width())
                         .arg(target.height()));
    setCropRect(automaticCropRect.x(), automaticCropRect.y(), automaticCropRect.width(), automaticCropRect.height());

    return true;
}

void CaptureController::switchToScreenRegionFallback()
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

    if (recordingActive)
    {
        setWarningMessage(QStringLiteral("Stop recording before switching capture mode."));

        return;
    }

    captureSettings.mode = CaptureMode::ScreenRegionFallback;
    captureSettings.fallbackScreenRect = currentFallbackScreenRect();
    currentCaptureModeText = CaptureFormatting::captureModeText(captureSettings.mode);
    emit captureModeChanged();

    const QString fallbackWarning = QStringLiteral("Fallback captures visible screen pixels. If another window covers "
                                                   "the selected video region, it will be captured too.");

    setWarningMessage(fallbackWarning);

    if (capturingActive)
    {
        restartCaptureForCurrentSettings();
    }
    else
    {
        setStatusMessage(QStringLiteral("Screen region fallback selected"));
    }

    updatePreviewMessage();
}

QRect CaptureController::validatedCropRect(int x, int y, int width, int height, QString* warningMessage) const
{
    const CropRectValidator::Result result =
        cropRectValidator.validate(captureSettings.sourceGeometry, QRect(x, y, width, height));

    if (warningMessage)
    {
        *warningMessage = result.warningMessage;
    }

    return result.rect;
}

void CaptureController::applyCropRect(const QRect& rect)
{
    if (captureSettings.cropRect == rect)
    {
        return;
    }

    captureSettings.cropRect = rect;
    syncCropProperties();
    emit cropChanged();
}

void CaptureController::syncCropProperties()
{
    currentCropX = captureSettings.cropRect.x();
    currentCropY = captureSettings.cropRect.y();
    currentCropWidth = captureSettings.cropRect.width();
    currentCropHeight = captureSettings.cropRect.height();
}

void CaptureController::syncSourceGeometryProperties()
{
    currentSourceGeometryText = CaptureFormatting::geometryString(captureSettings.sourceGeometry);
    currentSourceWidth = captureSettings.sourceGeometry.width();
    currentSourceHeight = captureSettings.sourceGeometry.height();
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
    if (!capturingActive)
    {
        return;
    }

    previewFrameProvider.clear();
    setHasPreviewFrame(false);
    currentFps = 0.0;
    emit fpsChanged();
    setStatusMessage(QStringLiteral("Restarting capture..."));

    QMetaObject::invokeMethod(captureWorker, "restartCapture", Qt::QueuedConnection,
                              Q_ARG(CaptureSettings, captureSettings), Q_ARG(quint64, nextCaptureCommandId()));
}
