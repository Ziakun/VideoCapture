#include "GStreamerCapturePipeline.h"

#include "capture/GStreamerCapturePipelineBuilder.h"

#include <QImage>
#include <QMutexLocker>

#include <gst/video/video.h>

namespace
{

constexpr int bytesPerBgraPixel = 4;

} // namespace

bool GStreamerCapturePipeline::buildPipeline(const CaptureSettings& settings, QString* errorMessage)
{
    const GStreamerCapturePipelineBuilder builder;
    const QString pipelineDescription = settings.mode == CaptureMode::X11WindowById
                                            ? builder.buildX11WindowDescription(settings)
                                            : builder.buildScreenRegionDescription(settings);

    GError* error = nullptr;
    GstElement* const parsedPipeline = gst_parse_launch(pipelineDescription.toUtf8().constData(), &error);

    if (!parsedPipeline)
    {
        if (errorMessage)
        {
            *errorMessage =
                error && error->message
                    ? QStringLiteral("Could not build capture pipeline: %1").arg(QString::fromUtf8(error->message))
                    : QStringLiteral("Could not build capture pipeline.");
        }

        if (error)
        {
            g_error_free(error);
        }

        return false;
    }

    if (error)
    {
        g_error_free(error);
    }

    GstElement* const sinkElement = gst_bin_get_by_name(GST_BIN(parsedPipeline), "previewSink");

    if (!sinkElement)
    {
        gst_object_unref(parsedPipeline);

        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Could not find appsink in capture pipeline.");
        }

        return false;
    }

    GstElement* const cropElement =
        settings.mode == CaptureMode::X11WindowById ? gst_bin_get_by_name(GST_BIN(parsedPipeline), "cropper") : nullptr;

    GstAppSinkCallbacks callbacks{};

    callbacks.new_sample = &GStreamerCapturePipeline::onNewSample;
    gst_app_sink_set_callbacks(GST_APP_SINK(sinkElement), &callbacks, this, nullptr);

    QMutexLocker locker(&stateMutex);

    pipeline = parsedPipeline;
    appSink = sinkElement;
    cropper = cropElement;

    return true;
}

GstFlowReturn GStreamerCapturePipeline::onNewSample(GstAppSink* sink, gpointer userData)
{
    auto* const self = static_cast<GStreamerCapturePipeline*>(userData);

    return self ? self->handleSample(sink) : GST_FLOW_ERROR;
}

GstFlowReturn GStreamerCapturePipeline::handleSample(GstAppSink* sink)
{
    if (!running.load())
    {
        return GST_FLOW_FLUSHING;
    }

    GstSample* const sample = gst_app_sink_pull_sample(sink);

    if (!sample)
    {
        return GST_FLOW_ERROR;
    }

    GstCaps* const caps = gst_sample_get_caps(sample);
    GstVideoInfo videoInfo;

    if (!caps || !gst_video_info_from_caps(&videoInfo, caps))
    {
        gst_sample_unref(sample);
        emit errorOccurred(QStringLiteral("Could not read video caps from capture sample."));

        return GST_FLOW_ERROR;
    }

    GstBuffer* const buffer = gst_sample_get_buffer(sample);
    GstMapInfo mapInfo;

    if (!buffer || !gst_buffer_map(buffer, &mapInfo, GST_MAP_READ))
    {
        gst_sample_unref(sample);
        emit errorOccurred(QStringLiteral("Could not map capture frame buffer."));

        return GST_FLOW_ERROR;
    }

    const int width = GST_VIDEO_INFO_WIDTH(&videoInfo);
    const int height = GST_VIDEO_INFO_HEIGHT(&videoInfo);
    const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0) > 0 ? GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0)
                                                                      : width * bytesPerBgraPixel;

    const CaptureFrameMonitor::FrameAnalysis analysis = frameMonitor.analyzeFrame(mapInfo.data, stride, width, height);

    // The mapped GstBuffer is valid only until gst_buffer_unmap(). Copy once
    // into QImage so preview and recorder can consume the frame asynchronously.
    QImage frameView(mapInfo.data, width, height, stride, QImage::Format_ARGB32);

    QImage frame = frameView.copy();
    qint64 timestampNs =
        GST_BUFFER_PTS_IS_VALID(buffer) ? static_cast<qint64>(GST_BUFFER_PTS(buffer)) : frameClock.nsecsElapsed();

    gst_buffer_unmap(buffer, &mapInfo);
    gst_sample_unref(sample);

    emitMonitorUpdate(frameMonitor.recordFrame(analysis, captureSettings.fps));

    const bool droppedPreviewFrame = previewFrameProvider->setFrame(frame);

    if (droppedPreviewFrame)
    {
        frameMonitor.noteDroppedPreviewFrame();
    }

    emit frameReady(VideoFrame{frame, timestampNs});

    return GST_FLOW_OK;
}

void GStreamerCapturePipeline::emitMonitorUpdate(const CaptureFrameMonitor::Update& update)
{
    if (update.emitBlackFrame)
    {
        emit blackFrameDetected();
    }

    if (update.emitStaleFrame)
    {
        emit staleFramesDetected();
    }

    if (!update.warningMessage.isEmpty())
    {
        emit warningOccurred(update.warningMessage);
    }

    if (update.emitStats)
    {
        emit fpsUpdated(update.statsSnapshot.currentFps);
        emit statsUpdated(update.statsSnapshot);
    }
}
