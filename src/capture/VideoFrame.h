#pragma once

#include <QImage>
#include <QMetaType>

// CPU-side video frame passed between capture, preview, and recorder layers.
//
// The current MVP stores BGRA pixels in QImage. Future platform backends can add
// texture/native-buffer variants behind the same higher-level frame contract.
struct VideoFrame {
    QImage image;
    qint64 timestampNs = 0;

    int width() const { return image.width(); }
    int height() const { return image.height(); }
    bool isValid() const { return !image.isNull(); }
};

Q_DECLARE_METATYPE(VideoFrame)
