#include "platform/PenTool.h"

#include <QByteArray>
#include <QDebug>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

bool PenTool::keyBitSet(const unsigned char *bits, int bitsLength, int key) {
    if (key < 0 || key / 8 >= bitsLength) return false;
    return bits[key / 8] & (1u << (key % 8));
}

PenTool::PenTool() {
#ifdef Q_OS_LINUX
    for (int index = 0; index < 32 && m_fd < 0; ++index) {
        const QByteArray path = "/dev/input/event" + QByteArray::number(index);
        const int fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        unsigned char capabilities[KEY_MAX / 8 + 1] = {};
        if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(capabilities)), capabilities) >= 0 &&
            keyBitSet(capabilities, sizeof(capabilities), BTN_TOOL_RUBBER) &&
            keyBitSet(capabilities, sizeof(capabilities), BTN_TOOL_PEN)) {
            m_fd = fd;
            qInfo() << "pen eraser: reading" << path;
        } else {
            ::close(fd);
        }
    }
#endif
}

PenTool::~PenTool() {
#ifdef Q_OS_LINUX
    if (m_fd >= 0) ::close(m_fd);
#endif
}

bool PenTool::available() const { return m_fd >= 0; }

bool PenTool::eraserActive() const {
#ifdef Q_OS_LINUX
    if (m_fd < 0) return false;
    unsigned char keys[KEY_MAX / 8 + 1] = {};
    if (::ioctl(m_fd, EVIOCGKEY(sizeof(keys)), keys) < 0) return false;
    return keyBitSet(keys, sizeof(keys), BTN_TOOL_RUBBER);
#else
    return false;
#endif
}
