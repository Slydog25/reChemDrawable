#pragma once

// AppLoad's qtfb wire protocol (the parts this app uses) and the pure helpers
// around it: message encoding, server-message decoding, virtual keyboard key
// translation and rotation mapping. No sockets here, so it is all unit tested
// (tests/test_display.cpp).
//
// Source of truth: https://github.com/asivery/rm-appload, src/qtfb/common.h and
// src/qtfb/fbmanagement.cpp. What we rely on, quoted from common.h:
//
//   struct ClientMessage {
//       uint8_t type;
//       union {
//           struct InitMessageContents init;
//           struct UpdateRegionMessageContents update;   // { int type; int x, y, w, h; }
//           struct CustomInitMessageContents customInit;
//           // Terminate does not send any data.
//           int refreshMode;
//           // Force full refresh does not send any data.
//       };
//   };
//   struct ServerMessage {
//       uint8_t type;
//       union {
//           struct InitMessageResponseContents init;     // { int shmKeyDefined; size_t shmSize; }
//           struct UserInputContents userInput;          // { int inputType; int devId; int x, y, d; }
//           struct DeviceStateChangedContents deviceStateChanged; // { int reason; union { struct { int rotation; } rotation; }; }
//       };
//   };
//
// On aarch64 the ClientMessage union holds only 4-byte-aligned members, so it
// starts at byte 4 and the whole message is 24 bytes (the server recv()s
// sizeof(ClientMessage) bytes per message). ServerMessage's union contains a
// size_t, so it starts at byte 8 and the message is 32 bytes.
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace display {

// Client -> server message types (MESSAGE_* in common.h).
constexpr std::uint8_t MessageInitialize = 0;
constexpr std::uint8_t MessageUpdate = 1;
constexpr std::uint8_t MessageTerminate = 3;
constexpr std::uint8_t MessageSetRefreshMode = 5;
constexpr std::uint8_t MessageRequestFullRefresh = 6;
// Server -> client message types.
constexpr std::uint8_t MessageUserInput = 4;
constexpr std::uint8_t MessageDeviceStateChanged = 7;
constexpr std::uint8_t MessageDeviceStateInit = 8;

constexpr int UpdateAll = 0;     // UPDATE_ALL
constexpr int UpdatePartial = 1; // UPDATE_PARTIAL

// REFRESH_MODE_* in common.h. fbmanagement.cpp rejects anything outside 0..4
// ("if(refresh > 4 || refresh < 0)") and then DISCONNECTS the client, so SLEEP
// (5) must never be sent.
enum RefreshMode { RefreshUFast = 0, RefreshFast = 1, RefreshAnimate = 2, RefreshContent = 3, RefreshUi = 4 };

// Input types (UserInputContents::inputType).
constexpr int InputVkbPress = 0x40;
constexpr int InputVkbRelease = 0x41;

constexpr int ClientMessageSize = 24;
constexpr int ServerMessageSize = 32;
using ClientMessageBytes = std::array<std::uint8_t, ClientMessageSize>;

// UPDATE_ALL: the whole framebuffer. UPDATE_PARTIAL: x, y, w, h in framebuffer
// pixels, handed to QRect(x, y, w, h) by the server without clamping.
ClientMessageBytes encodeUpdateAll(int width, int height);
ClientMessageBytes encodeUpdatePartial(const QRect &rect);
ClientMessageBytes encodeSetRefreshMode(int mode);
ClientMessageBytes encodeRequestFullRefresh();

// What a server message means to us.
struct ServerEvent {
    enum class Kind { Ignored, Input, Rotation } kind = Kind::Ignored;
    int inputType = 0, devId = 0, x = 0, y = 0, d = 0; // Kind::Input
    int rotation = 0;                                    // Kind::Rotation, a ROTATION_* value
};
ServerEvent decodeServerMessage(const std::uint8_t *data, std::size_t length);

// ---- Rotation -------------------------------------------------------------
// ROTATION_0..ROTATION_180 in common.h. AppLoad itself turns the picture
// (FBController::paint: Deg90L = painter->rotate(-90), i.e. the framebuffer is
// shown rotated counter-clockwise) and maps touches back into framebuffer pixels
// (convertPointToQTFBPixels), then tells the app the rotation. The framebuffer
// stays 1620x2160. To look upright in a landscape window the app draws its
// landscape UI turned the opposite way into that framebuffer.
enum class Rotation { Deg0 = 0, L90 = 1, R90 = 2, Deg180 = 3 };

// Wire value -> Rotation; out of range gives Deg0. flip swaps L90/R90, for the
// case that a device turns out to report them the other way round.
Rotation rotationFromWire(int value, bool flip = false);
bool isLandscape(Rotation rotation);
// The size of the app's own window for a framebuffer of fbSize.
QSize logicalSize(Rotation rotation, const QSize &fbSize);
// A touch/pen position in framebuffer pixels -> position in the app's window.
QPoint framebufferToLogical(Rotation rotation, const QPoint &point, const QSize &fbSize);
// The reverse.
QPoint logicalToFramebuffer(Rotation rotation, const QPoint &point, const QSize &fbSize);
// The app's rendered window -> the framebuffer picture to present.
QImage logicalToFramebuffer(Rotation rotation, const QImage &frame);

// ---- Virtual keyboard -------------------------------------------------------
// AppLoad's keyboard (resources/keyboard/default.layout.json) sends Qt::Key
// values in UserInputContents::x ("sendKeyEvent: .x = key"): letters as the
// upper-case Latin-1 code (65 = Key_A), punctuation as its Latin-1 code,
// special keys as 0x01000000-range codes. Shift, Ctrl and Alt arrive as keys of
// their own (press while held), not as flags on other keys.
struct KeyEventSpec {
    bool press = true;
    int key = 0;
    Qt::KeyboardModifiers modifiers;
    QString text;
};

class VkbTranslator {
public:
    // inputType is InputVkbPress or InputVkbRelease. Modifier keys only update
    // the state and give nothing back.
    std::optional<KeyEventSpec> translate(int inputType, int keyCode);
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    void reset() { m_modifiers = Qt::NoModifier; }

private:
    Qt::KeyboardModifiers m_modifiers;
};

} // namespace display
