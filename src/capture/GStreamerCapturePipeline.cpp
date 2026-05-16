#include "GStreamerCapturePipeline.h"

#include "utils/Logging.h"

#include <QDateTime>
#include <QImage>
#include <QMutexLocker>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <mutex>

#include <gst/video/video.h>

namespace {

void ensureGStreamerInitialized()
{
    // gst_init() is process-wide. Guard it so capture and recorder objects can
    // be constructed independently without racing initialization.
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
    if (pluginName == QLatin1String("ximagesrc") || pluginName == QLatin1String("matroskamux")) {
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

QString boolString(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

int nonNegative(int value)
{
    return std::max(0, value);
}

} // namespace

GStreamerCapturePipeline::GStreamerCapturePipeline(VideoFrameProvider* frameProvider, QObject* parent)
    : QObject(parent)
    , previewFrameProvider(frameProvider)
{
    ensureGStreamerInitialized();
}

GStreamerCapturePipeline::~GStreamerCapturePipeline()
{
    stop();
}

bool GStreamerCapturePipeline::start(const CaptureSettings& settings, QString* errorMessage)
{
    stop();
    ensureGStreamerInitialized();

    if (!previewFrameProvider) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: preview frame provider is not available.");
        }
        return false;
    }

    if (!checkPlugins(settings, errorMessage)) {
        return false;
    }

    {
        QMutexLocker locker(&stateMutex);
        captureSettings = settings;
    }

    if (!buildPipeline(settings, errorMessage)) {
        stop();
        return false;
    }

    resetDetection();
    {
        QMutexLocker statsLocker(&statsMutex);
        frameStats = FrameStats();
    }
    framesInInterval = 0;
    fpsTimer.restart();
    frameClock.restart();

    running.store(true);

    const GstStateChangeReturn stateResult = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (stateResult == GST_STATE_CHANGE_FAILURE) {
        running.store(false);
        if (errorMessage) {
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

    if (pipelineToStop) {
        gst_element_set_state(pipelineToStop, GST_STATE_NULL);
    }
    if (appSinkToRelease) {
        gst_object_unref(appSinkToRelease);
    }
    if (cropperToRelease) {
        gst_object_unref(cropperToRelease);
    }
    if (pipelineToStop) {
        gst_object_unref(pipelineToStop);
    }

    if (wasRunning) {
        emit captureStopped();
    }
}

bool GStreamerCapturePipeline::isRunning() const
{
    return running.load();
}

bool GStreamerCapturePipeline::updateCropRect(const QRect& cropRect)
{
    QMutexLocker locker(&stateMutex);
    captureSettings.cropRect = cropRect;

    if (!running.load()) {
        return true;
    }

    if (captureSettings.mode != CaptureMode::X11WindowById || !cropper) {
        return false;
    }

    const int sourceWidth = captureSettings.sourceGeometry.width();
    const int sourceHeight = captureSettings.sourceGeometry.height();
    const int left = nonNegative(cropRect.x());
    const int top = nonNegative(cropRect.y());
    const int right = nonNegative(sourceWidth - cropRect.x() - cropRect.width());
    const int bottom = nonNegative(sourceHeight - cropRect.y() - cropRect.height());

    g_object_set(
        G_OBJECT(cropper),
        "left",
        left,
        "top",
        top,
        "right",
        right,
        "bottom",
        bottom,
        nullptr);
    return true;
}

FrameStats GStreamerCapturePipeline::stats() const
{
    return snapshotStats();
}

bool GStreamerCapturePipeline::buildPipeline(const CaptureSettings& settings, QString* errorMessage)
{
    const QString pipelineDescription = settings.mode == CaptureMode::X11WindowById
        ? buildX11WindowPipelineDescription(settings)
        : buildScreenRegionPipelineDescription(settings);

    GError* error = nullptr;
    GstElement* parsedPipeline = gst_parse_launch(pipelineDescription.toUtf8().constData(), &error);
    if (!parsedPipeline) {
        if (errorMessage) {
            *errorMessage = error && error->message
                ? QStringLiteral("Could not build capture pipeline: %1").arg(QString::fromUtf8(error->message))
                : QStringLiteral("Could not build capture pipeline.");
        }
        if (error) {
            g_error_free(error);
        }
        return false;
    }
    if (error) {
        g_error_free(error);
    }

    GstElement* sinkElement = gst_bin_get_by_name(GST_BIN(parsedPipeline), "previewSink");
    if (!sinkElement) {
        gst_object_unref(parsedPipeline);
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not find appsink in capture pipeline.");
        }
        return false;
    }

    GstElement* cropElement = nullptr;
    if (settings.mode == CaptureMode::X11WindowById) {
        cropElement = gst_bin_get_by_name(GST_BIN(parsedPipeline), "cropper");
    }

    GstAppSinkCallbacks callbacks {};
    callbacks.new_sample = &GStreamerCapturePipeline::onNewSample;
    gst_app_sink_set_callbacks(GST_APP_SINK(sinkElement), &callbacks, this, nullptr);

    QMutexLocker locker(&stateMutex);
    pipeline = parsedPipeline;
    appSink = sinkElement;
    cropper = cropElement;
    return true;
}

QString GStreamerCapturePipeline::buildX11WindowPipelineDescription(const CaptureSettings& settings) const
{
    const QRect crop = settings.cropRect;
    const int sourceWidth = settings.sourceGeometry.width();
    const int sourceHeight = settings.sourceGeometry.height();
    const int left = nonNegative(crop.x());
    const int top = nonNegative(crop.y());
    const int right = nonNegative(sourceWidth - crop.x() - crop.width());
    const int bottom = nonNegative(sourceHeight - crop.y() - crop.height());

    // appsink and queue are configured for latest-frame behavior. If the UI
    // falls behind, old preview frames are dropped instead of accumulating.
    return QStringLiteral(
               "ximagesrc xid=%1 use-damage=false show-pointer=%2 "
               "! video/x-raw,framerate=%3/1 "
               "! videocrop name=cropper left=%4 top=%5 right=%6 bottom=%7 "
               "! videoconvert "
               "! video/x-raw,format=BGRA "
               "! queue leaky=downstream max-size-buffers=1 max-size-bytes=0 max-size-time=0 "
               "! appsink name=previewSink emit-signals=true sync=false max-buffers=1 drop=true")
        .arg(settings.windowId)
        .arg(boolString(settings.showCursor))
        .arg(settings.fps)
        .arg(left)
        .arg(top)
        .arg(right)
        .arg(bottom);
}

QString GStreamerCapturePipeline::buildScreenRegionPipelineDescription(const CaptureSettings& settings) const
{
    const QRect region = settings.fallbackScreenRect;
    const int startX = region.x();
    const int startY = region.y();
    const int endX = region.x() + std::max(1, region.width()) - 1;
    const int endY = region.y() + std::max(1, region.height()) - 1;

    // Fallback captures visible screen pixels directly; no videocrop is needed
    // because the ximagesrc rectangle already matches the desired region.
    return QStringLiteral(
               "ximagesrc startx=%1 starty=%2 endx=%3 endy=%4 use-damage=false show-pointer=%5 "
               "! video/x-raw,framerate=%6/1 "
               "! videoconvert "
               "! video/x-raw,format=BGRA "
               "! queue leaky=downstream max-size-buffers=1 max-size-bytes=0 max-size-time=0 "
               "! appsink name=previewSink emit-signals=true sync=false max-buffers=1 drop=true")
        .arg(startX)
        .arg(startY)
        .arg(endX)
        .arg(endY)
        .arg(boolString(settings.showCursor))
        .arg(settings.fps);
}

bool GStreamerCapturePipeline::checkPlugins(const CaptureSettings& settings, QString* errorMessage) const
{
    QStringList required = {
        QStringLiteral("ximagesrc"),
        QStringLiteral("videoconvert"),
        QStringLiteral("queue"),
        QStringLiteral("appsink")
    };

    if (settings.mode == CaptureMode::X11WindowById) {
        required.push_back(QStringLiteral("videocrop"));
    }

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

GstFlowReturn GStreamerCapturePipeline::onNewSample(GstAppSink* sink, gpointer userData)
{
    auto* self = static_cast<GStreamerCapturePipeline*>(userData);
    return self ? self->handleSample(sink) : GST_FLOW_ERROR;
}

GstFlowReturn GStreamerCapturePipeline::handleSample(GstAppSink* sink)
{
    if (!running.load()) {
        return GST_FLOW_FLUSHING;
    }

    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        return GST_FLOW_ERROR;
    }

    GstCaps* caps = gst_sample_get_caps(sample);
    GstVideoInfo videoInfo;
    if (!caps || !gst_video_info_from_caps(&videoInfo, caps)) {
        gst_sample_unref(sample);
        emit errorOccurred(QStringLiteral("Could not read video caps from capture sample."));
        return GST_FLOW_ERROR;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo mapInfo;
    if (!buffer || !gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
        gst_sample_unref(sample);
        emit errorOccurred(QStringLiteral("Could not map capture frame buffer."));
        return GST_FLOW_ERROR;
    }

    const int width = GST_VIDEO_INFO_WIDTH(&videoInfo);
    const int height = GST_VIDEO_INFO_HEIGHT(&videoInfo);
    const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0) > 0
        ? GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0)
        : width * 4;

    const FrameAnalysis analysis = analyzeFrame(mapInfo.data, stride, width, height);

    // The mapped GstBuffer is valid only until gst_buffer_unmap(). Copy once
    // into QImage so preview and recorder can consume the frame asynchronously.
    QImage frameView(mapInfo.data, width, height, stride, QImage::Format_ARGB32);
    QImage frame = frameView.copy();

    qint64 timestampNs = GST_BUFFER_PTS_IS_VALID(buffer)
        ? static_cast<qint64>(GST_BUFFER_PTS(buffer))
        : frameClock.nsecsElapsed();

    gst_buffer_unmap(buffer, &mapInfo);
    gst_sample_unref(sample);

    updateStatsAndWarnings(analysis);

    const bool droppedPreviewFrame = previewFrameProvider->setFrame(frame);
    if (droppedPreviewFrame) {
        QMutexLocker locker(&statsMutex);
        ++frameStats.droppedPreviewFrames;
    }
    emit frameReady(VideoFrame { frame, timestampNs });

    return GST_FLOW_OK;
}

GStreamerCapturePipeline::FrameAnalysis GStreamerCapturePipeline::analyzeFrame(
    const uchar* data,
    int stride,
    int width,
    int height) const
{
    FrameAnalysis analysis;
    if (!data || width <= 0 || height <= 0 || stride <= 0) {
        return analysis;
    }

    // Sampling a small grid is enough for black/stale detection while keeping
    // the appsink callback cheap.
    constexpr int gridX = 16;
    constexpr int gridY = 9;
    constexpr quint64 fnvOffset = 1469598103934665603ull;
    constexpr quint64 fnvPrime = 1099511628211ull;

    quint64 hash = fnvOffset;
    double brightnessSum = 0.0;
    int samples = 0;

    for (int gy = 0; gy < gridY; ++gy) {
        const int y = std::clamp((gy * height) / gridY + height / (gridY * 2), 0, height - 1);
        const uchar* row = data + y * stride;
        for (int gx = 0; gx < gridX; ++gx) {
            const int x = std::clamp((gx * width) / gridX + width / (gridX * 2), 0, width - 1);
            const uchar* pixel = row + x * 4;

            const int b = pixel[0];
            const int g = pixel[1];
            const int r = pixel[2];
            const int brightness = (r * 299 + g * 587 + b * 114) / 1000;

            brightnessSum += brightness;
            hash ^= static_cast<quint64>((r << 16) | (g << 8) | b);
            hash *= fnvPrime;
            ++samples;
        }
    }

    analysis.averageBrightness = samples > 0 ? brightnessSum / samples : 0.0;
    analysis.hash = hash;
    return analysis;
}

void GStreamerCapturePipeline::updateStatsAndWarnings(const FrameAnalysis& analysis)
{
    FrameStats statsSnapshot;
    bool shouldEmitStats = false;
    bool shouldEmitBlack = false;
    bool shouldEmitStale = false;
    QString warning;

    {
        QMutexLocker locker(&statsMutex);
        ++frameStats.totalFrames;
        frameStats.lastFrameTimestampMs = QDateTime::currentMSecsSinceEpoch();
        ++framesInInterval;

        if (analysis.averageBrightness < 8.0) {
            ++blackConsecutiveFrames;
            ++frameStats.blackFrameCount;
        } else {
            blackConsecutiveFrames = 0;
            blackWarningActive = false;
        }

        if (blackConsecutiveFrames >= std::max(1, captureSettings.fps) && !blackWarningActive) {
            blackWarningActive = true;
            shouldEmitBlack = true;
            warning = QStringLiteral("Window capture may be black or stale");
        }

        if (analysis.hash == lastHash && lastHash != 0) {
            ++staleConsecutiveFrames;
            if (staleConsecutiveFrames > std::max(1, captureSettings.fps) * 4) {
                ++frameStats.staleFrameCount;
            }
        } else {
            lastHash = analysis.hash;
            staleConsecutiveFrames = 0;
            staleWarningActive = false;
        }

        if (staleConsecutiveFrames >= std::max(1, captureSettings.fps) * 4 && !staleWarningActive) {
            staleWarningActive = true;
            shouldEmitStale = true;
            if (warning.isEmpty()) {
                warning = QStringLiteral("Window capture may be black or stale");
            }
        }

        const qint64 elapsedMs = fpsTimer.elapsed();
        if (elapsedMs >= 1000) {
            frameStats.currentFps = static_cast<double>(framesInInterval) * 1000.0 / static_cast<double>(elapsedMs);
            framesInInterval = 0;
            fpsTimer.restart();
            statsSnapshot = frameStats;
            shouldEmitStats = true;
        }
    }

    if (shouldEmitBlack) {
        emit blackFrameDetected();
    }
    if (shouldEmitStale) {
        emit staleFramesDetected();
    }
    if (!warning.isEmpty()) {
        emit warningOccurred(warning);
    }
    if (shouldEmitStats) {
        emit fpsUpdated(statsSnapshot.currentFps);
        emit statsUpdated(statsSnapshot);
    }
}

void GStreamerCapturePipeline::resetDetection()
{
    lastHash = 0;
    blackConsecutiveFrames = 0;
    staleConsecutiveFrames = 0;
    blackWarningActive = false;
    staleWarningActive = false;
}

void GStreamerCapturePipeline::startBusThread()
{
    stopBusThread();
    busRunning.store(true);
    busThread = std::thread([this]() { busLoop(); });
}

void GStreamerCapturePipeline::stopBusThread()
{
    busRunning.store(false);
    if (busThread.joinable()) {
        busThread.join();
    }
}

void GStreamerCapturePipeline::busLoop()
{
    while (busRunning.load()) {
        GstElement* pipelineSnapshot = nullptr;
        {
            QMutexLocker locker(&stateMutex);
            pipelineSnapshot = pipeline;
            if (pipelineSnapshot) {
                gst_object_ref(pipelineSnapshot);
            }
        }

        if (!pipelineSnapshot) {
            QThread::msleep(50);
            continue;
        }

        // Bus polling is kept off the UI thread so plugin errors or EOS handling
        // cannot freeze QML.
        GstBus* bus = gst_element_get_bus(pipelineSnapshot);
        GstMessage* message = gst_bus_timed_pop_filtered(
            bus,
            250 * GST_MSECOND,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING | GST_MESSAGE_EOS));

        if (message) {
            switch (GST_MESSAGE_TYPE(message)) {
            case GST_MESSAGE_ERROR: {
                GError* error = nullptr;
                gchar* debug = nullptr;
                gst_message_parse_error(message, &error, &debug);
                const QString messageText = error && error->message
                    ? QStringLiteral("Capture pipeline error: %1").arg(QString::fromUtf8(error->message))
                    : QStringLiteral("Capture pipeline error.");
                emit errorOccurred(messageText);
                if (debug) {
                    g_free(debug);
                }
                if (error) {
                    g_error_free(error);
                }
                gst_element_set_state(pipelineSnapshot, GST_STATE_NULL);
                if (running.exchange(false)) {
                    emit captureStopped();
                }
                break;
            }
            case GST_MESSAGE_WARNING: {
                GError* error = nullptr;
                gchar* debug = nullptr;
                gst_message_parse_warning(message, &error, &debug);
                if (error && error->message) {
                    emit warningOccurred(QStringLiteral("Capture warning: %1").arg(QString::fromUtf8(error->message)));
                }
                if (debug) {
                    g_free(debug);
                }
                if (error) {
                    g_error_free(error);
                }
                break;
            }
            case GST_MESSAGE_EOS:
                gst_element_set_state(pipelineSnapshot, GST_STATE_NULL);
                if (running.exchange(false)) {
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

FrameStats GStreamerCapturePipeline::snapshotStats() const
{
    QMutexLocker locker(&statsMutex);
    return frameStats;
}
