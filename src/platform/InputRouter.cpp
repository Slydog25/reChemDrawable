#include "platform/InputRouter.h"

bool InputRouter::accept(Source source, Phase phase, const QPointF &position, const QRectF &penOnlyArea, bool dialogOpen) {
    if (source == Source::Pen) {
        if (phase == Phase::Press) m_penDown = true;
        if (phase == Phase::Release) m_penDown = false;
        return true;
    }

    switch (phase) {
    case Phase::Press:
        m_touchDown = true;
        // Ignore the hand entirely while the pen is down (palm rejection).
        m_touchAccepted = !m_penDown && (dialogOpen || !penOnlyArea.contains(position));
        return m_touchAccepted;
    case Phase::Move:
        return m_touchDown && m_touchAccepted;
    case Phase::Release: {
        const bool accepted = m_touchDown && m_touchAccepted;
        m_touchDown = false;
        m_touchAccepted = false;
        return accepted;
    }
    }
    return false;
}
