#include "X11WindowFinder.h"

#include "utils/Logging.h"

#include <QByteArray>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <limits>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace {

Atom atom(Display* display, const char* name, bool onlyIfExists = true)
{
    return XInternAtom(display, name, onlyIfExists ? True : False);
}

QString readUtf8Property(Display* display, Window window, Atom property)
{
    if (property == None) {
        return {};
    }

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;

    // EWMH properties such as _NET_WM_NAME are Xlib-owned byte arrays. Copy
    // them into Qt types immediately and always release the Xlib allocation.
    const int status = XGetWindowProperty(
        display,
        window,
        property,
        0,
        4096,
        False,
        AnyPropertyType,
        &actualType,
        &actualFormat,
        &itemCount,
        &bytesAfter,
        &data);

    if (status != Success || !data) {
        return {};
    }

    QString value;
    if (actualFormat == 8 && itemCount > 0) {
        value = QString::fromUtf8(reinterpret_cast<const char*>(data), static_cast<int>(itemCount));
        value = value.trimmed();
    }

    XFree(data);
    return value;
}

QString readWindowTitle(Display* display, Window window)
{
    // Prefer the modern UTF-8 EWMH title, then fall back to legacy WM_NAME.
    QString title = readUtf8Property(display, window, atom(display, "_NET_WM_NAME"));
    if (!title.isEmpty()) {
        return title;
    }

    char* fetchedName = nullptr;
    if (XFetchName(display, window, &fetchedName) > 0 && fetchedName) {
        title = QString::fromLocal8Bit(fetchedName).trimmed();
        XFree(fetchedName);
    }

    return title;
}

QString readClassName(Display* display, Window window)
{
    XClassHint classHint;
    classHint.res_name = nullptr;
    classHint.res_class = nullptr;

    QString className;
    if (XGetClassHint(display, window, &classHint) != 0) {
        const QString resourceName = classHint.res_name ? QString::fromLocal8Bit(classHint.res_name).trimmed() : QString();
        const QString resourceClass = classHint.res_class ? QString::fromLocal8Bit(classHint.res_class).trimmed() : QString();

        if (!resourceClass.isEmpty() && !resourceName.isEmpty() && resourceClass != resourceName) {
            className = resourceName + QStringLiteral("/") + resourceClass;
        } else if (!resourceClass.isEmpty()) {
            className = resourceClass;
        } else {
            className = resourceName;
        }

        if (classHint.res_name) {
            XFree(classHint.res_name);
        }
        if (classHint.res_class) {
            XFree(classHint.res_class);
        }
    }

    return className;
}

qint64 readPid(Display* display, Window window)
{
    const Atom pidAtom = atom(display, "_NET_WM_PID");
    if (pidAtom == None) {
        return -1;
    }

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;

    const int status = XGetWindowProperty(
        display,
        window,
        pidAtom,
        0,
        1,
        False,
        XA_CARDINAL,
        &actualType,
        &actualFormat,
        &itemCount,
        &bytesAfter,
        &data);

    if (status != Success || !data || actualFormat != 32 || itemCount == 0) {
        if (data) {
            XFree(data);
        }
        return -1;
    }

    const auto* pidData = reinterpret_cast<unsigned long*>(data);
    const qint64 pid = static_cast<qint64>(pidData[0]);
    XFree(data);
    return pid;
}

QRect readGeometry(Display* display, Window window, bool* isMapped, bool* isVisible)
{
    XWindowAttributes attributes;
    if (XGetWindowAttributes(display, window, &attributes) == 0) {
        if (isMapped) {
            *isMapped = false;
        }
        if (isVisible) {
            *isVisible = false;
        }
        return {};
    }

    if (isMapped) {
        *isMapped = attributes.map_state != IsUnmapped;
    }
    if (isVisible) {
        *isVisible = attributes.map_state == IsViewable && attributes.width > 0 && attributes.height > 0;
    }

    Window child = None;
    int rootX = 0;
    int rootY = 0;
    if (XTranslateCoordinates(
            display,
            window,
            DefaultRootWindow(display),
            0,
            0,
            &rootX,
            &rootY,
            &child)
        == 0) {
        rootX = attributes.x;
        rootY = attributes.y;
    }

    return QRect(rootX, rootY, attributes.width, attributes.height);
}

