#include "recording/GStreamerFrameWriter.h"

#include <QImage>

#include <algorithm>
#include <cstring>

#include <gst/app/gstappsrc.h>

namespace
{

constexpr int bytesPerBgraPixel = 4;

} // namespace

bool GStreamerFrameWriter::pushFrame(GstElement* appSrc, const RecordingSettings& settings, quint64 frameIndex,
                                     const VideoFrame& frame) const
{
    if (!appSrc || frame.image.isNull())
    {
        return false;
    }

    const QImage image = frame.image.format() == QImage::Format_ARGB32
                             ? frame.image
                             : frame.image.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width();
    const int height = image.height();
    const int outputStride = width * bytesPerBgraPixel;
    const qsizetype outputSize = static_cast<qsizetype>(outputStride) * height;

    GstBuffer* const buffer = gst_buffer_new_allocate(nullptr, static_cast<gsize>(outputSize), nullptr);

    if (!buffer)
    {
        return false;
    }

    GstMapInfo mapInfo;

    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_WRITE))
    {
        gst_buffer_unref(buffer);

        return false;
    }

    if (image.bytesPerLine() == outputStride)
    {
        // Most BGRA frames are tightly packed. Copying the whole image at once
        // is cheaper than a row loop.
        std::memcpy(mapInfo.data, image.constBits(), static_cast<size_t>(outputSize));
    }
    else
    {
        for (int y = 0; y < height; ++y)
        {
            std::memcpy(mapInfo.data + static_cast<qsizetype>(y) * outputStride, image.constScanLine(y),
                        static_cast<size_t>(outputStride));
        }
    }

    gst_buffer_unmap(buffer, &mapInfo);

    const int fps = std::max(1, settings.fps);

    GST_BUFFER_PTS(buffer) = static_cast<GstClockTime>(frameIndex * GST_SECOND / fps);
    GST_BUFFER_DURATION(buffer) = static_cast<GstClockTime>(GST_SECOND / fps);

    const GstFlowReturn flow = gst_app_src_push_buffer(GST_APP_SRC(appSrc), buffer);

    return flow == GST_FLOW_OK;
}
