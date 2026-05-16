#include "VideoRecorder.h"

#include "utils/Logging.h"

#include <QImage>
#include <QMutexLocker>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <mutex>

namespace {

void ensureGStreamerInitialized()
{
    // Recorder can be used independently from capture; initialize GStreamer
    // once for the whole process.
    static std::once_flag once;
    std::call_once(once, []() {
        int argc = 0;
        char** argv = nullptr;
        gst_init(&argc, &argv);
    });
}

QString pluginInstallHint(const QString& pluginName)
{
    if (pluginName == QLatin1String("x264enc")) {
        return QStringLiteral("Install gstreamer1.0-plugins-ugly.");
    }
    if (pluginName == QLatin1String("matroskamux")) {
        return QStringLiteral("Install gstreamer1.0-plugins-good.");
    }
    if (pluginName == QLatin1String("h264parse")) {
        return QStringLiteral("Install gstreamer1.0-plugins-bad or gstreamer1.0-plugins-good, depending on your Ubuntu release.");
    }
    return QStringLiteral("Install the missing GStreamer runtime plugin package.");
}

QString missingPluginMessage(const QString& pluginName)
{
    return QStringLiteral("Missing GStreamer plugin: %1. %2").arg(pluginName, pluginInstallHint(pluginName));
}

bool hasElementFactory(const QString& name)
{
    GstElementFactory* factory = gst_element_factory_find(name.toUtf8().constData());
    if (!factory) {
        return false;
    }
    gst_object_unref(factory);
    return true;
}

QString escapedGstString(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return value;
}

} // namespace

VideoRecorder::VideoRecorder(QObject* parent)
    : QObject(parent)
{
    ensureGStreamerInitialized();
}

VideoRecorder::~VideoRecorder()
{
    stopAsync();
    if (worker.joinable()) {
        worker.join();
    }
}

bool VideoRecorder::start(const RecordingSettings& settings, QString* errorMessage)
{
    ensureGStreamerInitialized();

    if (recording.load() || stopping.load()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recorder is already running or still finalizing a file.");
        }
        return false;
    }

    joinWorkerIfFinished();

    if (settings.width <= 0 || settings.height <= 0 || settings.filePath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid recording settings.");
        }
        return false;
    }

    if (!checkPlugins(errorMessage)) {
        return false;
    }

    {
        QMutexLocker locker(&mutex);
        recordingSettings = settings;
        activeFilePath = settings.filePath;
        frameQueue.clear();
        frameIndex = 0;
        droppedFrames.store(0);
    }

    if (!buildPipeline(settings, errorMessage)) {
        cleanupPipeline();
        return false;
    }

    const GstStateChangeReturn stateResult = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (stateResult == GST_STATE_CHANGE_FAILURE) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to start GStreamer recording pipeline.");
        }
        cleanupPipeline();
        return false;
    }

    acceptingFrames.store(true);
    stopping.store(false);
    recording.store(true);
    worker = std::thread([this]() { workerLoop(); });

    emit recordingStarted(settings.filePath);
    return true;
}

void VideoRecorder::stopAsync()
{
    if (!recording.load() && !stopping.load()) {
        return;
    }

    // Stop accepting frames immediately so capture/preview are not blocked while
    // the worker sends EOS and waits for the muxer to finalize the file.
    acceptingFrames.store(false);
    stopping.store(true);

    {
        QMutexLocker locker(&mutex);
        frameQueue.clear();
    }
    queueNotEmpty.wakeAll();
}

bool VideoRecorder::isRecording() const
{
    return recording.load();
}

bool VideoRecorder::isStopping() const
{
    return stopping.load();
}

QString VideoRecorder::currentFilePath() const
{
    QMutexLocker locker(&mutex);
    return activeFilePath;
}

bool VideoRecorder::pushFrame(const VideoFrame& frame)
{
    if (!recording.load() || !acceptingFrames.load() || !frame.isValid()) {
        return false;
    }

    bool dropped = false;
    {
        QMutexLocker locker(&mutex);
        if (frame.width() != recordingSettings.width || frame.height() != recordingSettings.height) {
            dropped = true;
        } else if (static_cast<qsizetype>(frameQueue.size()) >= maxQueuedFrames) {
            // Recording is allowed to drop under load. Blocking here would feed
            // latency back into capture and preview.
            dropped = true;
        } else {
            frameQueue.push_back(frame);
        }
    }

    if (dropped) {
        noteDroppedFrame();
        return false;
    }

    queueNotEmpty.wakeOne();
    return true;
}

