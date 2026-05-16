#pragma once

#include <QMetaType>
#include <QPoint>
#include <QRect>
#include <QString>

enum class CaptureMode {
    X11WindowById,
    ScreenRegionFallback
};

// Immutable input settings for one capture pipeline run.
//
// cropRect is always relative to the selected source window. fallbackScreenRect
// is absolute screen coordinates and is used only by ScreenRegionFallback.
struct CaptureSettings {
    CaptureMode mode = CaptureMode::X11WindowById;
    quint64 windowId = 0;
    QString windowTitle;
    QRect sourceGeometry;
    QPoint sourceCaptureOffset;
    QRect cropRect;
    QRect fallbackScreenRect;
    int fps = 30;
    bool showCursor = false;
};

Q_DECLARE_METATYPE(CaptureMode)
Q_DECLARE_METATYPE(CaptureSettings)
