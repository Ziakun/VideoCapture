#pragma once

#include "ui/VideoFrameProvider.h"

#include <QMetaObject>
#include <QQuickItem>

// Qt Quick scene graph item that displays the latest preview frame.
//
// It uses QSG textures instead of QQuickPaintedItem to avoid an extra raster
// painting step and keep preview latency lower.
class VideoPreviewItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(VideoFrameProvider* frameProvider READ frameProvider WRITE setFrameProvider NOTIFY frameProviderChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)

public:
    explicit VideoPreviewItem(QQuickItem* parent = nullptr);

    VideoFrameProvider* frameProvider() const;
    void setFrameProvider(VideoFrameProvider* newProvider);
    bool hasFrame() const;

    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updateData) override;

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

signals:
    void frameProviderChanged();
    void hasFrameChanged();

private:
    void disconnectProvider();

    VideoFrameProvider* provider = nullptr;
    QMetaObject::Connection frameConnection;
    QMetaObject::Connection hasFrameConnection;
};
