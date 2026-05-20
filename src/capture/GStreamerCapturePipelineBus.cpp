#include "GStreamerCapturePipeline.h"

#include <QMutexLocker>
#include <QThread>

namespace
{

constexpr unsigned long missingPipelineSleepMs = 50;
constexpr GstClockTime busPollTimeout = 250 * GST_MSECOND;

} // namespace

void GStreamerCapturePipeline::startBusThread()
{
    stopBusThread();
    busRunning.store(true);
    busThread = std::thread(
        [this]()
        {
            busLoop();
        });
}

void GStreamerCapturePipeline::stopBusThread()
{
    busRunning.store(false);

    if (busThread.joinable())
    {
        busThread.join();
    }
}

void GStreamerCapturePipeline::busLoop()
{
    while (busRunning.load())
    {
        GstElement* pipelineSnapshot = nullptr;

        {
            QMutexLocker locker(&stateMutex);

            pipelineSnapshot = pipeline;

            if (pipelineSnapshot)
            {
                gst_object_ref(pipelineSnapshot);
            }
        }

        if (!pipelineSnapshot)
        {
            QThread::msleep(missingPipelineSleepMs);

            continue;
        }

        // Bus polling is kept off the UI thread so plugin errors or EOS handling
        // cannot freeze QML.
        GstBus* const bus = gst_element_get_bus(pipelineSnapshot);
        GstMessage* const message = gst_bus_timed_pop_filtered(
            bus, busPollTimeout,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING | GST_MESSAGE_EOS));

        if (message)
        {
            switch (GST_MESSAGE_TYPE(message))
            {
            case GST_MESSAGE_ERROR:
            {
                GError* error = nullptr;
                gchar* debug = nullptr;

                gst_message_parse_error(message, &error, &debug);

                const QString messageText =
                    error && error->message
                        ? QStringLiteral("Capture pipeline error: %1").arg(QString::fromUtf8(error->message))
                        : QStringLiteral("Capture pipeline error.");

                emit errorOccurred(messageText);

                if (debug)
                {
                    g_free(debug);
                }

                if (error)
                {
                    g_error_free(error);
                }

                gst_element_set_state(pipelineSnapshot, GST_STATE_NULL);

                if (running.exchange(false))
                {
                    emit captureStopped();
                }

                break;
            }
            case GST_MESSAGE_WARNING:
            {
                GError* error = nullptr;
                gchar* debug = nullptr;

                gst_message_parse_warning(message, &error, &debug);

                if (error && error->message)
                {
                    emit warningOccurred(QStringLiteral("Capture warning: %1").arg(QString::fromUtf8(error->message)));
                }

                if (debug)
                {
                    g_free(debug);
                }

                if (error)
                {
                    g_error_free(error);
                }

                break;
            }
            case GST_MESSAGE_EOS:
                gst_element_set_state(pipelineSnapshot, GST_STATE_NULL);

                if (running.exchange(false))
                {
                    emit captureStopped();
                }

                break;
            default:
                break;
            }

            gst_message_unref(message);
        }

        gst_object_unref(bus);
        gst_object_unref(pipelineSnapshot);
    }
}
