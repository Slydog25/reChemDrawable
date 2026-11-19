#pragma once

#include <QHash>
#include <QPointF>
#include <QRectF>

// Turns two fingers moving on the canvas (pen up) into a pan and a zoom. Feed
// every finger event in scene coordinates; while exactly two fingers that
// landed on the canvas are down, a move returns the step since the last event.
// Small movements are ignored until the fingers have clearly moved, so a
// two-finger tap is not mistaken for a pan.
class PanZoomGesture {
public:
    enum class Phase { Press, Move, Release };

    struct Step {
        bool valid = false;
        double dx = 0, dy = 0;   // centroid movement
        double scale = 1.0;      // finger distance ratio since the previous step
        QPointF center;          // centroid before the step (zoom about this point, then pan by dx, dy)
    };

    static constexpr double EngageDistance = 12.0;

    Step feed(int fingerId, Phase phase, const QPointF &position, const QRectF &canvas, bool penDown);
    // True once this multi-finger touch has been a pan/zoom; stays true until
    // the next touch begins, so the tap recogniser can be told to ignore it.
    bool engaged() const { return m_engaged; }

private:
    QHash<int, QPointF> m_start, m_last;
    bool m_engaged = false;
    bool m_spoiled = false; // a third finger, or the pen, joined
};
