#pragma once

#include <QString>

// Input settings for one recording session.
//
// The MVP writes MKV/H.264 through GStreamer. Width and height must match the
// cropped frame size that recorder receives.
struct RecordingSettings {
    QString outputDirectory;
    QString filePath;
    int fps = 30;
    int width = 0;
    int height = 0;
    int bitrateKbps = 2500;
    QString container = QStringLiteral("mkv");
    QString codec = QStringLiteral("h264");
};
