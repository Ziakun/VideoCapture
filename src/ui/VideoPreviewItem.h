#pragma once

#include "ui/VideoFrameProvider.h"

#include <QMetaObject>
#include <QQuickItem>

// Qt Quick scene graph item that displays the latest preview frame.
//
// It uses QSG textures instead of QQuickPaintedItem to avoid an extra raster
// painting step and keep preview latency lower.
class VideoPreviewItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(VideoFrameProvider* frameProvider MEMBER provider WRITE setFrameProvider NOTIFY frameProviderChanged)

  public:
    // Enables scene graph content for frame rendering.
    explicit VideoPreviewItem(QQuickItem* parent = nullptr);

    void setFrameProvider(VideoFrameProvider* const newProvider); // Reconnects to a provider and schedules redraw.

    QSGNode* updatePaintNode(QSGNode* const oldNode,
                             UpdatePaintNodeData* const updateData) override; // Uploads frame to scene graph texture.

  protected:
    void geometryChange(const QRectF& newGeometry,
                        const QRectF& oldGeometry) override; // Redraws when item size changes.

  signals:
    void frameProviderChanged();

  private:
    void disconnectProvider(); // Drops existing provider signal connections.

    VideoFrameProvider* provider = nullptr;
    QMetaObject::Connection frameConnection;
};
