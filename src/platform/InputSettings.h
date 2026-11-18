#pragma once

#include <QObject>

class InputSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool touchDrawingEnabled READ touchDrawingEnabled WRITE setTouchDrawingEnabled NOTIFY touchDrawingEnabledChanged)

public:
    explicit InputSettings(QObject *parent = nullptr) : QObject(parent) {}

    bool touchDrawingEnabled() const { return touchDrawingEnabled_; }

public slots:
    void setTouchDrawingEnabled(bool enabled) {
        if (touchDrawingEnabled_ == enabled)
            return;
        touchDrawingEnabled_ = enabled;
        emit touchDrawingEnabledChanged();
    }

signals:
    void touchDrawingEnabledChanged();

private:
    bool touchDrawingEnabled_ = true;
};
