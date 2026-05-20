#pragma once

#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

struct _XDisplay;

using X11WindowHandle = unsigned long;
using X11AtomHandle = unsigned long;

// Reads raw X11 window properties and geometry.
//
// This class owns low-level Xlib property access only. It copies Xlib-owned
// buffers into Qt values immediately and leaves classification/filtering to
// higher-level window discovery code.
class X11WindowPropertyReader
{
  public:
    struct Geometry
    {
        QRect visibleRect;
        QPoint captureOffset;
    };

    // Reads the EWMH client list from the root window.
    QVector<X11WindowHandle> readClientList(_XDisplay* display) const;

    // Reads root child windows as a fallback for WMs without _NET_CLIENT_LIST.
    QVector<X11WindowHandle> readRootChildren(_XDisplay* display) const;

    // Reads the best available UTF-8 or legacy window title.
    QString readWindowTitle(_XDisplay* display, X11WindowHandle window) const;

    // Reads WM_CLASS and formats resource name/class into one label.
    QString readClassName(_XDisplay* display, X11WindowHandle window) const;

    // Reads _NET_WM_PID, returning -1 when unavailable.
    qint64 readPid(_XDisplay* display, X11WindowHandle window) const;

    // Reads visible root-screen geometry and ximagesrc capture offset.
    Geometry readGeometry(_XDisplay* display, X11WindowHandle window, bool* isMapped, bool* isVisible) const;

  private:
    static QString readUtf8Property(_XDisplay* display, X11WindowHandle window, X11AtomHandle property);
};
