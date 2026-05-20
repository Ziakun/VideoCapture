#include "app/CropRectValidator.h"

#include "config/VideoCaptureConstants.h"

#include <algorithm>

CropRectValidator::Result CropRectValidator::validate(const QRect& sourceGeometry, const QRect& requested) const
{
    if (!sourceGeometry.isValid() || sourceGeometry.width() <= 0 || sourceGeometry.height() <= 0)
    {
        return {QRect(0, 0, 0, 0), QStringLiteral("No source geometry available.")};
    }

    constexpr int minimumSize = VideoCaptureConstants::minimumCropSize;
    QRect bounded = requested;

    bounded.setX(std::max(0, bounded.x()));
    bounded.setY(std::max(0, bounded.y()));
    bounded.setWidth(std::max(minimumSize, bounded.width()));
    bounded.setHeight(std::max(minimumSize, bounded.height()));

    if (bounded.width() > sourceGeometry.width())
    {
        bounded.setWidth(sourceGeometry.width());
    }

    if (bounded.height() > sourceGeometry.height())
    {
        bounded.setHeight(sourceGeometry.height());
    }

    if (bounded.x() + bounded.width() > sourceGeometry.width())
    {
        bounded.moveLeft(std::max(0, sourceGeometry.width() - bounded.width()));
    }

    if (bounded.y() + bounded.height() > sourceGeometry.height())
    {
        bounded.moveTop(std::max(0, sourceGeometry.height() - bounded.height()));
    }

    const int minimumEvenWidth = sourceGeometry.width() >= minimumSize
                                     ? minimumSize
                                     : std::max(2, sourceGeometry.width() - sourceGeometry.width() % 2);
    const int minimumEvenHeight = sourceGeometry.height() >= minimumSize
                                      ? minimumSize
                                      : std::max(2, sourceGeometry.height() - sourceGeometry.height() % 2);

    QRect clamped = bounded;

    if (clamped.width() % 2 != 0)
    {
        clamped.setWidth(std::max(minimumEvenWidth, clamped.width() - 1));
    }

    if (clamped.height() % 2 != 0)
    {
        clamped.setHeight(std::max(minimumEvenHeight, clamped.height() - 1));
    }

    QString warning;

    if (bounded != requested)
    {
        warning = QStringLiteral("Crop rectangle was clamped to fit inside the source window.");
    }

    return {clamped, warning};
}
