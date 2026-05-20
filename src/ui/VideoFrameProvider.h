#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>

#include <atomic>

// Shared latest-frame store between capture callbacks and the Qt Quick item.
//
// It stores only the newest frame. If the render thread has not consumed the
// previous frame yet, newer frames overwrite it and update requests are
// coalesced to avoid event-loop backlog.
class VideoFrameProvider : public QObject
{
    Q_OBJECT

  public:
    // Creates an empty latest-frame store.
    explicit VideoFrameProvider(QObject* parent = nullptr);

    bool hasFrame() const; // Returns true when a non-null frame is stored.
    QImage currentFrame(quint64* generation = nullptr) const; // Returns latest frame copy and optional generation.

    bool setFrame(const QImage& frame); // Replaces the latest frame and coalesces render updates.
    void markFrameRendered(
        quint64 renderedGeneration); // Clears pending state or requests another render if newer frame arrived.
    Q_INVOKABLE void clear();        // Clears stored frame and notifies QML/render users.

  signals:
    void frameChanged();
    void hasFrameChanged();

  private:
    mutable QMutex mutex;
    QImage latestFrame;
    quint64 frameGeneration = 0;
    std::atomic_bool renderPending = false;
};
