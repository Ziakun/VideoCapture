#include "WindowRefreshWorker.h"

#include "capture/x11/X11WindowFinder.h"

WindowRefreshWorker::WindowRefreshWorker(QObject* parent) : QObject(parent)
{
}

void WindowRefreshWorker::refresh(int requestId)
{
    X11WindowFinder finder;
    QString error;
    const QVector<X11WindowInfo> windows = finder.listWindows(&error);

    emit windowsReady(requestId, windows, error);
}
