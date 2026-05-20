#pragma once

#include "recording/RecordingSettings.h"

#include <QString>

#include <gst/gst.h>

// Builds and configures the appsrc-based GStreamer recording pipeline.
//
// VideoRecorder owns lifecycle and queueing; this builder owns plugin
// requirements, gst_parse_launch() text, appsrc lookup, and caps setup.
class GStreamerRecordingPipelineBuilder
{
  public:
    struct PipelineElements
    {
        GstElement* pipeline = nullptr;
        GstElement* appSrc = nullptr;
    };

    // Validates required GStreamer elements for MKV/H.264 recording.
    bool checkPlugins(QString* errorMessage = nullptr) const;

    // Creates the parsed pipeline and returns referenced pipeline/appsrc
    // elements. The caller owns both references and must unref them.
    bool build(const RecordingSettings& settings, PipelineElements* elements, QString* errorMessage = nullptr) const;

    // Builds the gst_parse_launch() pipeline string for current settings.
    QString buildDescription(const RecordingSettings& settings) const;
};
