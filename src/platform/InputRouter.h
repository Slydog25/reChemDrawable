#pragma once

#include <QPointF>
#include <QRectF>

// Decides which tablet input events reach the app. The pen draws and selects
// everywhere. A finger only operates the menus around the canvas (and dialogs
// shown over it), so a resting hand cannot draw. Whether a finger touch is
// accepted is decided when it lands and holds for that whole touch.
class InputRouter {
public:
    enum class Source { Pen, Touch };
    enum class Phase { Press, Move, Release };

    // penOnlyArea: the drawing canvas in window coordinates.
    // dialogOpen: a dialog covers the canvas, so a finger may use it.
    bool accept(Source source, Phase phase, const QPointF &position, const QRectF &penOnlyArea, bool dialogOpen);

private:
    bool m_penDown = false;
    bool m_touchDown = false;
    bool m_touchAccepted = false;
};
