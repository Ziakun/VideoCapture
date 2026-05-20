#include "GStreamerCapturePipeline.h"

#include "capture/GStreamerCapturePipelineBuilder.h"
#include "gstreamer/GStreamerRuntime.h"

#include <QMutexLocker>
#include <QStringList>

namespace
{

QStringList requiredCaptureElements(CaptureMode mode)
{
    QStringList required = {QStringLiteral("ximagesrc"), QStringLiteral("videoconvert"), QStringLiteral("queue"),
                            QStringLiteral("appsink")};

    if (mode == CaptureMode::X11WindowById)
    {
        required.push_back(QStringLiteral("videocrop"));
    }

    return required;
}

} // namespace

GStreamerCapturePipeline::GStreamerCapturePipeline(VideoFrameProvider* frameProvider, QObject* parent)
    : QObject(parent), previewFrameProvider(frameProvider)
{
    GStreamerRuntime::ensureInitialized();
}

GStreamerCapturePipeline::~GStreamerCapturePipeline()
{
    stop();
}

bool GStreamerCapturePipeline::start(const CaptureSettings& settings, QString* errorMessage)
{
    stop();
    GStreamerRuntime::ensureInitialized();

    if (!previewFrameProvider)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Internal error: preview frame provider is not available.");
        }

        return false;
    }

    if (!GStreamerRuntime::requireElements(requiredCaptureElements(settings.mode), errorMessage))
    {
        return false;
    }

    {
        QMutexLocker locker(&stateMutex);

        captureSettings = settings;
    }

    if (!buildPipeline(settings, errorMessage))
    {
        stop();

        return false;
    }

    frameMonitor.reset();
    frameClock.restart();

    running.store(true);

    const GstStateChangeReturn stateResult = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if (stateResult == GST_STATE_CHANGE_FAILURE)
    {
        running.store(false);

        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Failed to start GStreamer capture pipeline.");
        }

        stop();

        return false;
    }

    startBusThread();
    emit captureStarted();

    return true;
}

void GStreamerCapturePipeline::stop()
{
    const bool wasRunning = running.exchange(false);

    stopBusThread();

    GstElement* pipelineToStop = nullptr;
    GstElement* appSinkToRelease = nullptr;
    GstElement* cropperToRelease = nullptr;

    {
        QMutexLocker locker(&stateMutex);

        // Move owned GstObject pointers out under the lock, then release them
        // after unlocking. State changes can call into plugin code.
        pipelineToStop = pipeline;
        appSinkToRelease = appSink;
        cropperToRelease = cropper;
        pipeline = nullptr;
        appSink = nullptr;
        cropper = nullptr;
    }

    if (pipelineToStop)
    {
        gst_element_set_state(pipelineToStop, GST_STATE_NULL);
    }

    if (appSinkToRelease)
    {
        gst_object_unref(appSinkToRelease);
    }

    if (cropperToRelease)
    {
        gst_object_unref(cropperToRelease);
    }

    if (pipelineToStop)
    {
        gst_object_unref(pipelineToStop);
    }

    if (wasRunning)
    {
        emit captureStopped();
    }
}

bool GStreamerCapturePipeline::updateCropRect(const QRect& cropRect)
{
    QMutexLocker locker(&stateMutex);

    captureSettings.cropRect = cropRect;

    if (!running.load())
    {
        return true;
    }

    if (captureSettings.mode != CaptureMode::X11WindowById || !cropper)
    {
        return false;
    }

    const GStreamerCapturePipelineBuilder::CropMargins margins =
        GStreamerCapturePipelineBuilder::cropMargins(captureSettings);

    g_object_set(G_OBJECT(cropper), "left", margins.left, "top", margins.top, "right", margins.right, "bottom",
                 margins.bottom, nullptr);

    return true;
}
