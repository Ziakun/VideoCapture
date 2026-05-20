#pragma once

#include "capture/VideoFrame.h"
#include "config/VideoCaptureConstants.h"
#include "recording/GStreamerFrameWriter.h"
#include "recording/GStreamerRecordingFinalizer.h"
#include "recording/GStreamerRecordingPipelineBuilder.h"
#include "recording/RecordingSettings.h"

#include <QMutex>
#include <QObject>
#include <QWaitCondition>

#include <atomic>
#include <deque>
#include <thread>

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

// Owns the GStreamer appsrc -> encoder -> muxer -> filesink recording pipeline.
//
// Frames are accepted through a bounded queue so recording cannot block preview
// indefinitely. stopAsync() sends EOS and finalizes the file in the worker path,
// not in the UI thread.
class VideoRecorder : public QObject
{
    Q_OBJECT

  public:
    // Initializes process-wide GStreamer state for recording use.
    explicit VideoRecorder(QObject* parent = nullptr);
    // Requests async stop and joins the worker thread.
    ~VideoRecorder() override;

    bool start(const RecordingSettings& settings,
               QString* errorMessage = nullptr); // Starts a new appsrc recording pipeline.
    void stopAsync();                            // Stops accepting frames and lets the worker finalize the file.
    bool pushFrame(const VideoFrame& frame);     // Enqueues a frame into the bounded recorder queue.

  public slots:
    void enqueueFrame(const VideoFrame& frame); // Slot wrapper around pushFrame for capture signal connections.

  signals:
    void recordingStarted(const QString& filePath);
    void recordingStopped(const QString& filePath);
    void recordingFailed(const QString& error);
    void droppedFrameCountChanged(quint64 droppedFrames);

  private:
    bool buildPipeline(const RecordingSettings& settings,
                       QString* errorMessage); // Creates and stores pipeline/appsrc refs.
    void workerLoop();                         // Drains queued frames, sends EOS, and emits final recording result.
    bool pushFrameToAppSrc(const VideoFrame& frame); // Converts the next queued frame into a GstBuffer.
    void cleanupPipeline();                          // Sets pipeline to NULL and releases GstObject references.
    void joinWorkerIfFinished();                     // Joins a completed previous worker before starting again.
    void noteDroppedFrame();                         // Updates dropped-frame count and emits throttled notifications.

    mutable QMutex mutex;
    QWaitCondition queueNotEmpty;
    std::deque<VideoFrame> frameQueue;

    GstElement* pipeline = nullptr;
    GstElement* appSrc = nullptr;

    RecordingSettings recordingSettings;
    QString activeFilePath;
    GStreamerRecordingPipelineBuilder pipelineBuilder;
    GStreamerFrameWriter frameWriter;
    GStreamerRecordingFinalizer recordingFinalizer;

    std::atomic_bool recording = false;
    std::atomic_bool stopping = false;
    std::atomic_bool acceptingFrames = false;
    std::atomic<quint64> droppedFrames = 0;

    std::thread worker;
    quint64 frameIndex = 0;

    static constexpr qsizetype maxQueuedFrames = VideoCaptureConstants::maxQueuedRecordingFrames;
};
