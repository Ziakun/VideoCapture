#include "capture/x11/X11WindowPropertyReader.h"

#include <limits>

#include <X11/Xatom.h>
#include <X11/Xutil.h>

namespace
{

constexpr long allPropertyItems = std::numeric_limits<long>::max() / 4;
constexpr long pidPropertyItems = 1;
constexpr long utf8PropertyItems = 4096;
constexpr int propertyFormat32 = 32;
constexpr int propertyFormat8 = 8;

Atom atom(Display* display, const char* name, bool onlyIfExists = true)
{
    return XInternAtom(display, name, onlyIfExists ? True : False);
}

} // namespace

QVector<X11WindowHandle> X11WindowPropertyReader::readClientList(_XDisplay* display) const
{
    QVector<X11WindowHandle> windows;
    const Window root = DefaultRootWindow(display);

    // Window managers expose managed top-level windows through _NET_CLIENT_LIST.
    // This avoids internal child windows when the WM supports EWMH.
    const Atom clientListAtom = atom(display, "_NET_CLIENT_LIST");

    if (clientListAtom == None)
    {
        return windows;
    }

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(display, root, clientListAtom, 0, allPropertyItems, False, XA_WINDOW,
                                          &actualType, &actualFormat, &itemCount, &bytesAfter, &data);

    if (status != Success || !data || actualFormat != propertyFormat32)
    {
        if (data)
        {
            XFree(data);
        }

        return windows;
    }

    const auto* const rawWindows = reinterpret_cast<unsigned long*>(data);

    windows.reserve(static_cast<int>(itemCount));
    for (unsigned long i = 0; i < itemCount; ++i)
    {
        windows.push_back(static_cast<Window>(rawWindows[i]));
    }

    XFree(data);

    return windows;
}

QVector<X11WindowHandle> X11WindowPropertyReader::readRootChildren(_XDisplay* display) const
{
    // Some lightweight WMs may not expose _NET_CLIENT_LIST. Querying root
    // children is noisier, but it is a useful discovery fallback.
    QVector<X11WindowHandle> windows;
    Window root = DefaultRootWindow(display);
    Window parent = None;
    Window* children = nullptr;
    unsigned int childCount = 0;

    if (XQueryTree(display, root, &root, &parent, &children, &childCount) == 0 || !children)
    {
        return windows;
    }

    windows.reserve(static_cast<int>(childCount));
    for (unsigned int i = 0; i < childCount; ++i)
    {
        windows.push_back(children[i]);
    }

    XFree(children);

    return windows;
}

QString X11WindowPropertyReader::readWindowTitle(_XDisplay* display, X11WindowHandle window) const
{
    // Prefer the modern UTF-8 EWMH title, then fall back to legacy WM_NAME.
    QString title = readUtf8Property(display, window, atom(display, "_NET_WM_NAME"));

    if (!title.isEmpty())
    {
        return title;
    }

    char* fetchedName = nullptr;

    if (XFetchName(display, window, &fetchedName) > 0 && fetchedName)
    {
        title = QString::fromLocal8Bit(fetchedName).trimmed();

        XFree(fetchedName);
    }

    return title;
}

QString X11WindowPropertyReader::readClassName(_XDisplay* display, X11WindowHandle window) const
{
    XClassHint classHint;

    classHint.res_name = nullptr;
    classHint.res_class = nullptr;

    QString className;

    if (XGetClassHint(display, window, &classHint) != 0)
    {
        const QString resourceName =
            classHint.res_name ? QString::fromLocal8Bit(classHint.res_name).trimmed() : QString();
        const QString resourceClass =
            classHint.res_class ? QString::fromLocal8Bit(classHint.res_class).trimmed() : QString();

        if (!resourceClass.isEmpty() && !resourceName.isEmpty() && resourceClass != resourceName)
        {
            className = resourceName + QStringLiteral("/") + resourceClass;
        }
        else if (!resourceClass.isEmpty())
        {
            className = resourceClass;
        }
        else
        {
            className = resourceName;
        }

        if (classHint.res_name)
        {
            XFree(classHint.res_name);
        }

        if (classHint.res_class)
        {
            XFree(classHint.res_class);
        }
    }

    return className;
}

qint64 X11WindowPropertyReader::readPid(_XDisplay* display, X11WindowHandle window) const
{
    const Atom pidAtom = atom(display, "_NET_WM_PID");

    if (pidAtom == None)
    {
        return -1;
    }

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(display, window, pidAtom, 0, pidPropertyItems, False, XA_CARDINAL,
                                          &actualType, &actualFormat, &itemCount, &bytesAfter, &data);

    if (status != Success || !data || actualFormat != propertyFormat32 || itemCount == 0)
    {
        if (data)
        {
            XFree(data);
        }

        return -1;
    }

    const auto* const pidData = reinterpret_cast<unsigned long*>(data);
    const qint64 pid = static_cast<qint64>(pidData[0]);

    XFree(data);

    return pid;
}

X11WindowPropertyReader::Geometry X11WindowPropertyReader::readGeometry(_XDisplay* display, X11WindowHandle window,
                                                                        bool* isMapped, bool* isVisible) const
{
    XWindowAttributes attributes;

    if (XGetWindowAttributes(display, window, &attributes) == 0)
    {
        if (isMapped)
        {
            *isMapped = false;
        }

        if (isVisible)
        {
            *isVisible = false;
        }

        return {};
    }

    if (isMapped)
    {
        *isMapped = attributes.map_state != IsUnmapped;
    }

    Window child = None;
    int rootX = 0;
    int rootY = 0;

    if (XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &rootX, &rootY, &child) == 0)
    {
        rootX = attributes.x;
        rootY = attributes.y;
    }

    const QRect windowRect(rootX, rootY, attributes.width, attributes.height);

    const QRect rootRect(0, 0, DisplayWidth(display, DefaultScreen(display)),
                         DisplayHeight(display, DefaultScreen(display)));
    const QRect visibleRect = windowRect.intersected(rootRect);

    if (isVisible)
    {
        *isVisible = attributes.map_state == IsViewable && visibleRect.width() > 0 && visibleRect.height() > 0;
    }

    Geometry geometry;

    geometry.visibleRect = visibleRect;
    geometry.captureOffset = QPoint(visibleRect.x() - windowRect.x(), visibleRect.y() - windowRect.y());

    return geometry;
}

QString X11WindowPropertyReader::readUtf8Property(_XDisplay* display, X11WindowHandle window, X11AtomHandle property)
{
    if (property == None)
    {
        return {};
    }

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;

    // EWMH properties such as _NET_WM_NAME are Xlib-owned byte arrays. Copy
    // them into Qt types immediately and always release the Xlib allocation.
    const int status = XGetWindowProperty(display, window, property, 0, utf8PropertyItems, False, AnyPropertyType,
                                          &actualType, &actualFormat, &itemCount, &bytesAfter, &data);

    if (status != Success || !data)
    {
        return {};
    }

    QString value;

    if (actualFormat == propertyFormat8 && itemCount > 0)
    {
        value = QString::fromUtf8(reinterpret_cast<const char*>(data), static_cast<int>(itemCount));
        value = value.trimmed();
    }

    XFree(data);

    return value;
}
