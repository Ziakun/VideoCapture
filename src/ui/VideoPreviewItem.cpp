#include "VideoPreviewItem.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

namespace
{

QRectF centeredAspectFitRect(const QSize& sourceSize, const QRectF& bounds)
{
    if (sourceSize.isEmpty() || bounds.isEmpty())
    {
        return {};
    }

    const QSizeF scaled = QSizeF(sourceSize).scaled(bounds.size(), Qt::KeepAspectRatio);

    return QRectF(bounds.x() + (bounds.width() - scaled.width()) / 2.0,
                  bounds.y() + (bounds.height() - scaled.height()) / 2.0, scaled.width(), scaled.height());
}

class ManagedTextureNode final : public QSGSimpleTextureNode
{
  public:
    ~ManagedTextureNode() override
    {
        delete texture();
    }

    void replaceTexture(QSGTexture* const newTexture)
    {
        // QSGSimpleTextureNode does not own textures by default. Replace and
        // delete explicitly to avoid leaking one texture per frame.
        QSGTexture* const oldTexture = texture();

        setTexture(newTexture);
        delete oldTexture;
    }
};

} // namespace

VideoPreviewItem::VideoPreviewItem(QQuickItem* parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void VideoPreviewItem::setFrameProvider(VideoFrameProvider* const newProvider)
{
    if (provider == newProvider)
    {
        return;
    }

    disconnectProvider();
    provider = newProvider;

    if (provider)
    {
        frameConnection = connect(
            provider, &VideoFrameProvider::frameChanged, this,
            [this]()
            {
                update();
            },
            Qt::QueuedConnection);
    }

    emit frameProviderChanged();
    update();
}

QSGNode* VideoPreviewItem::updatePaintNode(QSGNode* const oldNode, UpdatePaintNodeData* const updateData)
{
    Q_UNUSED(updateData)

    if (!provider || !window())
    {
        delete oldNode;

        return nullptr;
    }

    quint64 renderedGeneration = 0;
    const QImage frame = provider->currentFrame(&renderedGeneration);

    if (frame.isNull())
    {
        delete oldNode;
        provider->markFrameRendered(renderedGeneration);

        return nullptr;
    }

    ManagedTextureNode* const node = oldNode ? static_cast<ManagedTextureNode*>(oldNode) : new ManagedTextureNode;

    if (!oldNode)
    {
        node->setFiltering(QSGTexture::Linear);
    }

    // Upload through the scene graph instead of QPainter/QQuickPaintedItem. This
    // keeps preview on Qt Quick's rendering path and avoids an extra raster pass.
    QSGTexture* const texture = window()->createTextureFromImage(frame);

    if (!texture)
    {
        delete node;
        provider->markFrameRendered(renderedGeneration);

        return nullptr;
    }

    node->replaceTexture(texture);
    node->setRect(centeredAspectFitRect(frame.size(), boundingRect()));
    provider->markFrameRendered(renderedGeneration);

    return node;
}

void VideoPreviewItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() != oldGeometry.size())
    {
        update();
    }
}

void VideoPreviewItem::disconnectProvider()
{
    if (frameConnection)
    {
        disconnect(frameConnection);
        frameConnection = {};
    }
}
