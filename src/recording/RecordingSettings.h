#pragma once

#include "config/VideoCaptureConstants.h"

#include <QString>

// Input settings for one recording session.
//
// The MVP writes MKV/H.264 through GStreamer. Width and height must match the
// cropped frame size that recorder receives.
struct RecordingSettings
{
    // Directory selected by the user for recordings.
    QString outputDirectory;
    // Full path of the file being written.
    QString filePath;
    // Recording framerate used for buffer timestamps and encoder key interval.
    int fps = VideoCaptureConstants::defaultFps;
    // Frame width expected from the crop pipeline.
    int width = 0;
    // Frame height expected from the crop pipeline.
    int height = 0;
    // x264enc target bitrate in kbps.
    int bitrateKbps = VideoCaptureConstants::defaultRecordingBitrateKbps;
    // Container label retained for future format selection.
    QString container = QStringLiteral("mkv");
    // Codec label retained for future format selection.
    QString codec = QStringLiteral("h264");
};