QVector<Window> readClientList(Display* display)
{
    QVector<Window> windows;
    const Window root = DefaultRootWindow(display);
    // Window managers expose managed top-level windows through _NET_CLIENT_LIST.
    // This avoids internal child windows when the WM supports EWMH.
    const Atom clientListAtom = atom(display, "_NET_CLIENT_LIST");
    if (clientListAtom == None) {
        return windows;
    }

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;

    const int status = XGetWindowProperty(
        display,
        root,
        clientListAtom,
        0,
        std::numeric_limits<long>::max() / 4,
        False,
        XA_WINDOW,
        &actualType,
        &actualFormat,
        &itemCount,
        &bytesAfter,
        &data);

    if (status != Success || !data || actualFormat != 32) {
        if (data) {
            XFree(data);
        }
        return windows;
    }

    const auto* rawWindows = reinterpret_cast<unsigned long*>(data);
    windows.reserve(static_cast<int>(itemCount));
    for (unsigned long i = 0; i < itemCount; ++i) {
        windows.push_back(static_cast<Window>(rawWindows[i]));
    }

    XFree(data);
    return windows;
}

QVector<Window> readRootChildren(Display* display)
{
    // Some lightweight WMs may not expose _NET_CLIENT_LIST. Querying root
    // children is noisier, but it is a useful discovery fallback.
    QVector<Window> windows;
    Window root = DefaultRootWindow(display);
    Window parent = None;
    Window* children = nullptr;
    unsigned int childCount = 0;

    if (XQueryTree(display, root, &root, &parent, &children, &childCount) == 0 || !children) {
        return windows;
    }

    windows.reserve(static_cast<int>(childCount));
    for (unsigned int i = 0; i < childCount; ++i) {
        windows.push_back(children[i]);
    }

    XFree(children);
    return windows;
}

int browserPriority(const X11WindowInfo& info)
{
    // Browser-like windows are sorted first for convenience, but the caller
    // still receives all valid top-level windows.
    const QString haystack = (info.className + QLatin1Char(' ') + info.title).toLower();
    static const QStringList browserTokens = {
        QStringLiteral("firefox"),
        QStringLiteral("google-chrome"),
        QStringLiteral("chrome"),
        QStringLiteral("chromium"),
        QStringLiteral("brave"),
        QStringLiteral("microsoft-edge"),
        QStringLiteral("edge")
    };

    for (int i = 0; i < browserTokens.size(); ++i) {
        if (haystack.contains(browserTokens.at(i))) {
            return i;
        }
    }
    return 1000;
}

QString fallbackTitle(const QString& title, const QString& className, quint64 xid)
{
    if (!title.isEmpty()) {
        return title;
    }
    if (!className.isEmpty()) {
        return className;
    }
    return QStringLiteral("X11 window 0x%1").arg(xid, 0, 16);
}

} // namespace

QVector<X11WindowInfo> X11WindowFinder::listWindows(QString* errorMessage) const
{
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not open X display. This MVP requires an X11 session.");
        }
        return {};
    }

    QVector<Window> rawWindows = readClientList(display);
    if (rawWindows.isEmpty()) {
        rawWindows = readRootChildren(display);
    }

    QVector<X11WindowInfo> windows;
    QSet<quint64> seen;
    windows.reserve(rawWindows.size());

    for (Window window : rawWindows) {
        const quint64 xid = static_cast<quint64>(window);
        if (window == None || seen.contains(xid)) {
            continue;
        }
        seen.insert(xid);

        bool isMapped = false;
        bool isVisible = false;
        const QRect geometry = readGeometry(display, window, &isMapped, &isVisible);
        if (!isMapped || geometry.width() < 64 || geometry.height() < 64) {
            continue;
        }

        X11WindowInfo info;
        info.xid = xid;
        info.title = readWindowTitle(display, window);
        info.className = readClassName(display, window);
        info.pid = readPid(display, window);
        info.geometry = geometry;
        info.isMapped = isMapped;
        info.isVisible = isVisible;

        if (info.title.isEmpty() && info.className.isEmpty()) {
            continue;
        }

        info.title = fallbackTitle(info.title, info.className, info.xid);
        windows.push_back(info);
    }

    XCloseDisplay(display);

    std::sort(windows.begin(), windows.end(), [](const X11WindowInfo& left, const X11WindowInfo& right) {
        const int leftBrowserPriority = browserPriority(left);
        const int rightBrowserPriority = browserPriority(right);
        if (leftBrowserPriority != rightBrowserPriority) {
            return leftBrowserPriority < rightBrowserPriority;
        }
        if (left.isVisible != right.isVisible) {
            return left.isVisible;
        }
        return left.title.localeAwareCompare(right.title) < 0;
    });

    return windows;
}
