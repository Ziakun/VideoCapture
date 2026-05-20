#include "app/CaptureFormatting.h"

#include "config/VideoCaptureConstants.h"

namespace
{

constexpr int hexBase = 16;

} // namespace

QString CaptureFormatting::xidString(quint64 xid)
{
    return QStringLiteral("0x%1").arg(xid, 0, hexBase);
}

QString CaptureFormatting::geometryString(const QRect& rect)
{
    if (!rect.isValid())
    {
        return QStringLiteral("-");
    }

    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

QString CaptureFormatting::captureModeText(CaptureMode mode)
{
    return mode == CaptureMode::X11WindowById ? QStringLiteral("X11WindowById")
                                              : QStringLiteral("ScreenRegionFallback");
}

QString CaptureFormatting::recordingPrefixForSource(const QString& sourceType, const QString& title)
{
    if (sourceType == QLatin1String("Zoom"))
    {
        return QStringLiteral("zoom-recording");
    }

    const QString titleLower = title.toLower();

    if (titleLower.contains(QStringLiteral("google meet")) || titleLower.contains(QStringLiteral("meet.google")))
    {
        return QStringLiteral("meet-recording");
    }

    if (sourceType == QLatin1String("Browser"))
    {
        return QStringLiteral("browser-recording");
    }

    return QStringLiteral("window-recording");
}

QString CaptureFormatting::recordingTimeText(int seconds)
{
    constexpr int fieldWidth = 2;
    constexpr int decimalBase = 10;
    const int hours = seconds / VideoCaptureConstants::secondsPerHour;
    const int minutes = (seconds / VideoCaptureConstants::secondsPerMinute) % VideoCaptureConstants::secondsPerMinute;
    const int remainingSeconds = seconds % VideoCaptureConstants::secondsPerMinute;

    return QStringLiteral("%1:%2:%3")
        .arg(hours, fieldWidth, decimalBase, QLatin1Char('0'))
        .arg(minutes, fieldWidth, decimalBase, QLatin1Char('0'))
        .arg(remainingSeconds, fieldWidth, decimalBase, QLatin1Char('0'));
}

QString CaptureFormatting::blackCaptureWarning()
{
    return QStringLiteral("Window capture may be black or stale");
}
