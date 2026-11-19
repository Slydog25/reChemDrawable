#pragma once

#include <QObject>

// Exposed to QML as `eink`. Lets the toolbar ask the platform layer for a full
// e-ink refresh; main.cpp connects the signal to the qtfb connection. Absent on
// the desktop and in the QML tests (QML guards with typeof).
class DisplayController : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE void requestCleanScreen() { emit cleanScreenRequested(); }
signals:
    void cleanScreenRequested();
};
