#include "X11WindowFinder.h"

#include "capture/x11/X11WindowClassifier.h"
#include "capture/x11/X11WindowPropertyReader.h"
#include "config/VideoCaptureConstants.h"

#include <QSet>

#include <algorithm>

#include <X11/Xlib.h>

QVector<X11WindowInfo> X11WindowFinder::listWindows(QString* errorMessage) const
{
    Display* const display = XOpenDisplay(nullptr);

    if (!display)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Could not open X display. This MVP requires an X11 session.");
        }

        return {};
    }

    const X11WindowPropertyReader reader;
    const X11WindowClassifier classifier;
    QVector<X11WindowHandle> rawWindows = reader.readClientList(display);

    if (rawWindows.isEmpty())
    {
        rawWindows = reader.readRootChildren(display);
    }

    QVector<X11WindowInfo> windows;
    QSet<quint64> seen;

    windows.reserve(rawWindows.size());

    for (X11WindowHandle window : rawWindows)
    {
        const quint64 xid = static_cast<quint64>(window);

        if (window == None || seen.contains(xid))
        {
            continue;
        }

        seen.insert(xid);

        bool isMapped = false;
        bool isVisible = false;
        const X11WindowPropertyReader::Geometry geometry = reader.readGeometry(display, window, &isMapped, &isVisible);

        if (!isMapped || geometry.visibleRect.width() < VideoCaptureConstants::minimumCropSize ||
            geometry.visibleRect.height() < VideoCaptureConstants::minimumCropSize)
        {
            continue;
        }

        X11WindowInfo info;

        info.xid = xid;
        info.title = reader.readWindowTitle(display, window);
        info.className = reader.readClassName(display, window);

        const X11WindowClassifier::Result classification = classifier.classify(info.title, info.className);

        info.sourceType = classification.sourceType;
        info.sourceHint = classification.sourceHint;
        info.sortPriority = classification.sortPriority;
        info.pid = reader.readPid(display, window);
        info.geometry = geometry.visibleRect;
        info.captureOffset = geometry.captureOffset;
        info.isMapped = isMapped;
        info.isVisible = isVisible;

        if (info.title.isEmpty() && info.className.isEmpty())
        {
            continue;
        }

        info.title = classifier.fallbackTitle(info.title, info.className, info.xid);
        windows.push_back(info);
    }

    XCloseDisplay(display);

    std::sort(windows.begin(), windows.end(),
              [](const X11WindowInfo& left, const X11WindowInfo& right)
              {
                  if (left.sortPriority != right.sortPriority)
                  {
                      return left.sortPriority < right.sortPriority;
                  }

                  if (left.isVisible != right.isVisible)
                  {
                      return left.isVisible;
                  }

                  return left.title.localeAwareCompare(right.title) < 0;
              });

    return windows;
}
