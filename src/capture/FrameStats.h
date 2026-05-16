#pragma once

#include <QMetaType>

// Lightweight capture/recording counters exposed to the controller and UI.
//
// The hot path updates these values at a low rate where possible; per-frame UI
// updates are intentionally avoided to keep preview latency low.
struct FrameStats {
    double currentFps = 0.0;
    quint64 totalFrames = 0;
    quint64 droppedPreviewFrames = 0;
    quint64 droppedRecordingFrames = 0;
    quint64 blackFrameCount = 0;
    quint64 staleFrameCount = 0;
    qint64 lastFrameTimestampMs = 0;
};

Q_DECLARE_METATYPE(FrameStats)
