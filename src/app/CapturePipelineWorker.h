#pragma once

#include "capture/CaptureSettings.h"
#include "capture/FrameStats.h"
#include "capture/VideoFrame.h"

#include <QObject>
#include <QRect>
#include <QString>

#include <memory>

class GStreamerCapturePipeline;
class VideoFrameProvider;

// Serial command worker for capture lifecycle operations.
//
// start/stop/restart/update-crop commands run in this object's QThread, so
// potentially slow GStreamer state transitions and plugin calls do not execute
// in the QML thread. Frame delivery still comes from GStreamer streaming
// threads and is forwarded without an extra UI-thread hop.
class CapturePipelineWorker : public QObject {
    Q_OBJECT

public:
    explicit CapturePipelineWorker(VideoFrameProvider* frameProvider, QObject* parent = nullptr);
    ~CapturePipelineWorker() override;

public slots:
    void initialize();
    void startCapture(const CaptureSettings& settings, quint64 commandId);
    void stopCapture(quint64 commandId);
    void restartCapture(const CaptureSettings& settings, quint64 commandId);
    void updateCropRect(const QRect& cropRect, const CaptureSettings& settings, quint64 commandId);
    void shutdown();

signals:
    void captureStarted();
    void captureStopped();
    void frameReady(const VideoFrame& frame);
    void fpsUpdated(double fps);
    void statsUpdated(const FrameStats& stats);
    void warningOccurred(const QString& message);
    void errorOccurred(const QString& message);
    void blackFrameDetected();
    void staleFramesDetected();
    void commandSucceeded(quint64 commandId);
    void commandFailed(quint64 commandId, const QString& errorMessage);

private:
    void ensurePipeline();
    void connectPipelineSignals();

    VideoFrameProvider* previewFrameProvider = nullptr;
    std::unique_ptr<GStreamerCapturePipeline> capturePipeline;
};
