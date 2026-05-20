#include "capture/x11/X11WindowClassifier.h"

#include <QStringList>

namespace
{

constexpr int zoomMeetingPriority = 0;
constexpr int zoomWindowPriority = 20;
constexpr int meetingBrowserPriorityBase = 40;
constexpr int browserPriorityBase = 100;
constexpr int hexBase = 16;

} // namespace

X11WindowClassifier::Result X11WindowClassifier::classify(const QString& title, const QString& className) const
{
    // Meeting windows are sorted first for convenience, but the caller still
    // receives all valid top-level windows.
    const QString titleLower = title.toLower();
    const QString classLower = className.toLower();
    const QString haystack = classLower + QLatin1Char(' ') + titleLower;

    if (haystack.contains(QStringLiteral("zoom")))
    {
        Result classification;

        classification.sourceType = QStringLiteral("Zoom");
        classification.sourceHint = QStringLiteral("Zoom desktop app window");
        classification.sortPriority = titleLower.contains(QStringLiteral("meeting")) ||
                                              titleLower.contains(QStringLiteral("conference")) ||
                                              titleLower.contains(QStringLiteral("zoom meeting"))
                                          ? zoomMeetingPriority
                                          : zoomWindowPriority;

        return classification;
    }

    static const QStringList browserTokens = {QStringLiteral("firefox"), QStringLiteral("google-chrome"),
                                              QStringLiteral("chrome"),  QStringLiteral("chromium"),
                                              QStringLiteral("brave"),   QStringLiteral("microsoft-edge"),
                                              QStringLiteral("edge")};

    for (int i = 0; i < browserTokens.size(); ++i)
    {
        if (haystack.contains(browserTokens.at(i)))
        {
            Result classification;

            classification.sourceType = QStringLiteral("Browser");
            classification.sourceHint = QStringLiteral("Browser-rendered meeting window");
            classification.sortPriority = titleLower.contains(QStringLiteral("google meet")) ||
                                                  titleLower.contains(QStringLiteral("meet.google")) ||
                                                  titleLower.contains(QStringLiteral("meet -"))
                                              ? meetingBrowserPriorityBase + i
                                              : browserPriorityBase + i;

            return classification;
        }
    }

    return {};
}

QString X11WindowClassifier::fallbackTitle(const QString& title, const QString& className, quint64 xid) const
{
    if (!title.isEmpty())
    {
        return title;
    }

    if (!className.isEmpty())
    {
        return className;
    }

    return QStringLiteral("X11 window 0x%1").arg(xid, 0, hexBase);
}
