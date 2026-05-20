#pragma once

#include <QMetaType>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

namespace X11WindowDefaults
{

inline constexpr int genericSortPriority = 1000;

} // namespace X11WindowDefaults

// Metadata for a single top-level X11 window.
//
// Geometry is expressed in root-screen coordinates for the visible capture
// area. captureOffset is relative to the native X11 window and is used by
// ximagesrc when a window is partially outside the root screen.
struct X11WindowInfo
{
    // Native X11 window id.
    quint64 xid = 0;
    // User-facing title or fallback label.
    QString title;
    // WM_CLASS-derived class label.
    QString className;
    // UI classification, for example Window, Browser, or Zoom.
    QString sourceType = QStringLiteral("Window");
    // Extra source hint shown in the selector.
    QString sourceHint;
    // Lower values sort earlier in the selector.
    int sortPriority = X11WindowDefaults::genericSortPriority;
    // _NET_WM_PID value, or -1 when unavailable.
    qint64 pid = -1;
    // Visible source geometry in root-screen coordinates.
    QRect geometry;
    // Offset for ximagesrc when native and visible bounds differ.
    QPoint captureOffset;
    // True when X11 map state is not IsUnmapped.
    bool isMapped = false;
    // True when the window is viewable and intersects the root screen.
    bool isVisible = false;
};

Q_DECLARE_METATYPE(X11WindowInfo)
Q_DECLARE_METATYPE(QVector<X11WindowInfo>)
