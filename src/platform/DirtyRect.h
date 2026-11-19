#pragma once

#include <QImage>
#include <QRect>

// Which part of the screen changed between two frames, for sending the e-ink
// panel only that part.
struct DirtyResult {
    bool changed = false; // anything differs at all
    bool full = false;    // send the whole screen (first frame, size change, or most of it changed)
    QRect rect;           // padded, clamped bounding box of the change (the whole frame when full)
};

// previous and current should be in the same format (otherwise the result is
// "full"). padding grows the box on every side (e-ink waveforms bleed a little);
// when the padded box covers at least fullFraction of the frame, the whole frame
// is reported instead of a partial box.
DirtyResult computeDirtyRect(const QImage &previous, const QImage &current, int padding, double fullFraction = 0.6);