void VideoRecorder::enqueueFrame(const VideoFrame& frame)
{
    pushFrame(frame);
}

bool VideoRecorder::checkPlugins(QString* errorMessage) const
{
    const QStringList required = {
        QStringLiteral("appsrc"),
        QStringLiteral("queue"),
        QStringLiteral("videoconvert"),
        QStringLiteral("x264enc"),
        QStringLiteral("h264parse"),
        QStringLiteral("matroskamux"),
        QStringLiteral("filesink")
    };

    for (const QString& plugin : required) {
        if (!hasElementFactory(plugin)) {
            if (errorMessage) {
                *errorMessage = missingPluginMessage(plugin);
            }
            return false;
        }
    }
    return true;
}

bool VideoRecorder::buildPipeline(const RecordingSettings& settings, QString* errorMessage)
{
    GError* error = nullptr;
    GstElement* parsedPipeline = gst_parse_launch(buildPipelineDescription(settings).toUtf8().constData(), &error);
    if (!parsedPipeline) {
        if (errorMessage) {
            *errorMessage = error && error->message
                ? QStringLiteral("Could not build recording pipeline: %1").arg(QString::fromUtf8(error->message))
                : QStringLiteral("Could not build recording pipeline.");
        }
        if (error) {
            g_error_free(error);
        }
        return false;
    }
    if (error) {
        g_error_free(error);
    }

    GstElement* sourceElement = gst_bin_get_by_name(GST_BIN(parsedPipeline), "recordSrc");
    if (!sourceElement) {
        gst_object_unref(parsedPipeline);
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not find appsrc in recording pipeline.");
        }
        return false;
    }

    GstCaps* caps = gst_caps_new_simple(
        "video/x-raw",
        "format",
        G_TYPE_STRING,
        "BGRA",
        "width",
        G_TYPE_INT,
        settings.width,
        "height",
        G_TYPE_INT,
        settings.height,
        "framerate",
        GST_TYPE_FRACTION,
        std::max(1, settings.fps),
        1,
        nullptr);
    gst_app_src_set_caps(GST_APP_SRC(sourceElement), caps);
    gst_caps_unref(caps);
    gst_app_src_set_stream_type(GST_APP_SRC(sourceElement), GST_APP_STREAM_TYPE_STREAM);

    QMutexLocker locker(&mutex);
    pipeline = parsedPipeline;
    appSrc = sourceElement;
    return true;
}

QString VideoRecorder::buildPipelineDescription(const RecordingSettings& settings) const
{
    // MKV is used as the default live-recording container because it is less
    // fragile than MP4 if the process exits before perfect finalization.
    return QStringLiteral(
               "appsrc name=recordSrc is-live=true format=time do-timestamp=false block=false "
               "! queue max-size-buffers=30 max-size-bytes=0 max-size-time=0 leaky=downstream "
               "! videoconvert "
               "! video/x-raw,format=I420 "
               "! x264enc tune=zerolatency speed-preset=ultrafast key-int-max=%1 bitrate=%2 "
               "! h264parse "
               "! matroskamux "
               "! filesink location=\"%3\"")
        .arg(std::max(1, settings.fps))
        .arg(settings.bitrateKbps)
        .arg(escapedGstString(settings.filePath));
}

