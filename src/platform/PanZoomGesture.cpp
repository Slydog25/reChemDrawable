#include "platform/PanZoomGesture.h"

#include <QLineF>

PanZoomGesture::Step PanZoomGesture::feed(int fingerId, Phase phase, const QPointF &position, const QRectF &canvas,
                                          bool penDown) {
    Step step;
    switch (phase) {
    case Phase::Press:
        if (m_start.isEmpty()) { m_engaged = false; m_spoiled = false; }
        if (penDown || !canvas.contains(position)) return step;
        m_start.insert(fingerId, position);
        m_last.insert(fingerId, position);
        if (m_start.size() > 2) m_spoiled = true;
        return step;
    case Phase::Release:
        m_start.remove(fingerId);
        m_last.remove(fingerId);
        return step;
    case Phase::Move:
        break;
    }
    if (!m_last.contains(fingerId)) return step;
    QHash<int, QPointF> before = m_last;
    m_last[fingerId] = position;
    if (m_spoiled || penDown || m_last.size() != 2) return step;

    if (!m_engaged) {
        for (auto it = m_last.cbegin(); it != m_last.cend(); ++it)
            if (QLineF(m_start.value(it.key()), it.value()).length() > EngageDistance) m_engaged = true;
        if (!m_engaged) return step;
        before = m_start; // the first step covers all movement since the fingers landed
    }

    QList<int> keys = m_last.keys();
    const QPointF oldA = before.value(keys[0]), oldB = before.value(keys[1]);
    const QPointF newA = m_last.value(keys[0]), newB = m_last.value(keys[1]);
    const QPointF oldCenter = (oldA + oldB) / 2, newCenter = (newA + newB) / 2;
    const double oldDistance = QLineF(oldA, oldB).length(), newDistance = QLineF(newA, newB).length();
    step.valid = true;
    step.center = oldCenter;
    step.dx = newCenter.x() - oldCenter.x();
    step.dy = newCenter.y() - oldCenter.y();
    step.scale = oldDistance > 1.0 && newDistance > 1.0 ? newDistance / oldDistance : 1.0;
    return step;
}
