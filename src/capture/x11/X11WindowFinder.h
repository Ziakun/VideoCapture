#pragma once

#include "capture/x11/X11WindowInfo.h"

#include <QVector>

// Enumerates candidate top-level X11 windows for user selection.
//
// This class is intentionally X11-only and contains no GStreamer logic. It
// gathers metadata needed by the controller: title, class, XID, geometry, PID,
// and visibility state.
class X11WindowFinder {
public:
    QVector<X11WindowInfo> listWindows(QString* errorMessage = nullptr) const;
};
