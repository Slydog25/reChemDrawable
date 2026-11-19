#include "platform/QtfbClient.h"

#include "platform/DirtyRect.h"

#include <QByteArray>
#include <QDebug>

#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
constexpr std::uint8_t Initialize = 0;
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
    // If a device turns out to report landscape directions the other way round,
    // RECHEMDRAW_FLIP_ROTATION=1 swaps them (see README).
    flipRotation_ = qEnvironmentVariableIntValue("RECHEMDRAW_FLIP_ROTATION") != 0;

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
    for (int attempt = 0;; ++attempt) {
        if (::read(socketFd_, response, sizeof(response)) != static_cast<ssize_t>(sizeof(response))) {
            qWarning() << "qtfb: initialization response failed";
            close();
            return false;
        }
        if (response[0] == display::MessageInitialize) break;
        // A device-state message that overtook the response: keep its rotation.
        const display::ServerEvent early = display::decodeServerMessage(response, sizeof(response));
        if (early.kind == display::ServerEvent::Kind::Rotation)
            rotation_ = display::rotationFromWire(early.rotation, flipRotation_);
        if (attempt >= 3) {
            qWarning() << "qtfb: no initialization response";
            close();
            return false;
        }
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

bool QtfbClient::sendMessage(const display::ClientMessageBytes &message) {
    if (!isConnected()) return false;
    const ssize_t written = ::write(socketFd_, message.data(), message.size());
    if (written == static_cast<ssize_t>(message.size())) return true;
    // The socket is non-blocking; a full buffer (AppLoad is busy) drops the
    // message. present() reacts by resending the whole screen next time.
    qWarning() << "qtfb: message type" << message[0] << "not sent";
    return false;
}

// Copies the rectangle into the shared RGB565 framebuffer (little endian).
void QtfbClient::copyRegion(const QImage &rgb, const QRect &rect) {
    const QRect area = rect & QRect(0, 0, std::min(width_, rgb.width()), std::min(height_, rgb.height()));
    for (int y = area.top(); y <= area.bottom(); ++y) {
        const auto *source = rgb.constScanLine(y);
        auto *destination = sharedMemory_ + static_cast<std::size_t>(y) * width_ * 2;
        for (int x = area.left(); x <= area.right(); ++x) {
            const auto *pixel = source + x * 3;
            const std::uint16_t rgb565 = static_cast<std::uint16_t>(
                ((pixel[0] >> 3) << 11) | ((pixel[1] >> 2) << 5) | (pixel[2] >> 3));
            destination[x * 2] = static_cast<std::uint8_t>(rgb565);
            destination[x * 2 + 1] = static_cast<std::uint8_t>(rgb565 >> 8);
        }
    }
}

bool QtfbClient::present(const QImage &image, bool force) {
    if (!isConnected() || image.isNull())
        return false;

    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    // Only push what actually changed, so an idle editor does not keep
    // refreshing the e-ink panel and a pen stroke refreshes just its box.
    DirtyResult dirty = computeDirtyRect(lastFrame_, rgb, 16);
    if (force) {
        dirty.changed = dirty.full = true;
        dirty.rect = QRect(QPoint(0, 0), rgb.size());
    }
    if (!dirty.changed)
        return false;
    // Pixels outside the picture (if it is smaller than the framebuffer) are white.
    if (dirty.full) std::memset(sharedMemory_, 0xff, sharedMemorySize_);
    copyRegion(rgb, dirty.rect);
    lastFrame_ = rgb;

    const bool sent = dirty.full ? sendMessage(display::encodeUpdateAll(width_, height_))
                                 : sendMessage(display::encodeUpdatePartial(dirty.rect));
    if (!sent) {
        lastFrame_ = QImage(); // unsure what the panel shows: resend everything next time
        return false;
    }
    dirtySinceClean_ = dirty.full ? QRect(0, 0, width_, height_) : dirtySinceClean_.united(dirty.rect);
    return true;
}

bool QtfbClient::presentClean() {
    if (!isConnected() || dirtySinceClean_.isNull()) return false;
    const QRect area = dirtySinceClean_;
    dirtySinceClean_ = QRect();
    return area.width() >= width_ && area.height() >= height_
               ? sendMessage(display::encodeUpdateAll(width_, height_))
               : sendMessage(display::encodeUpdatePartial(area));
}

void QtfbClient::invalidate() { lastFrame_ = QImage(); }

void QtfbClient::setRefreshMode(int mode) { sendMessage(display::encodeSetRefreshMode(mode)); }

void QtfbClient::requestFullRefresh() { sendMessage(display::encodeRequestFullRefresh()); }

bool QtfbClient::pollInput(QtfbInputEvent &event) {
    if (!isConnected())
        return false;

    for (;;) {
        // ServerMessage is 32 bytes on aarch64 (see DisplayProtocol.h).
        std::uint8_t message[display::ServerMessageSize] {};
        const ssize_t bytesRead = ::recv(socketFd_, message, sizeof(message), MSG_DONTWAIT);
        // On a SEQPACKET socket, 0 means AppLoad closed its end: the app was closed.
        if (bytesRead == 0 || (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            serverHungUp_ = true;
            return false;
        }
        if (bytesRead < 0)
            return false;
        const display::ServerEvent decoded = display::decodeServerMessage(message, static_cast<std::size_t>(bytesRead));
        if (decoded.kind == display::ServerEvent::Kind::Rotation) {
            rotation_ = display::rotationFromWire(decoded.rotation, flipRotation_);
            continue;
        }
        if (decoded.kind != display::ServerEvent::Kind::Input)
            continue;
        event.type = decoded.inputType;
        event.devId = decoded.devId;
        event.x = decoded.x;
        event.y = decoded.y;
        // Virtual keyboard events carry the Qt key code in x.
        return true;
    }
}

bool QtfbClient::serverHungUp() const { return serverHungUp_; }

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
    dirtySinceClean_ = QRect();
}
