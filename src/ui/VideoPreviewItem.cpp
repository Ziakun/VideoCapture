#include "VideoPreviewItem.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

namespace {

QRectF centeredAspectFitRect(const QSize& sourceSize, const QRectF& bounds)
{
    if (sourceSize.isEmpty() || bounds.isEmpty()) {
        return {};
    }

    const QSizeF scaled = QSizeF(sourceSize).scaled(bounds.size(), Qt::KeepAspectRatio);
    return QRectF(
        bounds.x() + (bounds.width() - scaled.width()) / 2.0,
        bounds.y() + (bounds.height() - scaled.height()) / 2.0,
        scaled.width(),
        scaled.height());
}

class ManagedTextureNode final : public QSGSimpleTextureNode {
public:
    ~ManagedTextureNode() override
    {
        delete texture();
    }

    void replaceTexture(QSGTexture* newTexture)
    {
        // QSGSimpleTextureNode does not own textures by default. Replace and
        // delete explicitly to avoid leaking one texture per frame.
        QSGTexture* oldTexture = texture();
        setTexture(newTexture);
        delete oldTexture;
    }
};

} // namespace

VideoPreviewItem::VideoPreviewItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

VideoFrameProvider* VideoPreviewItem::frameProvider() const
{
    return provider;
}

void VideoPreviewItem::setFrameProvider(VideoFrameProvider* newProvider)
{
    if (provider == newProvider) {
        return;
    }

    disconnectProvider();
    provider = newProvider;

    if (provider) {
        frameConnection = connect(
            provider,
            &VideoFrameProvider::frameChanged,
            this,
            [this]() { update(); },
            Qt::QueuedConnection);
        hasFrameConnection = connect(
            provider,
            &VideoFrameProvider::hasFrameChanged,
            this,
            [this]() {
                emit hasFrameChanged();
                update();
            },
            Qt::QueuedConnection);
    }

    emit frameProviderChanged();
    emit hasFrameChanged();
    update();
}

bool VideoPreviewItem::hasFrame() const
{
    return provider && provider->hasFrame();
}

QSGNode* VideoPreviewItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updateData)
{
    Q_UNUSED(updateData)

    auto* node = static_cast<ManagedTextureNode*>(oldNode);

    if (!provider || !window()) {
        delete node;
        return nullptr;
    }

    quint64 renderedGeneration = 0;
    const QImage frame = provider->currentFrame(&renderedGeneration);
    if (frame.isNull()) {
        delete node;
        provider->markFrameRendered(renderedGeneration);
        return nullptr;
    }

    if (!node) {
        node = new ManagedTextureNode;
        node->setFiltering(QSGTexture::Linear);
    }

    // Upload through the scene graph instead of QPainter/QQuickPaintedItem. This
    // keeps preview on Qt Quick's rendering path and avoids an extra raster pass.
    QSGTexture* texture = window()->createTextureFromImage(frame);
    if (!texture) {
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
    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}

void VideoPreviewItem::disconnectProvider()
{
    if (frameConnection) {
        disconnect(frameConnection);
        frameConnection = {};
    }
    if (hasFrameConnection) {
        disconnect(hasFrameConnection);
        hasFrameConnection = {};
    }
}
