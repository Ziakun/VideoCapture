#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSize>

#include <atomic>

// Shared latest-frame store between capture callbacks and the Qt Quick item.
//
// It stores only the newest frame. If the render thread has not consumed the
// previous frame yet, newer frames overwrite it and update requests are
// coalesced to avoid event-loop backlog.
class VideoFrameProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(QSize frameSize READ frameSize NOTIFY frameChanged)

public:
    explicit VideoFrameProvider(QObject* parent = nullptr);

    bool hasFrame() const;
    QSize frameSize() const;
    QImage currentFrame(quint64* generation = nullptr) const;
    quint64 generation() const;

    bool setFrame(const QImage& frame);
    void markFrameRendered(quint64 renderedGeneration);
    Q_INVOKABLE void clear();

signals:
    void frameChanged();
    void hasFrameChanged();

private:
    mutable QMutex mutex;
    QImage latestFrame;
    quint64 frameGeneration = 0;
    std::atomic_bool renderPending = false;
};
