#pragma once

#include <QMetaType>
#include <QRect>
#include <QString>
#include <QVector>

// Metadata for a single top-level X11 window.
//
// Geometry is expressed in root-screen coordinates. The XID is the native
// source identifier used by ximagesrc for window capture.
struct X11WindowInfo {
    quint64 xid = 0;
    QString title;
    QString className;
    qint64 pid = -1;
    QRect geometry;
    bool isMapped = false;
    bool isVisible = false;
};

Q_DECLARE_METATYPE(X11WindowInfo)
Q_DECLARE_METATYPE(QVector<X11WindowInfo>)
