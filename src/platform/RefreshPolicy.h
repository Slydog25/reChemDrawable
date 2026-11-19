#pragma once

#include <QtGlobal>

// Decides how the e-ink panel is driven, from pen activity. Time is passed in
// (milliseconds), so the state machine is unit tested without a clock.
//
//   Idle --pen event--> Interacting   : switch to the fast waveform, present every ~40 ms
//   Interacting --no pen event for settleDelayMs and pen up--> Idle
//                                     : switch back to the default waveform and
//                                       send ONE clean update of what was drawn
//
// Why only the pen, and why a generous settle delay: AppLoad's server sleeps one
// second after each SET_REFRESH_MODE (and REQUEST_FULL_REFRESH) message before it
// reads the next one (fbmanagement.cpp, safetyWaitForEventLoopToCatchUp), so a
// mode switch delays everything queued behind it. Finger taps on the menus are
// therefore left on the default waveform, and a run of short pen strokes stays in
// the fast mode instead of switching back and forth.
class RefreshPolicy {
public:
    struct Config {
        int interactiveMode = 1;     // RefreshFast
        int restingMode = 4;         // RefreshUi (AppLoad's default)
        int activePresentMs = 40;
        int idlePresentMs = 250;
        int settleDelayMs = 1200;
        bool modeSwitching = true;
    };
    struct Action {
        int setMode = -1;         // send SET_REFRESH_MODE with this value when >= 0
        bool cleanUpdate = false; // then send one update of everything drawn while interacting
    };
    enum class State { Idle, Interacting };

    RefreshPolicy() = default;
    explicit RefreshPolicy(const Config &config) : m_config(config) {}

    // A pen event happened. Returns the mode switch to send, if any.
    Action penEvent(qint64 nowMs, bool penDown);
    // A frame was actually sent to the panel.
    void framePresented() { m_presentedWhileInteracting = true; }
    // Called on every present tick; may end the interaction.
    Action tick(qint64 nowMs);

    State state() const { return m_state; }
    int presentIntervalMs() const { return m_state == State::Interacting ? m_config.activePresentMs : m_config.idlePresentMs; }
    const Config &config() const { return m_config; }

private:
    Config m_config;
    State m_state = State::Idle;
    bool m_penDown = false;
    bool m_presentedWhileInteracting = false;
    qint64 m_lastPenMs = 0;
};
