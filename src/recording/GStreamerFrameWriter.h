#pragma once

#include "capture/VideoFrame.h"
#include "recording/RecordingSettings.h"

#include <gst/gst.h>

// Writes CPU-side BGRA frames into a GStreamer appsrc element.
//
// The writer keeps image format conversion and GstBuffer timestamping out of
// VideoRecorder so queue/lifecycle code remains focused.
class GStreamerFrameWriter
{
  public:
    // Copies frame pixels into a new GstBuffer, timestamps it from frameIndex,
    // and pushes the buffer into appSrc.
    bool pushFrame(GstElement* appSrc, const RecordingSettings& settings, quint64 frameIndex,
                   const VideoFrame& frame) const;
};
