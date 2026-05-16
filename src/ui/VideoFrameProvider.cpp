#include "VideoFrameProvider.h"

#include <QMutexLocker>

VideoFrameProvider::VideoFrameProvider(QObject* parent)
    : QObject(parent)
{
}

bool VideoFrameProvider::hasFrame() const
{
    QMutexLocker locker(&mutex);
    return !latestFrame.isNull();
}

QSize VideoFrameProvider::frameSize() const
{
    QMutexLocker locker(&mutex);
    return latestFrame.size();
}

QImage VideoFrameProvider::currentFrame(quint64* generation) const
{
    QMutexLocker locker(&mutex);
    if (generation) {
        *generation = frameGeneration;
    }
    return latestFrame;
}

quint64 VideoFrameProvider::generation() const
{
    QMutexLocker locker(&mutex);
    return frameGeneration;
}

bool VideoFrameProvider::setFrame(const QImage& frame)
{
    if (frame.isNull()) {
        return false;
    }

    // If rendering is already pending, replace the stored frame but do not post
    // another update event. This keeps the UI event queue from growing under load.
    const bool overwroteUnrenderedFrame = renderPending.exchange(true);
    bool hadFrame = false;
    quint64 newGeneration = 0;
    {
        QMutexLocker locker(&mutex);
        hadFrame = !latestFrame.isNull();
        latestFrame = frame;
        newGeneration = ++frameGeneration;
    }

    if (!hadFrame) {
        emit hasFrameChanged();
    }
    if (!overwroteUnrenderedFrame || newGeneration == 1) {
        emit frameChanged();
    }
    return overwroteUnrenderedFrame;
}

void VideoFrameProvider::markFrameRendered(quint64 renderedGeneration)
{
    bool shouldRequestAnotherUpdate = false;
    {
        QMutexLocker locker(&mutex);
        // Only clear renderPending when the rendered generation is still the
        // latest one. Otherwise a newer frame arrived and needs another pass.
        if (frameGeneration == renderedGeneration) {
            renderPending.store(false);
        } else {
            shouldRequestAnotherUpdate = true;
        }
    }

    if (shouldRequestAnotherUpdate) {
        emit frameChanged();
    }
}

void VideoFrameProvider::clear()
{
    renderPending.store(false);
    bool hadFrame = false;
    {
        QMutexLocker locker(&mutex);
        hadFrame = !latestFrame.isNull();
        latestFrame = QImage();
        ++frameGeneration;
    }

    if (hadFrame) {
        emit hasFrameChanged();
    }
    emit frameChanged();
}
