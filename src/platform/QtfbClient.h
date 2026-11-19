#pragma once

#include "platform/DisplayProtocol.h"

#include <QImage>
#include <QRect>

#include <cstddef>
#include <cstdint>

struct QtfbInputEvent {
    int type = 0;
    // For touch, AppLoad's id for the finger (Qt's touch point id), so several
    // fingers can be told apart. Always 0 for the pen.
    int devId = 0;
    int x = 0;
    int y = 0;
};

class QtfbClient {
public:
    QtfbClient() = default;
    ~QtfbClient();

    QtfbClient(const QtfbClient &) = delete;
    QtfbClient &operator=(const QtfbClient &) = delete;

    bool connect(int width, int height);
    // image is the picture for the whole framebuffer (already rotated for the
    // current orientation). Only the part that changed since the last call is
    // copied and announced (UPDATE_PARTIAL); the whole screen when most of it
    // changed or when force is set. Returns true if an update was sent.
    bool present(const QImage &image, bool force = false);
    // One update of everything sent since the last clean update, to run after
    // switching back from the fast waveform. Returns true if one was sent.
    bool presentClean();
    // Forget what is on the panel so the next present() sends the whole screen.
    void invalidate();
    // MESSAGE_SET_REFRESH_MODE / MESSAGE_REQUEST_FULL_REFRESH. Note AppLoad sleeps
    // a second after each, delaying whatever is queued behind it.
    void setRefreshMode(int mode);
    void requestFullRefresh();
    // Next input event (touch, pen, virtual keyboard). Device-state messages
    // are consumed on the way and update rotation().
    bool pollInput(QtfbInputEvent &event);
    // Orientation last reported by AppLoad (DEVICE_STATE_* messages).
    display::Rotation rotation() const { return rotation_; }
    bool isConnected() const;
    // True once AppLoad has closed the connection (the user closed the app).
    bool serverHungUp() const;

private:
    void close();
    bool sendMessage(const display::ClientMessageBytes &message);
    void copyRegion(const QImage &rgb, const QRect &rect);

    int socketFd_ = -1;
    std::uint8_t *sharedMemory_ = nullptr;
    std::size_t sharedMemorySize_ = 0;
    int width_ = 0;
    int height_ = 0;
    QImage lastFrame_;
    QRect dirtySinceClean_;
    display::Rotation rotation_ = display::Rotation::Deg0;
    bool flipRotation_ = false;
    bool serverHungUp_ = false;
};
