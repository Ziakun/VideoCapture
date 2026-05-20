#include "recording/GStreamerRecordingFinalizer.h"

#include <gst/app/gstappsrc.h>

namespace
{

constexpr GstClockTime recordingFinalizeTimeout = 10 * GST_SECOND;

} // namespace

GStreamerRecordingFinalizer::Result GStreamerRecordingFinalizer::finalize(GstElement* pipeline,
                                                                          GstElement* appSrc) const
{
    Result result;

    if (!pipeline || !appSrc)
    {
        result.errorMessage = QStringLiteral("Recording pipeline is not available for finalization.");

        return result;
    }

    // EOS is required so matroskamux writes final metadata before we report the
    // file as saved.
    const GstFlowReturn eosResult = gst_app_src_end_of_stream(GST_APP_SRC(appSrc));

    if (eosResult != GST_FLOW_OK)
    {
        result.errorMessage = QStringLiteral("Failed to send EOS to recording pipeline.");

        return result;
    }

    GstBus* const bus = gst_element_get_bus(pipeline);
    GstMessage* const message = gst_bus_timed_pop_filtered(
        bus, recordingFinalizeTimeout, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

    if (message)
    {
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS)
        {
            result.finalized = true;
        }
        else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
        {
            GError* error = nullptr;
            gchar* debug = nullptr;

            gst_message_parse_error(message, &error, &debug);
            result.errorMessage =
                error && error->message
                    ? QStringLiteral("Recording pipeline error: %1").arg(QString::fromUtf8(error->message))
                    : QStringLiteral("Recording pipeline error while finalizing.");

            if (debug)
            {
                g_free(debug);
            }

            if (error)
            {
                g_error_free(error);
            }
        }

        gst_message_unref(message);
    }
    else
    {
        result.errorMessage = QStringLiteral("Timed out while finalizing recording file.");
    }

    gst_object_unref(bus);

    return result;
}
