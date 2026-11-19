#pragma once

#include <QHash>
#include <QPointF>
#include <QtGlobal>

// Recognises a quick two-finger tap (used for undo on the tablet): two fingers
// land within a quarter second of each other, neither moves far, no third
// finger joins, and both are lifted within half a second of the first touch.
class TwoFingerTap {
public:
    // Same order as InputRouter::Phase, so main.cpp can convert between them.
    enum class Phase { Press, Move, Release };

    static constexpr qint64 MaxPressGapMs = 250;
    static constexpr qint64 MaxDurationMs = 500;
    static constexpr double MaxMovement = 40.0;

    // Feed every finger event. Returns true for the release that completes a
    // two-finger tap.
    bool feed(int fingerId, Phase phase, const QPointF &position, qint64 timeMs);
    int fingersDown() const;

private:
    QHash<int, QPointF> m_starts;
    int m_mostFingers = 0;
    qint64 m_firstPressMs = 0;
    bool m_failed = false;
};
