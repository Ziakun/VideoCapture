#pragma once

#include <QtGlobal>

// Shared numeric constants for capture, preview, and recording code.
//
// These values are domain defaults or cross-module limits. Keeping them in one
// place prevents capture, recorder, tests, and UI-facing logic from drifting.
namespace VideoCaptureConstants
{

inline constexpr int defaultFps = 30;
inline constexpr int defaultRecordingBitrateKbps = 2500;
inline constexpr int minimumCropSize = 64;
inline constexpr qsizetype maxQueuedRecordingFrames = 30;
inline constexpr int millisecondsPerSecond = 1000;
inline constexpr int secondsPerMinute = 60;
inline constexpr int minutesPerHour = 60;
inline constexpr int secondsPerHour = secondsPerMinute * minutesPerHour;
inline constexpr qint64 nanosecondsPerSecond = 1000000000LL;

} // namespace VideoCaptureConstants
