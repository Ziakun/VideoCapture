#include "app/WindowListModelBuilder.h"

#include "app/CaptureFormatting.h"

#include <QVariantMap>

QVariantList WindowListModelBuilder::build(const QVector<X11WindowInfo>& windows) const
{
    QVariantList model;

    model.reserve(windows.size());

    for (const X11WindowInfo& info : windows)
    {
        QVariantMap item;

        item.insert(QStringLiteral("display"), displayTitle(info));
        item.insert(QStringLiteral("title"), info.title);
        item.insert(QStringLiteral("sourceType"), info.sourceType);
        item.insert(QStringLiteral("sourceHint"), info.sourceHint);
        item.insert(QStringLiteral("className"), info.className);
        item.insert(QStringLiteral("xid"), QVariant::fromValue<qulonglong>(static_cast<qulonglong>(info.xid)));
        item.insert(QStringLiteral("xidText"), CaptureFormatting::xidString(info.xid));
        item.insert(QStringLiteral("geometry"), CaptureFormatting::geometryString(info.geometry));
        item.insert(QStringLiteral("pid"), info.pid >= 0 ? QString::number(info.pid) : QStringLiteral("-"));
        item.insert(QStringLiteral("isVisible"), info.isVisible);
        model.push_back(item);
    }

    return model;
}

const X11WindowInfo* WindowListModelBuilder::findByXid(const QVector<X11WindowInfo>& windows, quint64 xid) const
{
    for (const X11WindowInfo& info : windows)
    {
        if (info.xid == xid)
        {
            return &info;
        }
    }

    return nullptr;
}

QString WindowListModelBuilder::displayTitle(const X11WindowInfo& info)
{
    return QStringLiteral("%1 | %2 | %3 | %4 | %5 | pid %6")
        .arg(info.title)
        .arg(info.sourceType)
        .arg(info.className.isEmpty() ? QStringLiteral("-") : info.className)
        .arg(CaptureFormatting::xidString(info.xid))
        .arg(CaptureFormatting::geometryString(info.geometry))
        .arg(info.pid >= 0 ? QString::number(info.pid) : QStringLiteral("-"));
}
