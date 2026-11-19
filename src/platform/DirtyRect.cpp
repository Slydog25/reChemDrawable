#include "platform/DirtyRect.h"

#include <algorithm>
#include <cstring>

DirtyResult computeDirtyRect(const QImage &previous, const QImage &current, int padding, double fullFraction) {
    DirtyResult result;
    if (current.isNull()) return result;
    const QRect whole(QPoint(0, 0), current.size());
    if (previous.isNull() || previous.size() != current.size() || previous.format() != current.format()) {
        result.changed = result.full = true;
        result.rect = whole;
        return result;
    }

    const int pixelBytes = std::max(1, current.depth() / 8);
    const std::size_t rowBytes = static_cast<std::size_t>(current.width()) * pixelBytes;
    int top = -1, bottom = -1;
    std::size_t leftByte = rowBytes, rightByte = 0; // [leftByte, rightByte) of differing bytes
    for (int y = 0; y < current.height(); ++y) {
        const uchar *a = previous.constScanLine(y);
        const uchar *b = current.constScanLine(y);
        if (std::memcmp(a, b, rowBytes) == 0) continue;
        if (top < 0) top = y;
        bottom = y;
        const std::size_t first = static_cast<std::size_t>(std::mismatch(a, a + rowBytes, b).first - a);
        std::size_t last = rowBytes;
        while (last > 0 && a[last - 1] == b[last - 1]) --last;
        leftByte = std::min(leftByte, first);
        rightByte = std::max(rightByte, last);
    }
    if (top < 0) return result; // identical

    result.changed = true;
    const int left = static_cast<int>(leftByte / pixelBytes);
    const int right = static_cast<int>((rightByte + pixelBytes - 1) / pixelBytes); // exclusive
    QRect box(left, top, right - left, bottom - top + 1);
    box.adjust(-padding, -padding, padding, padding);
    box &= whole;
    const double area = static_cast<double>(box.width()) * box.height();
    const double total = static_cast<double>(whole.width()) * whole.height();
    if (area >= fullFraction * total) {
        result.full = true;
        result.rect = whole;
    } else {
        result.rect = box;
    }
    return result;
}
