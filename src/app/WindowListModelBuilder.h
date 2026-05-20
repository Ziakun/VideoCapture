#pragma once

#include "capture/x11/X11WindowInfo.h"

#include <QVariantList>
#include <QVector>

// Converts X11 window metadata into the QVariant model consumed by QML.
//
// The finder stays responsible for X11 enumeration. This class is only the
// presentation adapter between backend metadata and WindowSelector.qml roles.
class WindowListModelBuilder
{
  public:
    // Builds one QVariantMap per window with display, title, geometry, pid, and
    // XID roles expected by the QML combo box.
    QVariantList build(const QVector<X11WindowInfo>& windows) const;

    // Returns the first window with the requested XID, or nullptr when absent.
    const X11WindowInfo* findByXid(const QVector<X11WindowInfo>& windows, quint64 xid) const;

  private:
    static QString displayTitle(const X11WindowInfo& info);
};
