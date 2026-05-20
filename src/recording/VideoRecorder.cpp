#include "VideoRecorder.h"

#include <QMutexLocker>

namespace
{

constexpr unsigned long queueWaitTimeoutMs = 250;

} // namespace

bool VideoRecorder::pushFrame(const VideoFrame& frame)
{
    if (!recording.load() || !acceptingFrames.load() || frame.image.isNull())
    {
        return false;
    }

    bool dropped = false;

    {
        QMutexLocker locker(&mutex);

        if (frame.image.width() != recordingSettings.width || frame.image.height() != recordingSettings.height)
        {
            dropped = true;
        }
        else if (static_cast<qsizetype>(frameQueue.size()) >= maxQueuedFrames)
        {
            // Recording is allowed to drop under load. Blocking here would feed
            // latency back into capture and preview.
            dropped = true;
        }
        else
        {
            frameQueue.push_back(frame);
        }
    }

    if (dropped)
    {
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

void VideoRecorder::workerLoop()
{
    bool pushFailed = false;
    QString pushError;

    while (true)
    {
        VideoFrame frame;

        {
            QMutexLocker locker(&mutex);

            while (frameQueue.empty() && !stopping.load())
            {
                queueNotEmpty.wait(&mutex, queueWaitTimeoutMs);
            }

            if (stopping.load())
            {
                break;
            }

            frame = frameQueue.front();
            frameQueue.pop_front();
        }

        if (!pushFrameToAppSrc(frame))
        {
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

    GStreamerRecordingFinalizer::Result finalizeResult;

    if (pipelineSnapshot && sourceElement && !pushFailed)
    {
        finalizeResult = recordingFinalizer.finalize(pipelineSnapshot, sourceElement);
    }

    if (pushFailed)
    {
        finalizeResult.errorMessage = pushError;
    }

    cleanupPipeline();

    recording.store(false);
    stopping.store(false);

    if (finalizeResult.finalized && finalizeResult.errorMessage.isEmpty())
    {
        emit recordingStopped(settings.filePath);
    }
    else
    {
        emit recordingFailed(finalizeResult.errorMessage.isEmpty() ? QStringLiteral("Recording failed.")
                                                                   : finalizeResult.errorMessage);
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

    if (!sourceElement || frame.image.isNull())
    {
        return false;
    }

    return frameWriter.pushFrame(sourceElement, settings, currentFrameIndex, frame);
}
