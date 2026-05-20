#pragma once

#include <QRect>
#include <QString>

// Validates crop rectangles before they reach GStreamer or the recorder.
//
// The validator clamps out-of-bounds requests, enforces the minimum size used by
// the UI, and rounds dimensions to even values for I420/H.264 compatibility.
class CropRectValidator
{
  public:
    struct Result
    {
        QRect rect;
        QString warningMessage;
    };

    // Returns a safe crop rectangle relative to sourceGeometry and a warning
    // when the requested rectangle had to be clamped.
    Result validate(const QRect& sourceGeometry, const QRect& requested) const;
};
