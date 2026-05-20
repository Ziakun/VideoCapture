#pragma once

#include "capture/CaptureSettings.h"

#include <QString>

// Builds textual GStreamer pipelines for capture modes.
//
// The runtime pipeline owner remains GStreamerCapturePipeline; this builder is
// only responsible for deterministic gst_parse_launch() descriptions and crop
// margin calculations.
class GStreamerCapturePipelineBuilder
{
  public:
    struct CropMargins
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    // Builds an ximagesrc-by-window pipeline with videocrop and latest-frame
    // appsink behavior.
    QString buildX11WindowDescription(const CaptureSettings& settings) const;

    // Builds a visible screen-region pipeline for fallback capture.
    QString buildScreenRegionDescription(const CaptureSettings& settings) const;

    // Converts the current source geometry and crop rect into videocrop margins.
    static CropMargins cropMargins(const CaptureSettings& settings);

  private:
    static QString boolString(bool value);
    static int nonNegative(int value);
};
