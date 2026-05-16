#pragma once

#include <QMetaType>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

// Metadata for a single top-level X11 window.
//
// Geometry is expressed in root-screen coordinates for the visible capture
// area. captureOffset is relative to the native X11 window and is used by
// ximagesrc when a window is partially outside the root screen.
struct X11WindowInfo {
    quint64 xid = 0;
    QString title;
    QString className;
    QString sourceType = QStringLiteral("Window");
    QString sourceHint;
    int sortPriority = 1000;
    qint64 pid = -1;
    QRect geometry;
    QPoint captureOffset;
    bool isMapped = false;
    bool isVisible = false;
};

Q_DECLARE_METATYPE(X11WindowInfo)
Q_DECLARE_METATYPE(QVector<X11WindowInfo>)
