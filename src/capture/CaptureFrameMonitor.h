#pragma once

#include "capture/FrameStats.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QString>

// Tracks capture frame statistics and black/stale-frame detection.
//
// The monitor samples a small grid per frame, updates counters under its own
// mutex, and returns batched notification flags so the GStreamer callback can
// emit Qt signals outside the locked section.
class CaptureFrameMonitor
{
  public:
    struct FrameAnalysis
    {
        double averageBrightness = 0.0;
        quint64 hash = 0;
    };

    struct Update
    {
        FrameStats statsSnapshot;
        bool emitStats = false;
        bool emitBlackFrame = false;
        bool emitStaleFrame = false;
        QString warningMessage;
    };

    // Clears counters and detection state for a new capture run.
    void reset();

    // Samples BGRA frame bytes and returns average brightness plus a stable hash.
    FrameAnalysis analyzeFrame(const uchar* data, int stride, int width, int height) const;

    // Records one analyzed frame and returns any UI notifications to emit.
    Update recordFrame(const FrameAnalysis& analysis, int fps);

    // Increments dropped-preview accounting when the latest-frame provider
    // overwrites a frame before rendering.
    void noteDroppedPreviewFrame();

  private:
    mutable QMutex mutex;
    FrameStats frameStats;
    QElapsedTimer fpsTimer;
    int framesInInterval = 0;
    quint64 lastHash = 0;
    int blackConsecutiveFrames = 0;
    int staleConsecutiveFrames = 0;
    bool blackWarningActive = false;
    bool staleWarningActive = false;
};
