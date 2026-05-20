#include "VideoRecorder.h"

#include "gstreamer/GStreamerRuntime.h"

#include <QMutexLocker>

namespace
{

constexpr quint64 firstDroppedFrameNotification = 1;
constexpr quint64 droppedFrameNotificationInterval = VideoCaptureConstants::maxQueuedRecordingFrames;

} // namespace

VideoRecorder::VideoRecorder(QObject* parent) : QObject(parent)
{
    GStreamerRuntime::ensureInitialized();
}

VideoRecorder::~VideoRecorder()
{
    stopAsync();

    if (worker.joinable())
    {
        worker.join();
    }
}

bool VideoRecorder::start(const RecordingSettings& settings, QString* errorMessage)
{
    GStreamerRuntime::ensureInitialized();

    if (recording.load() || stopping.load())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Recorder is already running or still finalizing a file.");
        }

        return false;
    }

    joinWorkerIfFinished();

    if (settings.width <= 0 || settings.height <= 0 || settings.filePath.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Invalid recording settings.");
        }

        return false;
    }

    if (!pipelineBuilder.checkPlugins(errorMessage))
    {
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

    if (!buildPipeline(settings, errorMessage))
    {
        cleanupPipeline();

        return false;
    }

    const GstStateChangeReturn stateResult = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if (stateResult == GST_STATE_CHANGE_FAILURE)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Failed to start GStreamer recording pipeline.");
        }

        cleanupPipeline();

        return false;
    }

    acceptingFrames.store(true);
    stopping.store(false);
    recording.store(true);
    worker = std::thread(
        [this]()
        {
            workerLoop();
        });

    emit recordingStarted(settings.filePath);

    return true;
}

void VideoRecorder::stopAsync()
{
    if (!recording.load() && !stopping.load())
    {
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

bool VideoRecorder::buildPipeline(const RecordingSettings& settings, QString* errorMessage)
{
    GStreamerRecordingPipelineBuilder::PipelineElements elements;

    if (!pipelineBuilder.build(settings, &elements, errorMessage))
    {
        return false;
    }

    QMutexLocker locker(&mutex);

    pipeline = elements.pipeline;
    appSrc = elements.appSrc;

    return true;
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

    if (pipelineToRelease)
    {
        gst_element_set_state(pipelineToRelease, GST_STATE_NULL);
    }

    if (sourceElement)
    {
        gst_object_unref(sourceElement);
    }

    if (pipelineToRelease)
    {
        gst_object_unref(pipelineToRelease);
    }
}

void VideoRecorder::joinWorkerIfFinished()
{
    if (worker.joinable() && !recording.load() && !stopping.load())
    {
        worker.join();
    }
}

void VideoRecorder::noteDroppedFrame()
{
    const quint64 dropped = ++droppedFrames;

    if (dropped == firstDroppedFrameNotification || dropped % droppedFrameNotificationInterval == 0)
    {
        emit droppedFrameCountChanged(dropped);
    }
}
