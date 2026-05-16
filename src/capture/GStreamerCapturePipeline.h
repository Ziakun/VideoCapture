#pragma once

#include "capture/CaptureSettings.h"
#include "capture/FrameStats.h"
#include "capture/VideoFrame.h"
#include "ui/VideoFrameProvider.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QString>

#include <atomic>
#include <thread>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

// Owns the GStreamer capture pipeline and converts captured pixels into frames.
//
// The main backend is ximagesrc by X11 window id. A screen-region fallback is
// also supported. The class keeps capture latency low by using leaky queues,
// appsink drop mode, and a latest-frame handoff to VideoFrameProvider.
class GStreamerCapturePipeline : public QObject {
    Q_OBJECT

public:
    explicit GStreamerCapturePipeline(VideoFrameProvider* frameProvider, QObject* parent = nullptr);
    ~GStreamerCapturePipeline() override;

    bool start(const CaptureSettings& settings, QString* errorMessage = nullptr);
    void stop();
    bool isRunning() const;
    bool updateCropRect(const QRect& cropRect);
    FrameStats stats() const;

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

private:
    // A small sampled summary of a frame used for black/stale detection without
    // scanning every pixel.
    struct FrameAnalysis {
        double averageBrightness = 0.0;
        quint64 hash = 0;
    };

    static GstFlowReturn onNewSample(GstAppSink* sink, gpointer userData);

    bool buildPipeline(const CaptureSettings& settings, QString* errorMessage);
    QString buildX11WindowPipelineDescription(const CaptureSettings& settings) const;
    QString buildScreenRegionPipelineDescription(const CaptureSettings& settings) const;
    bool checkPlugins(const CaptureSettings& settings, QString* errorMessage) const;
    GstFlowReturn handleSample(GstAppSink* sink);
    FrameAnalysis analyzeFrame(const uchar* data, int stride, int width, int height) const;
    void updateStatsAndWarnings(const FrameAnalysis& analysis);
    void resetDetection();
    void startBusThread();
    void stopBusThread();
    void busLoop();
    FrameStats snapshotStats() const;

    VideoFrameProvider* previewFrameProvider = nullptr;

    mutable QMutex stateMutex;
    mutable QMutex statsMutex;

    GstElement* pipeline = nullptr;
    GstElement* appSink = nullptr;
    GstElement* cropper = nullptr;

    CaptureSettings captureSettings;
    FrameStats frameStats;

    std::atomic_bool running = false;
    std::atomic_bool busRunning = false;
    std::thread busThread;

    QElapsedTimer fpsTimer;
    QElapsedTimer frameClock;
    int framesInInterval = 0;

    quint64 lastHash = 0;
    int blackConsecutiveFrames = 0;
    int staleConsecutiveFrames = 0;
    bool blackWarningActive = false;
    bool staleWarningActive = false;
};
