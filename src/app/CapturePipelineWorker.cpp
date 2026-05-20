#include "CapturePipelineWorker.h"

#include "capture/GStreamerCapturePipeline.h"
#include "ui/VideoFrameProvider.h"

CapturePipelineWorker::CapturePipelineWorker(VideoFrameProvider* frameProvider, QObject* parent)
    : QObject(parent), previewFrameProvider(frameProvider)
{
}

CapturePipelineWorker::~CapturePipelineWorker()
{
    shutdown();
}

void CapturePipelineWorker::initialize()
{
    ensurePipeline();
}

void CapturePipelineWorker::startCapture(const CaptureSettings& settings, quint64 commandId)
{
    ensurePipeline();

    QString error;

    if (!capturePipeline || !capturePipeline->start(settings, &error))
    {
        emit commandFailed(commandId, error.isEmpty() ? QStringLiteral("Failed to start capture.") : error);

        return;
    }

    emit commandSucceeded(commandId);
}

void CapturePipelineWorker::stopCapture(quint64 commandId)
{
    if (capturePipeline)
    {
        capturePipeline->stop();
    }

    emit commandSucceeded(commandId);
}

void CapturePipelineWorker::restartCapture(const CaptureSettings& settings, quint64 commandId)
{
    ensurePipeline();

    QString error;

    if (!capturePipeline || !capturePipeline->start(settings, &error))
    {
        emit commandFailed(commandId, error.isEmpty() ? QStringLiteral("Failed to restart capture.") : error);

        return;
    }

    emit commandSucceeded(commandId);
}

void CapturePipelineWorker::updateCropRect(const QRect& cropRect, const CaptureSettings& settings, quint64 commandId)
{
    ensurePipeline();

    if (!capturePipeline || capturePipeline->updateCropRect(cropRect))
    {
        emit commandSucceeded(commandId);

        return;
    }

    // Fallback mode changes ximagesrc coordinates, and some cropper updates may
    // fail depending on the current pipeline. Restarting in the worker keeps
    // the UI responsive while preserving one serialized capture lifecycle.
    restartCapture(settings, commandId);
}

void CapturePipelineWorker::shutdown()
{
    if (capturePipeline)
    {
        capturePipeline->stop();
        capturePipeline.reset();
    }
}

void CapturePipelineWorker::ensurePipeline()
{
    if (capturePipeline)
    {
        return;
    }

    capturePipeline = std::make_unique<GStreamerCapturePipeline>(previewFrameProvider);
    connectPipelineSignals();
}

void CapturePipelineWorker::connectPipelineSignals()
{
    // Direct forwarding keeps frame delivery on the original GStreamer callback
    // thread. UI receivers still get queued delivery because they live in the
    // QML thread, while the recorder can use a direct bounded enqueue path.
    connect(capturePipeline.get(), &GStreamerCapturePipeline::captureStarted, this,
            &CapturePipelineWorker::captureStarted, Qt::DirectConnection);
    connect(capturePipeline.get(), &GStreamerCapturePipeline::captureStopped, this,
            &CapturePipelineWorker::captureStopped, Qt::DirectConnection);
    connect(capturePipeline.get(), &GStreamerCapturePipeline::frameReady, this, &CapturePipelineWorker::frameReady,
            Qt::DirectConnection);

    connect(capturePipeline.get(), &GStreamerCapturePipeline::fpsUpdated, this, &CapturePipelineWorker::fpsUpdated,
            Qt::DirectConnection);

    connect(capturePipeline.get(), &GStreamerCapturePipeline::statsUpdated, this, &CapturePipelineWorker::statsUpdated,
            Qt::DirectConnection);

    connect(capturePipeline.get(), &GStreamerCapturePipeline::warningOccurred, this,
            &CapturePipelineWorker::warningOccurred, Qt::DirectConnection);
    connect(capturePipeline.get(), &GStreamerCapturePipeline::errorOccurred, this,
            &CapturePipelineWorker::errorOccurred, Qt::DirectConnection);
    connect(capturePipeline.get(), &GStreamerCapturePipeline::blackFrameDetected, this,
            &CapturePipelineWorker::blackFrameDetected, Qt::DirectConnection);
    connect(capturePipeline.get(), &GStreamerCapturePipeline::staleFramesDetected, this,
            &CapturePipelineWorker::staleFramesDetected, Qt::DirectConnection);
}
