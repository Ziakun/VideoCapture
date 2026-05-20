#pragma once

#include <QImage>
#include <QRect>

// Detects the dominant video-content rectangle inside a preview frame.
//
// The detector samples edge contrast instead of scanning every pixel. It is
// used only for browser auto-crop attempts and returns an empty QRect when the
// frame does not expose a confident large content region.
class VideoContentDetector
{
  public:
    // Finds a likely video area in frame coordinates; returns an invalid QRect
    // when confidence or size thresholds are not met.
    QRect detectLargeContentRect(const QImage& frame) const;

  private:
    static int pixelDistance(QRgb left, QRgb right);
    static double horizontalBoundaryScore(const QImage& image, int y, int xStart, int xEnd, int step);
    static double verticalBoundaryScore(const QImage& image, int x, int yStart, int yEnd, int step);
    static int strongestHorizontalBoundary(const QImage& image, int yStart, int yEnd, int xStart, int xEnd, int step,
                                           double* bestScore);
    static int strongestVerticalBoundary(const QImage& image, int xStart, int xEnd, int yStart, int yEnd, int step,
                                         double* bestScore);
};
