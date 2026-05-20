#pragma once

#include "config/VideoCaptureConstants.h"

#include <QMetaType>
#include <QPoint>
#include <QRect>
#include <QString>

enum class CaptureMode
{
    X11WindowById,
    ScreenRegionFallback
};

// Immutable input settings for one capture pipeline run.
//
// cropRect is always relative to the selected source window. fallbackScreenRect
// is absolute screen coordinates and is used only by ScreenRegionFallback.
struct CaptureSettings
{
    // Backend mode that decides whether XID capture or screen-region fallback is used.
    CaptureMode mode = CaptureMode::X11WindowById;
    // Native X11 id of the selected top-level window.
    quint64 windowId = 0;
    // Source title copied for diagnostics/status messages.
    QString windowTitle;
    // Visible source geometry in root-screen coordinates.
    QRect sourceGeometry;
    // Offset used by ximagesrc when the visible window starts inside native bounds.
    QPoint sourceCaptureOffset;
    // Selected crop rectangle relative to sourceGeometry.
    QRect cropRect;
    // Absolute screen rectangle used only by ScreenRegionFallback.
    QRect fallbackScreenRect;
    // Requested capture framerate.
    int fps = VideoCaptureConstants::defaultFps;
    // Whether the cursor should be included by ximagesrc.
    bool showCursor = false;
};

Q_DECLARE_METATYPE(CaptureMode)
Q_DECLARE_METATYPE(CaptureSettings)
