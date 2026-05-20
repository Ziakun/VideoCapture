#include "capture/CaptureFrameMonitor.h"

#include "config/VideoCaptureConstants.h"

#include <QDateTime>
#include <QMutexLocker>

#include <algorithm>

namespace
{

constexpr int sampleGridColumns = 16;
constexpr int sampleGridRows = 9;
constexpr int bytesPerBgraPixel = 4;
constexpr int redChannelOffset = 2;
constexpr int greenChannelOffset = 1;
constexpr int blueChannelOffset = 0;
constexpr int redBrightnessWeight = 299;
constexpr int greenBrightnessWeight = 587;
constexpr int blueBrightnessWeight = 114;
constexpr int brightnessWeightScale = 1000;
constexpr int redHashShift = 16;
constexpr int greenHashShift = 8;
constexpr int staleWarningSeconds = 4;
constexpr double blackBrightnessThreshold = 8.0;
constexpr quint64 fnvOffsetBasis = 1469598103934665603ull;
constexpr quint64 fnvPrime = 1099511628211ull;

} // namespace

void CaptureFrameMonitor::reset()
{
    QMutexLocker locker(&mutex);

    frameStats = FrameStats();
    framesInInterval = 0;
    lastHash = 0;
    blackConsecutiveFrames = 0;
    staleConsecutiveFrames = 0;
    blackWarningActive = false;
    staleWarningActive = false;
    fpsTimer.restart();
}

CaptureFrameMonitor::FrameAnalysis CaptureFrameMonitor::analyzeFrame(const uchar* data, int stride, int width,
                                                                     int height) const
{
    FrameAnalysis analysis;

    if (!data || width <= 0 || height <= 0 || stride <= 0)
    {
        return analysis;
    }

    // Sampling a small grid is enough for black/stale detection while keeping
    // the appsink callback cheap.
    quint64 hash = fnvOffsetBasis;
    double brightnessSum = 0.0;
    int samples = 0;

    for (int gy = 0; gy < sampleGridRows; ++gy)
    {
        const int y = std::clamp((gy * height) / sampleGridRows + height / (sampleGridRows * 2), 0, height - 1);
        const uchar* const row = data + y * stride;

        for (int gx = 0; gx < sampleGridColumns; ++gx)
        {
            const int x = std::clamp((gx * width) / sampleGridColumns + width / (sampleGridColumns * 2), 0, width - 1);
            const uchar* const pixel = row + x * bytesPerBgraPixel;

            const int b = pixel[blueChannelOffset];
            const int g = pixel[greenChannelOffset];
            const int r = pixel[redChannelOffset];
            const int brightness = (r * redBrightnessWeight + g * greenBrightnessWeight + b * blueBrightnessWeight) /
                                   brightnessWeightScale;

            brightnessSum += brightness;
            hash ^= static_cast<quint64>((r << redHashShift) | (g << greenHashShift) | b);
            hash *= fnvPrime;
            ++samples;
        }
    }

    analysis.averageBrightness = samples > 0 ? brightnessSum / samples : 0.0;
    analysis.hash = hash;

    return analysis;
}

CaptureFrameMonitor::Update CaptureFrameMonitor::recordFrame(const FrameAnalysis& analysis, int fps)
{
    Update update;
    QMutexLocker locker(&mutex);

    ++frameStats.totalFrames;
    frameStats.lastFrameTimestampMs = QDateTime::currentMSecsSinceEpoch();
    ++framesInInterval;

    if (analysis.averageBrightness < blackBrightnessThreshold)
    {
        ++blackConsecutiveFrames;
        ++frameStats.blackFrameCount;
    }
    else
    {
        blackConsecutiveFrames = 0;
        blackWarningActive = false;
    }

    if (blackConsecutiveFrames >= std::max(1, fps) && !blackWarningActive)
    {
        blackWarningActive = true;
        update.emitBlackFrame = true;
        update.warningMessage = QStringLiteral("Window capture may be black or stale");
    }

    if (analysis.hash == lastHash && lastHash != 0)
    {
        ++staleConsecutiveFrames;

        if (staleConsecutiveFrames > std::max(1, fps) * staleWarningSeconds)
        {
            ++frameStats.staleFrameCount;
        }
    }
    else
    {
        lastHash = analysis.hash;
        staleConsecutiveFrames = 0;
        staleWarningActive = false;
    }

    if (staleConsecutiveFrames >= std::max(1, fps) * staleWarningSeconds && !staleWarningActive)
    {
        staleWarningActive = true;
        update.emitStaleFrame = true;
    }

    const qint64 elapsedMs = fpsTimer.elapsed();

    if (elapsedMs >= VideoCaptureConstants::millisecondsPerSecond)
    {
        frameStats.currentFps = static_cast<double>(framesInInterval) * VideoCaptureConstants::millisecondsPerSecond /
                                static_cast<double>(elapsedMs);
        framesInInterval = 0;
        fpsTimer.restart();
        update.statsSnapshot = frameStats;
        update.emitStats = true;
    }

    return update;
}

void CaptureFrameMonitor::noteDroppedPreviewFrame()
{
    QMutexLocker locker(&mutex);

    ++frameStats.droppedPreviewFrames;
}
