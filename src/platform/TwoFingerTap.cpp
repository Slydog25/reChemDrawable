#include "platform/TwoFingerTap.h"

#include <QLineF>

int TwoFingerTap::feed(int fingerId, Phase phase, const QPointF &position, qint64 timeMs) {
    switch (phase) {
    case Phase::Press:
        if (m_starts.isEmpty()) {
            m_mostFingers = 0;
            m_firstPressMs = timeMs;
            m_failed = false;
        }
        m_starts.insert(fingerId, position);
        m_mostFingers = qMax(m_mostFingers, static_cast<int>(m_starts.size()));
        if (m_starts.size() > 3 || (m_starts.size() >= 2 && timeMs - m_firstPressMs > MaxPressGapMs)) m_failed = true;
        return 0;
    case Phase::Move:
        if (m_starts.contains(fingerId) && QLineF(m_starts.value(fingerId), position).length() > MaxMovement) m_failed = true;
        return 0;
    case Phase::Release:
        if (!m_starts.remove(fingerId) || !m_starts.isEmpty()) return 0;
        return !m_failed && (m_mostFingers == 2 || m_mostFingers == 3) && timeMs - m_firstPressMs <= MaxDurationMs ? m_mostFingers : 0;
    }
    return 0;
}

int TwoFingerTap::fingersDown() const { return static_cast<int>(m_starts.size()); }
