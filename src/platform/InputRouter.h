#pragma once

#include <QPointF>
#include <QRectF>
#include <QSet>

// Decides which tablet input events reach the app. The pen draws and selects
// everywhere. A finger only operates the menus around the canvas (and dialogs
// shown over it), so a resting hand cannot draw. Whether a finger touch is
// accepted is decided when it lands and holds for that whole touch. Only one
// finger at a time drives the app; extra fingers are gestures (two-finger tap).
class InputRouter {
public:
    enum class Source { Pen, Touch };
    enum class Phase { Press, Move, Release };

    // penOnlyArea: the drawing canvas in window coordinates.
    // dialogOpen: a dialog covers the canvas, so a finger may use it.
    // touchId tells fingers apart; it is ignored for the pen.
    bool accept(Source source, Phase phase, const QPointF &position, const QRectF &penOnlyArea, bool dialogOpen,
                int touchId = 0);

    // Stops delivering the finger currently driving the app, e.g. because a
    // second finger turned the touch into a gesture. Returns true if one was
    // being delivered, so the caller can cancel the press it started.
    bool cancelTouch();

    bool penDown() const;

private:
    bool m_penDown = false;
    QSet<int> m_touchesDown;
    int m_deliveredTouch = -1;
};
