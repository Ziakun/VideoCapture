#pragma once

#include "capture/x11/X11WindowInfo.h"

#include <QVector>

// Enumerates candidate top-level X11 windows for user selection.
//
// This class is intentionally X11-only and contains no GStreamer logic. It
// gathers metadata needed by the controller: title, class, XID, geometry, PID,
// and visibility state.
class X11WindowFinder
{
  public:
    // Returns sorted candidate windows and writes an error when X display cannot open.
    QVector<X11WindowInfo> listWindows(QString* errorMessage = nullptr) const;
};
