#include "platform/DisplayProtocol.h"

#include <QTransform>

#include <cstring>

namespace display {
namespace {
void put32(ClientMessageBytes &bytes, int offset, int value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
int get32(const std::uint8_t *data, int offset) {
    std::int32_t value = 0;
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}
}

ClientMessageBytes encodeUpdateAll(int width, int height) {
    ClientMessageBytes bytes {};
    bytes[0] = MessageUpdate;
    put32(bytes, 4, UpdateAll);
    put32(bytes, 8, 0);
    put32(bytes, 12, 0);
    put32(bytes, 16, width);
    put32(bytes, 20, height);
    return bytes;
}

ClientMessageBytes encodeUpdatePartial(const QRect &rect) {
    ClientMessageBytes bytes {};
    bytes[0] = MessageUpdate;
    put32(bytes, 4, UpdatePartial);
    put32(bytes, 8, rect.x());
    put32(bytes, 12, rect.y());
    put32(bytes, 16, rect.width());
    put32(bytes, 20, rect.height());
    return bytes;
}

ClientMessageBytes encodeSetRefreshMode(int mode) {
    ClientMessageBytes bytes {};
    bytes[0] = MessageSetRefreshMode;
    // Never send a value the server would answer by dropping us (see header).
    put32(bytes, 4, mode < RefreshUFast || mode > RefreshUi ? RefreshUi : mode);
    return bytes;
}

ClientMessageBytes encodeRequestFullRefresh() {
    ClientMessageBytes bytes {};
    bytes[0] = MessageRequestFullRefresh;
    return bytes;
}

ServerEvent decodeServerMessage(const std::uint8_t *data, std::size_t length) {
    ServerEvent event;
    if (!data || length < 12) return event;
    switch (data[0]) {
    case MessageUserInput:
        if (length < 24) return event;
        event.kind = ServerEvent::Kind::Input;
        event.inputType = get32(data, 8);
        event.devId = get32(data, 12);
        event.x = get32(data, 16);
        event.y = get32(data, 20);
        event.d = length >= 28 ? get32(data, 24) : 0;
        break;
    case MessageDeviceStateChanged:
    case MessageDeviceStateInit:
        // reason (STATE_CHANGED_REASON_ROTATION = 0) at 8, rotation at 12.
        if (length < 16 || get32(data, 8) != 0) return event;
        event.kind = ServerEvent::Kind::Rotation;
        event.rotation = get32(data, 12);
        break;
    default:
        break;
    }
    return event;
}

Rotation rotationFromWire(int value, bool flip) {
    switch (value) {
    case 1: return flip ? Rotation::R90 : Rotation::L90;
    case 2: return flip ? Rotation::L90 : Rotation::R90;
    case 3: return Rotation::Deg180;
    default: return Rotation::Deg0;
    }
}

bool isLandscape(Rotation rotation) { return rotation == Rotation::L90 || rotation == Rotation::R90; }

QSize logicalSize(Rotation rotation, const QSize &fbSize) {
    return isLandscape(rotation) ? QSize(fbSize.height(), fbSize.width()) : fbSize;
}

// L90: the framebuffer is shown turned counter-clockwise, so the window's
// (lx, ly) sits at framebuffer (W-1-ly, lx). R90 is the mirror image.
QPoint framebufferToLogical(Rotation rotation, const QPoint &p, const QSize &fb) {
    switch (rotation) {
    case Rotation::L90: return QPoint(p.y(), fb.width() - 1 - p.x());
    case Rotation::R90: return QPoint(fb.height() - 1 - p.y(), p.x());
    case Rotation::Deg180: return QPoint(fb.width() - 1 - p.x(), fb.height() - 1 - p.y());
    default: return p;
    }
}

QPoint logicalToFramebuffer(Rotation rotation, const QPoint &p, const QSize &fb) {
    switch (rotation) {
    case Rotation::L90: return QPoint(fb.width() - 1 - p.y(), p.x());
    case Rotation::R90: return QPoint(p.y(), fb.height() - 1 - p.x());
    case Rotation::Deg180: return QPoint(fb.width() - 1 - p.x(), fb.height() - 1 - p.y());
    default: return p;
    }
}

QImage logicalToFramebuffer(Rotation rotation, const QImage &frame) {
    switch (rotation) {
    case Rotation::L90: return frame.transformed(QTransform().rotate(90));   // clockwise
    case Rotation::R90: return frame.transformed(QTransform().rotate(-90));  // counter-clockwise
    case Rotation::Deg180: return frame.transformed(QTransform().rotate(180));
    default: return frame;
    }
}

std::optional<KeyEventSpec> VkbTranslator::translate(int inputType, int keyCode) {
    const bool press = inputType == InputVkbPress;
    if (!press && inputType != InputVkbRelease) return std::nullopt;

    Qt::KeyboardModifier modifier = Qt::NoModifier;
    switch (keyCode) {
    case Qt::Key_Shift: modifier = Qt::ShiftModifier; break;
    case Qt::Key_Control: modifier = Qt::ControlModifier; break;
    case Qt::Key_Alt: modifier = Qt::AltModifier; break;
    default: break;
    }
    if (modifier != Qt::NoModifier) {
        if (press) m_modifiers |= modifier;
        else m_modifiers &= ~Qt::KeyboardModifiers(modifier);
        return std::nullopt;
    }
    if (keyCode <= 0) return std::nullopt;

    KeyEventSpec spec;
    spec.press = press;
    spec.key = keyCode;
    spec.modifiers = m_modifiers;
    if (press && !(m_modifiers & (Qt::ControlModifier | Qt::AltModifier))) {
        if (keyCode >= 0x20 && keyCode <= 0x7e) {
            QChar c(static_cast<char16_t>(keyCode));
            if (c.isLetter()) c = (m_modifiers & Qt::ShiftModifier) ? c.toUpper() : c.toLower();
            spec.text = QString(c);
        } else if (keyCode == Qt::Key_Return || keyCode == Qt::Key_Enter) spec.text = QStringLiteral("\r");
        else if (keyCode == Qt::Key_Backspace) spec.text = QStringLiteral("\b");
        else if (keyCode == Qt::Key_Tab) spec.text = QStringLiteral("\t");
        else if (keyCode == Qt::Key_Escape) spec.text = QStringLiteral("\x1b");
        else if (keyCode == Qt::Key_Delete) spec.text = QStringLiteral("\x7f");
    }
    return spec;
}

} // namespace display
