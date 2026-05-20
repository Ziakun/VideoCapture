#pragma once

#include <QMetaType>

// Lightweight capture/recording counters exposed to the controller and UI.
//
// The hot path updates these values at a low rate where possible; per-frame UI
// updates are intentionally avoided to keep preview latency low.
struct FrameStats
{
    // Last measured capture framerate.
    double currentFps = 0.0;
    // Number of frames processed by the capture callback.
    quint64 totalFrames = 0;
    // Preview frames overwritten before render.
    quint64 droppedPreviewFrames = 0;
    // Recorder frames dropped by bounded queue/backpressure.
    quint64 droppedRecordingFrames = 0;
    // Frames classified as black by sampled brightness.
    quint64 blackFrameCount = 0;
    // Frames counted after sustained identical hashes.
    quint64 staleFrameCount = 0;
    // Wall-clock timestamp of the last processed frame.
    qint64 lastFrameTimestampMs = 0;
};

Q_DECLARE_METATYPE(FrameStats)
