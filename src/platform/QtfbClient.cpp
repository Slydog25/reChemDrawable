#include "platform/QtfbClient.h"

#include <QByteArray>
#include <QDebug>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
constexpr std::uint8_t Initialize = 0;
constexpr std::uint8_t Update = 1;
// AppLoad's Paper Pro RGB565 framebuffer format.  Format 0 is the legacy
// reMarkable 2 framebuffer, with a different resolution.
constexpr std::uint8_t Rgb565 = 3;

void writeU32(std::uint8_t *destination, std::uint32_t value) {
    std::memcpy(destination, &value, sizeof(value));
}

std::uint32_t readU32(const std::uint8_t *source) {
    std::uint32_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

std::size_t readSize(const std::uint8_t *source) {
    std::size_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}
}

QtfbClient::~QtfbClient() {
    close();
}

bool QtfbClient::connect(int width, int height) {
    close();
    width_ = width;
    height_ = height;

    bool keyOk = false;
    const std::uint32_t requestedKey = qEnvironmentVariableIntValue("QTFB_KEY", &keyOk);
    if (!keyOk || requestedKey == 0) {
        qWarning() << "qtfb: QTFB_KEY is missing or invalid";
        return false;
    }

    socketFd_ = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (socketFd_ < 0) {
        qWarning() << "qtfb: cannot create socket";
        return false;
    }

    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, "/tmp/qtfb.sock", sizeof(address.sun_path) - 1);
    if (::connect(socketFd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        qWarning() << "qtfb: cannot connect to qtfb.sock";
        close();
        return false;
    }

    std::uint8_t request[24] {};
    request[0] = Initialize;
    writeU32(request + 4, requestedKey);
    request[8] = Rgb565;
    if (::write(socketFd_, request, sizeof(request)) != static_cast<ssize_t>(sizeof(request))) {
        qWarning() << "qtfb: initialization write failed";
        close();
        return false;
    }

    // ServerMessage is naturally aligned: its payload starts at byte 8 and
    // the size_t framebuffer length starts at byte 16 on the Paper Pro.
    std::uint8_t response[32] {};
    if (::read(socketFd_, response, sizeof(response)) != static_cast<ssize_t>(sizeof(response))) {
        qWarning() << "qtfb: initialization response failed";
        close();
        return false;
    }

    const std::int32_t sharedKey = static_cast<std::int32_t>(readU32(response + 8));
    sharedMemorySize_ = readSize(response + 16);
    // Some qtfb builds leave the size field unset.  The framebuffer layout is
    // still fixed by the dimensions requested during initialization.
    if (sharedMemorySize_ == 0)
        sharedMemorySize_ = static_cast<std::size_t>(width_) * height_ * 2;
    if (sharedMemorySize_ < static_cast<std::size_t>(width_) * height_ * 2) {
        qWarning() << "qtfb: framebuffer is smaller than the Paper Pro display";
        close();
        return false;
    }

    qWarning() << "qtfb: response shared key" << sharedKey
               << "size" << sharedMemorySize_;
    QByteArray path = QByteArray("/dev/shm/qtfb_") + QByteArray::number(sharedKey);
    int memoryFd = ::open(path.constData(), O_RDWR);
    // Recent AppLoad builds return zero or a stale shared-memory id here, while
    // the buffer is named after the session key given to the application.
    if (memoryFd < 0 && sharedKey != static_cast<std::int32_t>(requestedKey)) {
        path = QByteArray("/dev/shm/qtfb_") + QByteArray::number(requestedKey);
        memoryFd = ::open(path.constData(), O_RDWR);
    }
    if (memoryFd < 0) {
        qWarning() << "qtfb: cannot open shared framebuffer" << path;
        close();
        return false;
    }
    void *mapping = ::mmap(nullptr, sharedMemorySize_, PROT_READ | PROT_WRITE, MAP_SHARED, memoryFd, 0);
    ::close(memoryFd);
    if (mapping == MAP_FAILED) {
        qWarning() << "qtfb: cannot map shared framebuffer";
        close();
        return false;
    }
    sharedMemory_ = static_cast<std::uint8_t *>(mapping);
    const int flags = ::fcntl(socketFd_, F_GETFL, 0);
    if (flags >= 0)
        (void)::fcntl(socketFd_, F_SETFL, flags | O_NONBLOCK);
    return true;
}

void QtfbClient::present(const QImage &image) {
    if (!isConnected() || image.isNull())
        return;

    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    // The window is grabbed on a timer; only push frames that actually changed,
    // so an idle editor does not keep refreshing the e-ink panel.
    if (rgb == lastFrame_)
        return;
    lastFrame_ = rgb;
    std::memset(sharedMemory_, 0xff, sharedMemorySize_);
    const int copyWidth = std::min(width_, rgb.width());
    const int copyHeight = std::min(height_, rgb.height());
    for (int y = 0; y < copyHeight; ++y) {
        const auto *source = rgb.constScanLine(y);
        auto *destination = sharedMemory_ + static_cast<std::size_t>(y) * width_ * 2;
        for (int x = 0; x < copyWidth; ++x) {
            const auto *pixel = source + x * 3;
            const std::uint16_t rgb565 = static_cast<std::uint16_t>(
                ((pixel[0] >> 3) << 11) | ((pixel[1] >> 2) << 5) | (pixel[2] >> 3));
            destination[x * 2] = static_cast<std::uint8_t>(rgb565);
            destination[x * 2 + 1] = static_cast<std::uint8_t>(rgb565 >> 8);
        }
    }

    std::uint8_t update[24] {};
    update[0] = Update;
    writeU32(update + 8, 0);
    writeU32(update + 12, 0);
    writeU32(update + 16, width_);
    writeU32(update + 20, height_);
    (void)::write(socketFd_, update, sizeof(update));
}

bool QtfbClient::pollInput(QtfbInputEvent &event) {
    if (!isConnected())
        return false;

    // ServerMessage is 32 bytes on aarch64: the union begins at byte 8 and
    // UserInputContents places its type, device id, x, and y at 8, 12, 16,
    // and 20 respectively.
    std::uint8_t message[32] {};
    const ssize_t bytesRead = ::recv(socketFd_, message, sizeof(message), MSG_DONTWAIT);
    if (bytesRead <= 0 || message[0] != 4 || bytesRead < 24)
        return false;

    event.type = static_cast<int>(readU32(message + 8));
    event.x = static_cast<int>(readU32(message + 16));
    event.y = static_cast<int>(readU32(message + 20));
    return true;
}

bool QtfbClient::isConnected() const {
    return socketFd_ >= 0 && sharedMemory_ != nullptr;
}

void QtfbClient::close() {
    if (sharedMemory_) {
        ::munmap(sharedMemory_, sharedMemorySize_);
        sharedMemory_ = nullptr;
    }
    if (socketFd_ >= 0) {
        ::close(socketFd_);
        socketFd_ = -1;
    }
    sharedMemorySize_ = 0;
    lastFrame_ = QImage();
}
