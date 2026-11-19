#include "platform/RefreshPolicy.h"

RefreshPolicy::Action RefreshPolicy::penEvent(qint64 nowMs, bool penDown) {
    Action action;
    m_penDown = penDown;
    m_lastPenMs = nowMs;
    if (m_state == State::Idle) {
        m_state = State::Interacting;
        m_presentedWhileInteracting = false;
        if (m_config.modeSwitching) action.setMode = m_config.interactiveMode;
    }
    return action;
}

RefreshPolicy::Action RefreshPolicy::tick(qint64 nowMs) {
    Action action;
    if (m_state != State::Interacting || m_penDown || nowMs - m_lastPenMs < m_config.settleDelayMs) return action;
    m_state = State::Idle;
    if (m_config.modeSwitching) action.setMode = m_config.restingMode;
    action.cleanUpdate = m_presentedWhileInteracting;
    m_presentedWhileInteracting = false;
    return action;
}
