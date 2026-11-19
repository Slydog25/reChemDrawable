#include "platform/InputRouter.h"

bool InputRouter::accept(Source source, Phase phase, const QPointF &position, const QRectF &penOnlyArea, bool dialogOpen,
                         int touchId) {
    if (source == Source::Pen) {
        if (phase == Phase::Press) m_penDown = true;
        if (phase == Phase::Release) m_penDown = false;
        return true;
    }

    switch (phase) {
    case Phase::Press: {
        m_touchesDown.insert(touchId);
        // Only a lone finger, with the pen up (palm rejection), outside the
        // canvas or on a dialog, reaches the app.
        const bool accepted = m_deliveredTouch < 0 && m_touchesDown.size() == 1 && !m_penDown &&
                              (dialogOpen || !penOnlyArea.contains(position));
        if (accepted) m_deliveredTouch = touchId;
        return accepted;
    }
    case Phase::Move:
        return touchId == m_deliveredTouch;
    case Phase::Release:
        m_touchesDown.remove(touchId);
        if (touchId != m_deliveredTouch) return false;
        m_deliveredTouch = -1;
        return true;
    }
    return false;
}

bool InputRouter::cancelTouch() {
    if (m_deliveredTouch < 0) return false;
    m_deliveredTouch = -1;
    return true;
}

bool InputRouter::penDown() const { return m_penDown; }
