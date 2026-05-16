#pragma once

#include "capture/x11/X11WindowInfo.h"

#include <QObject>
#include <QString>
#include <QVector>

// Runs X11 window enumeration away from the UI thread.
//
// Refreshing top-level windows is usually fast, but Xlib calls can still stall
// on a slow or broken X server. Keeping this worker separate means the QML
// thread only receives the finished window list.
class WindowRefreshWorker : public QObject {
    Q_OBJECT

public:
    explicit WindowRefreshWorker(QObject* parent = nullptr);

public slots:
    void refresh(int requestId);

signals:
    void windowsReady(int requestId, const QVector<X11WindowInfo>& windows, const QString& errorMessage);
};
