#include "recording/GStreamerRecordingPipelineBuilder.h"

#include "config/VideoCaptureConstants.h"
#include "gstreamer/GStreamerRuntime.h"

#include <algorithm>

#include <gst/app/gstappsrc.h>

bool GStreamerRecordingPipelineBuilder::checkPlugins(QString* errorMessage) const
{
    return GStreamerRuntime::requireElements(
        {QStringLiteral("appsrc"), QStringLiteral("queue"), QStringLiteral("videoconvert"), QStringLiteral("x264enc"),
         QStringLiteral("h264parse"), QStringLiteral("matroskamux"), QStringLiteral("filesink")},
        errorMessage);
}

bool GStreamerRecordingPipelineBuilder::build(const RecordingSettings& settings, PipelineElements* elements,
                                              QString* errorMessage) const
{
    if (!elements)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Internal error: recording pipeline output is not available.");
        }

        return false;
    }

    GError* error = nullptr;
    GstElement* const parsedPipeline = gst_parse_launch(buildDescription(settings).toUtf8().constData(), &error);

    if (!parsedPipeline)
    {
        if (errorMessage)
        {
            *errorMessage =
                error && error->message
                    ? QStringLiteral("Could not build recording pipeline: %1").arg(QString::fromUtf8(error->message))
                    : QStringLiteral("Could not build recording pipeline.");
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

    GstElement* const sourceElement = gst_bin_get_by_name(GST_BIN(parsedPipeline), "recordSrc");

    if (!sourceElement)
    {
        gst_object_unref(parsedPipeline);

        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Could not find appsrc in recording pipeline.");
        }

        return false;
    }

    GstCaps* const caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGRA", "width", G_TYPE_INT,
                                              settings.width, "height", G_TYPE_INT, settings.height, "framerate",
                                              GST_TYPE_FRACTION, std::max(1, settings.fps), 1, nullptr);

    gst_app_src_set_caps(GST_APP_SRC(sourceElement), caps);
    gst_caps_unref(caps);
    gst_app_src_set_stream_type(GST_APP_SRC(sourceElement), GST_APP_STREAM_TYPE_STREAM);

    elements->pipeline = parsedPipeline;
    elements->appSrc = sourceElement;

    return true;
}

QString GStreamerRecordingPipelineBuilder::buildDescription(const RecordingSettings& settings) const
{
    // MKV is used as the default live-recording container because it is less
    // fragile than MP4 if the process exits before perfect finalization.
    return QStringLiteral("appsrc name=recordSrc is-live=true format=time do-timestamp=false block=false "
                          "! queue max-size-buffers=%3 max-size-bytes=0 max-size-time=0 leaky=downstream "
                          "! videoconvert "
                          "! video/x-raw,format=I420 "
                          "! x264enc tune=zerolatency speed-preset=ultrafast key-int-max=%1 bitrate=%2 "
                          "! h264parse "
                          "! matroskamux "
                          "! filesink location=\"%4\"")
        .arg(std::max(1, settings.fps))
        .arg(settings.bitrateKbps)
        .arg(VideoCaptureConstants::maxQueuedRecordingFrames)
        .arg(GStreamerRuntime::escapedString(settings.filePath));
}
