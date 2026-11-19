#pragma once

#include <QImage>

#include <cstddef>
#include <cstdint>

struct QtfbInputEvent {
    int type = 0;
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
    void present(const QImage &image);
    bool pollInput(QtfbInputEvent &event);
    bool isConnected() const;
    // True once AppLoad has closed the connection (the user closed the app).
    bool serverHungUp() const;

private:
    void close();

    int socketFd_ = -1;
    std::uint8_t *sharedMemory_ = nullptr;
    std::size_t sharedMemorySize_ = 0;
    int width_ = 0;
    int height_ = 0;
    QImage lastFrame_;
    bool serverHungUp_ = false;
};
