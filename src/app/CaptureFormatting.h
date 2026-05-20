#pragma once

#include "capture/CaptureSettings.h"

#include <QRect>
#include <QString>

// Formats capture-domain values for QML labels, status text, and filenames.
//
// The controller owns state transitions; this class keeps deterministic text
// conversion separate so UI-facing formatting is reusable and easy to audit.
class CaptureFormatting
{
  public:
    // Formats an X11 id as lowercase hexadecimal text, for example "0x3a00007".
    static QString xidString(quint64 xid);

    // Formats a rectangle as "x,y widthxheight" or "-" when geometry is invalid.
    static QString geometryString(const QRect& rect);

    // Converts the backend capture mode enum into the text shown in QML.
    static QString captureModeText(CaptureMode mode);

    // Chooses a stable recording filename prefix from source type and title.
    static QString recordingPrefixForSource(const QString& sourceType, const QString& title);

    // Formats elapsed recording seconds as HH:MM:SS.
    static QString recordingTimeText(int seconds);

    // Returns the shared warning text for black or stale capture frames.
    static QString blackCaptureWarning();
};
