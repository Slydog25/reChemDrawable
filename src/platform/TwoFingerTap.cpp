#include "platform/TwoFingerTap.h"

#include <QLineF>

bool TwoFingerTap::feed(int fingerId, Phase phase, const QPointF &position, qint64 timeMs) {
    switch (phase) {
    case Phase::Press:
        if (m_starts.isEmpty()) {
            m_mostFingers = 0;
            m_firstPressMs = timeMs;
            m_failed = false;
        }
        m_starts.insert(fingerId, position);
        m_mostFingers = qMax(m_mostFingers, static_cast<int>(m_starts.size()));
        if (m_starts.size() > 2 || (m_starts.size() == 2 && timeMs - m_firstPressMs > MaxPressGapMs)) m_failed = true;
        return false;
    case Phase::Move:
        if (m_starts.contains(fingerId) && QLineF(m_starts.value(fingerId), position).length() > MaxMovement) m_failed = true;
        return false;
    case Phase::Release:
        if (!m_starts.remove(fingerId) || !m_starts.isEmpty()) return false;
        return !m_failed && m_mostFingers == 2 && timeMs - m_firstPressMs <= MaxDurationMs;
    }
    return false;
}

int TwoFingerTap::fingersDown() const { return static_cast<int>(m_starts.size()); }
