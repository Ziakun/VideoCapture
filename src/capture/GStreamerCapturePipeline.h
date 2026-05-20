#pragma once

#include "capture/CaptureSettings.h"
#include "capture/CaptureFrameMonitor.h"
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
class GStreamerCapturePipeline : public QObject
{
    Q_OBJECT

  public:
    // Stores the preview provider used for latest-frame delivery.
    explicit GStreamerCapturePipeline(VideoFrameProvider* frameProvider, QObject* parent = nullptr);
    // Stops the pipeline and joins the bus thread.
    ~GStreamerCapturePipeline() override;

    bool start(const CaptureSettings& settings,
               QString* errorMessage = nullptr); // Builds and starts a capture pipeline.
    void stop(); // Stops pipeline, releases GStreamer references, and emits captureStopped if needed.
    bool updateCropRect(const QRect& cropRect); // Updates videocrop margins when the active mode supports it.

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
    static GstFlowReturn onNewSample(GstAppSink* sink, gpointer userData);

    bool buildPipeline(const CaptureSettings& settings, QString* errorMessage); // Parses and wires the GStreamer graph.
    GstFlowReturn handleSample(GstAppSink* sink); // Copies one appsink sample into preview/recorder frame paths.
    void emitMonitorUpdate(const CaptureFrameMonitor::Update& update); // Emits monitor notifications outside locks.
    void startBusThread();                                             // Starts background bus polling.
    void stopBusThread();                                              // Stops and joins the bus polling thread.
    void busLoop();                                                    // Polls GStreamer bus for ERROR/WARNING/EOS.

    VideoFrameProvider* previewFrameProvider = nullptr;

    mutable QMutex stateMutex;

    GstElement* pipeline = nullptr;
    GstElement* appSink = nullptr;
    GstElement* cropper = nullptr;

    CaptureSettings captureSettings;
    CaptureFrameMonitor frameMonitor;

    std::atomic_bool running = false;
    std::atomic_bool busRunning = false;
    std::thread busThread;
    QElapsedTimer frameClock;
};
