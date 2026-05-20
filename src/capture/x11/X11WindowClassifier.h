#pragma once

#include "capture/x11/X11WindowInfo.h"

#include <QString>

// Classifies X11 windows into source categories used by the UI.
//
// Classification is intentionally a hint for sorting and labels. Capture stays
// generic and still targets the selected top-level X11 window by XID.
class X11WindowClassifier
{
  public:
    struct Result
    {
        QString sourceType = QStringLiteral("Window");
        QString sourceHint;
        int sortPriority = X11WindowDefaults::genericSortPriority;
    };

    // Classifies a window from its title/class metadata.
    Result classify(const QString& title, const QString& className) const;

    // Returns title, class, or a stable XID-based fallback when both are empty.
    QString fallbackTitle(const QString& title, const QString& className, quint64 xid) const;
};