void VideoRecorder::workerLoop()
{
    bool pushFailed = false;
    QString pushError;

    while (true) {
        VideoFrame frame;
        {
            QMutexLocker locker(&mutex);
            while (frameQueue.empty() && !stopping.load()) {
                queueNotEmpty.wait(&mutex, 250);
            }

            if (stopping.load()) {
                break;
            }

            frame = frameQueue.front();
            frameQueue.pop_front();
        }

        if (!pushFrameToAppSrc(frame)) {
            pushFailed = true;
            pushError = QStringLiteral("Failed to push frame into recording pipeline.");
            break;
        }
    }

    acceptingFrames.store(false);

    GstElement* pipelineSnapshot = nullptr;
    GstElement* sourceElement = nullptr;
    RecordingSettings settings;
    {
        QMutexLocker locker(&mutex);
        pipelineSnapshot = pipeline;
        sourceElement = appSrc;
        settings = recordingSettings;
    }

    bool finalized = false;
    QString finalizeError;

    if (pipelineSnapshot && sourceElement && !pushFailed) {
        // EOS is required so matroskamux writes final metadata before we report
        // the file as saved.
        const GstFlowReturn eosResult = gst_app_src_end_of_stream(GST_APP_SRC(sourceElement));
        if (eosResult != GST_FLOW_OK) {
            finalizeError = QStringLiteral("Failed to send EOS to recording pipeline.");
        } else {
            GstBus* bus = gst_element_get_bus(pipelineSnapshot);
            GstMessage* message = gst_bus_timed_pop_filtered(
                bus,
                10 * GST_SECOND,
                static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

            if (message) {
                if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
                    finalized = true;
                } else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
                    GError* error = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_error(message, &error, &debug);
                    finalizeError = error && error->message
                        ? QStringLiteral("Recording pipeline error: %1").arg(QString::fromUtf8(error->message))
                        : QStringLiteral("Recording pipeline error while finalizing.");
                    if (debug) {
                        g_free(debug);
                    }
                    if (error) {
                        g_error_free(error);
                    }
                }
                gst_message_unref(message);
            } else {
                finalizeError = QStringLiteral("Timed out while finalizing recording file.");
            }
            gst_object_unref(bus);
        }
    }

    if (pushFailed) {
        finalizeError = pushError;
    }

    cleanupPipeline();

    recording.store(false);
    stopping.store(false);

    if (finalized && finalizeError.isEmpty()) {
        emit recordingStopped(settings.filePath);
    } else {
        emit recordingFailed(finalizeError.isEmpty() ? QStringLiteral("Recording failed.") : finalizeError);
    }
}

bool VideoRecorder::pushFrameToAppSrc(const VideoFrame& frame)
{
    GstElement* sourceElement = nullptr;
    RecordingSettings settings;
    quint64 currentFrameIndex = 0;

    {
        QMutexLocker locker(&mutex);
        sourceElement = appSrc;
        settings = recordingSettings;
        currentFrameIndex = frameIndex++;
    }

    if (!sourceElement || frame.image.isNull()) {
        return false;
    }

    const QImage image = frame.image.format() == QImage::Format_ARGB32
        ? frame.image
        : frame.image.convertToFormat(QImage::Format_ARGB32);

    const int width = image.width();
    const int height = image.height();
    const int bytesPerPixel = 4;
    const int outputStride = width * bytesPerPixel;
    const qsizetype outputSize = static_cast<qsizetype>(outputStride) * height;

    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, static_cast<gsize>(outputSize), nullptr);
    if (!buffer) {
        return false;
    }

    GstMapInfo mapInfo;
    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_WRITE)) {
        gst_buffer_unref(buffer);
        return false;
    }

    if (image.bytesPerLine() == outputStride) {
        // Most BGRA frames are tightly packed. Copying the whole image at once
        // is cheaper than a row loop.
        std::memcpy(mapInfo.data, image.constBits(), static_cast<size_t>(outputSize));
    } else {
        for (int y = 0; y < height; ++y) {
            std::memcpy(
                mapInfo.data + static_cast<qsizetype>(y) * outputStride,
                image.constScanLine(y),
                static_cast<size_t>(outputStride));
        }
    }
    gst_buffer_unmap(buffer, &mapInfo);

    const int fps = std::max(1, settings.fps);
    GST_BUFFER_PTS(buffer) = static_cast<GstClockTime>(currentFrameIndex * GST_SECOND / fps);
    GST_BUFFER_DURATION(buffer) = static_cast<GstClockTime>(GST_SECOND / fps);

    const GstFlowReturn flow = gst_app_src_push_buffer(GST_APP_SRC(sourceElement), buffer);
    return flow == GST_FLOW_OK;
}

void VideoRecorder::cleanupPipeline()
{
    GstElement* pipelineToRelease = nullptr;
    GstElement* sourceElement = nullptr;
    {
        QMutexLocker locker(&mutex);
        pipelineToRelease = pipeline;
        sourceElement = appSrc;
        pipeline = nullptr;
        appSrc = nullptr;
        frameQueue.clear();
    }

    if (pipelineToRelease) {
        gst_element_set_state(pipelineToRelease, GST_STATE_NULL);
    }
    if (sourceElement) {
        gst_object_unref(sourceElement);
    }
    if (pipelineToRelease) {
        gst_object_unref(pipelineToRelease);
    }
}

void VideoRecorder::joinWorkerIfFinished()
{
    if (worker.joinable() && !recording.load() && !stopping.load()) {
        worker.join();
    }
}

void VideoRecorder::noteDroppedFrame()
{
    const quint64 dropped = ++droppedFrames;
    if (dropped == 1 || dropped % 30 == 0) {
        emit droppedFrameCountChanged(dropped);
    }
}
