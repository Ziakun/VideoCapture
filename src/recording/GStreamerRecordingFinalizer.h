#pragma once

#include <QString>

#include <gst/gst.h>

// Sends EOS and waits for the recording container to finalize.
//
// Matroska still needs a clean end-of-stream path for reliable metadata. This
// class isolates blocking bus wait/error parsing from VideoRecorder's queue
// management.
class GStreamerRecordingFinalizer
{
  public:
    struct Result
    {
        bool finalized = false;
        QString errorMessage;
    };

    // Sends EOS to appSrc and waits for EOS or ERROR on pipeline's bus.
    Result finalize(GstElement* pipeline, GstElement* appSrc) const;
};
