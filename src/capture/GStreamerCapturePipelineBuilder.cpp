#include "capture/GStreamerCapturePipelineBuilder.h"

#include <algorithm>

QString GStreamerCapturePipelineBuilder::buildX11WindowDescription(const CaptureSettings& settings) const
{
    const int sourceWidth = settings.sourceGeometry.width();
    const int sourceHeight = settings.sourceGeometry.height();
    const int sourceStartX = nonNegative(settings.sourceCaptureOffset.x());
    const int sourceStartY = nonNegative(settings.sourceCaptureOffset.y());
    const int sourceEndX = sourceStartX + std::max(1, sourceWidth) - 1;
    const int sourceEndY = sourceStartY + std::max(1, sourceHeight) - 1;
    const CropMargins margins = cropMargins(settings);

    // appsink and queue are configured for latest-frame behavior. If the UI
    // falls behind, old preview frames are dropped instead of accumulating.
    return QStringLiteral("ximagesrc xid=%1 startx=%2 starty=%3 endx=%4 endy=%5 use-damage=false show-pointer=%6 "
                          "! video/x-raw,framerate=%7/1 "
                          "! videocrop name=cropper left=%8 top=%9 right=%10 bottom=%11 "
                          "! videoconvert "
                          "! video/x-raw,format=BGRA "
                          "! queue leaky=downstream max-size-buffers=1 max-size-bytes=0 max-size-time=0 "
                          "! appsink name=previewSink emit-signals=true sync=false max-buffers=1 drop=true")
        .arg(settings.windowId)
        .arg(sourceStartX)
        .arg(sourceStartY)
        .arg(sourceEndX)
        .arg(sourceEndY)
        .arg(boolString(settings.showCursor))
        .arg(settings.fps)
        .arg(margins.left)
        .arg(margins.top)
        .arg(margins.right)
        .arg(margins.bottom);
}

QString GStreamerCapturePipelineBuilder::buildScreenRegionDescription(const CaptureSettings& settings) const
{
    const QRect region = settings.fallbackScreenRect;
    const int startX = region.x();
    const int startY = region.y();
    const int endX = region.x() + std::max(1, region.width()) - 1;
    const int endY = region.y() + std::max(1, region.height()) - 1;

    // Fallback captures visible screen pixels directly; no videocrop is needed
    // because the ximagesrc rectangle already matches the desired region.
    return QStringLiteral("ximagesrc startx=%1 starty=%2 endx=%3 endy=%4 use-damage=false show-pointer=%5 "
                          "! video/x-raw,framerate=%6/1 "
                          "! videoconvert "
                          "! video/x-raw,format=BGRA "
                          "! queue leaky=downstream max-size-buffers=1 max-size-bytes=0 max-size-time=0 "
                          "! appsink name=previewSink emit-signals=true sync=false max-buffers=1 drop=true")
        .arg(startX)
        .arg(startY)
        .arg(endX)
        .arg(endY)
        .arg(boolString(settings.showCursor))
        .arg(settings.fps);
}

GStreamerCapturePipelineBuilder::CropMargins GStreamerCapturePipelineBuilder::cropMargins(
    const CaptureSettings& settings)
{
    const QRect crop = settings.cropRect;
    const int sourceWidth = settings.sourceGeometry.width();
    const int sourceHeight = settings.sourceGeometry.height();

    return {nonNegative(crop.x()), nonNegative(crop.y()), nonNegative(sourceWidth - crop.x() - crop.width()),
            nonNegative(sourceHeight - crop.y() - crop.height())};
}

QString GStreamerCapturePipelineBuilder::boolString(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

int GStreamerCapturePipelineBuilder::nonNegative(int value)
{
    return std::max(0, value);
}
