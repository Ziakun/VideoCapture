#include "capture/VideoContentDetector.h"

#include "config/VideoCaptureConstants.h"

#include <algorithm>
#include <cmath>

namespace
{

constexpr int samplingScaleDivisor = 520;
constexpr int minimumSamplingStep = 1;
constexpr int maximumSamplingStep = 5;
constexpr int horizontalInsetDivisor = 10;
constexpr int topSearchMinimumY = 24;
constexpr int bottomSearchInset = 24;
constexpr int topSearchFallbackY = 25;
constexpr int lowerHalfPercent = 55;
constexpr int percentScale = 100;
constexpr double horizontalBoundaryThreshold = 12.0;
constexpr double verticalBoundaryThreshold = 6.0;
constexpr int leftSearchStart = 4;
constexpr int rightSearchInset = 5;
constexpr int oneThirdDivisor = 3;
constexpr int twoThirdsMultiplier = 2;
constexpr int minimumMargin = 8;
constexpr int marginDivisor = 90;
constexpr int minimumContentAreaDivisor = 8;

} // namespace

QRect VideoContentDetector::detectLargeContentRect(const QImage& frame) const
{
    if (frame.isNull() || frame.width() < VideoCaptureConstants::minimumCropSize ||
        frame.height() < VideoCaptureConstants::minimumCropSize)
    {
        return {};
    }

    const QImage image = frame.convertToFormat(QImage::Format_RGB32);
    const int width = image.width();
    const int height = image.height();
    const int step =
        std::clamp(std::min(width, height) / samplingScaleDivisor, minimumSamplingStep, maximumSamplingStep);
    const int horizontalXStart = std::clamp(width / horizontalInsetDivisor, 0, width - 1);
    const int horizontalXEnd = std::clamp(width - width / horizontalInsetDivisor, horizontalXStart, width - 1);

    double topScore = 0.0;
    const int top = strongestHorizontalBoundary(image, std::max(topSearchMinimumY, height / horizontalInsetDivisor),
                                                std::max(topSearchFallbackY, height * lowerHalfPercent / percentScale),
                                                horizontalXStart, horizontalXEnd, step, &topScore);

    if (top < 0 || topScore < horizontalBoundaryThreshold)
    {
        return {};
    }

    double bottomScore = 0.0;
    const int bottom = strongestHorizontalBoundary(
        image,
        std::min(height - bottomSearchInset,
                 std::max(top + VideoCaptureConstants::minimumCropSize, height * lowerHalfPercent / percentScale)),
        height - bottomSearchInset, horizontalXStart, horizontalXEnd, step, &bottomScore);

    if (bottom <= top + VideoCaptureConstants::minimumCropSize || bottomScore < horizontalBoundaryThreshold)
    {
        return {};
    }

    double leftScore = 0.0;
    const int left = strongestVerticalBoundary(
        image, leftSearchStart, std::max(leftSearchStart, width / oneThirdDivisor), top, bottom, step, &leftScore);

    double rightScore = 0.0;
    const int right = strongestVerticalBoundary(
        image, std::min(width - rightSearchInset, width * twoThirdsMultiplier / oneThirdDivisor),
        width - rightSearchInset, top, bottom, step, &rightScore);

    if (left < 0 || right <= left + VideoCaptureConstants::minimumCropSize || leftScore < verticalBoundaryThreshold ||
        rightScore < verticalBoundaryThreshold)
    {
        return {};
    }

    const int margin = std::max(minimumMargin, std::min(width, height) / marginDivisor);
    const QRect detected(QPoint(std::max(0, left - margin), std::max(0, top - margin)),
                         QPoint(std::min(width - 1, right + margin), std::min(height - 1, bottom + margin)));

    if (detected.width() < VideoCaptureConstants::minimumCropSize ||
        detected.height() < VideoCaptureConstants::minimumCropSize)
    {
        return {};
    }

    if (static_cast<qint64>(detected.width()) * detected.height() <
        static_cast<qint64>(width) * height / minimumContentAreaDivisor)
    {
        return {};
    }

    return detected;
}

int VideoContentDetector::pixelDistance(QRgb left, QRgb right)
{
    return std::abs(qRed(left) - qRed(right)) + std::abs(qGreen(left) - qGreen(right)) +

           std::abs(qBlue(left) - qBlue(right));
}

double VideoContentDetector::horizontalBoundaryScore(const QImage& image, int y, int xStart, int xEnd, int step)
{
    if (y <= 0 || y >= image.height())
    {
        return 0.0;
    }

    const auto* const previousRow = reinterpret_cast<const QRgb*>(image.constScanLine(y - 1));
    const auto* const currentRow = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    double total = 0.0;
    int samples = 0;

    for (int x = xStart; x <= xEnd; x += step)
    {
        total += pixelDistance(previousRow[x], currentRow[x]);
        ++samples;
    }

    return samples > 0 ? total / samples : 0.0;
}

double VideoContentDetector::verticalBoundaryScore(const QImage& image, int x, int yStart, int yEnd, int step)
{
    if (x <= 0 || x >= image.width())
    {
        return 0.0;
    }

    double total = 0.0;
    int samples = 0;

    for (int y = yStart; y <= yEnd; y += step)
    {
        const auto* const row = reinterpret_cast<const QRgb*>(image.constScanLine(y));

        total += pixelDistance(row[x - 1], row[x]);
        ++samples;
    }

    return samples > 0 ? total / samples : 0.0;
}

int VideoContentDetector::strongestHorizontalBoundary(const QImage& image, int yStart, int yEnd, int xStart, int xEnd,
                                                      int step, double* bestScore)
{
    int bestY = -1;
    double best = 0.0;

    for (int y = yStart; y <= yEnd; ++y)
    {
        const double score = horizontalBoundaryScore(image, y, xStart, xEnd, step);

        if (score > best)
        {
            best = score;
            bestY = y;
        }
    }

    if (bestScore)
    {
        *bestScore = best;
    }

    return bestY;
}

int VideoContentDetector::strongestVerticalBoundary(const QImage& image, int xStart, int xEnd, int yStart, int yEnd,
                                                    int step, double* bestScore)
{
    int bestX = -1;
    double best = 0.0;

    for (int x = xStart; x <= xEnd; ++x)
    {
        const double score = verticalBoundaryScore(image, x, yStart, yEnd, step);

        if (score > best)
        {
            best = score;
            bestX = x;
        }
    }

    if (bestScore)
    {
        *bestScore = best;
    }

    return bestX;
}
