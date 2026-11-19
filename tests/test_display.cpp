#include "platform/DirtyRect.h"
#include "platform/DisplayProtocol.h"
#include "platform/RefreshPolicy.h"

#include <QtTest>

#include <cstring>

using namespace display;

class DisplayTests : public QObject {
    Q_OBJECT

private:
    static QImage blank(int w = 100, int h = 200) {
        QImage image(w, h, QImage::Format_RGB888);
        image.fill(Qt::white);
        return image;
    }
    static int le32(const ClientMessageBytes &b, int offset) {
        int v;
        std::memcpy(&v, b.data() + offset, 4);
        return v;
    }

private slots:
    // ---- dirty rectangle -------------------------------------------------
    void noChange() {
        const DirtyResult r = computeDirtyRect(blank(), blank(), 4);
        QVERIFY(!r.changed);
        QVERIFY(!r.full);
    }

    void firstFrameIsFull() {
        const DirtyResult r = computeDirtyRect(QImage(), blank(), 4);
        QVERIFY(r.changed && r.full);
        QCOMPARE(r.rect, QRect(0, 0, 100, 200));
    }

    void sizeChangeIsFull() {
        const DirtyResult r = computeDirtyRect(blank(100, 200), blank(200, 100), 4);
        QVERIFY(r.full);
    }

    void smallChangeGivesPaddedBox() {
        QImage next = blank();
        next.setPixel(30, 50, qRgb(0, 0, 0));
        next.setPixel(40, 60, qRgb(0, 0, 0));
        const DirtyResult r = computeDirtyRect(blank(), next, 3);
        QVERIFY(r.changed);
        QVERIFY(!r.full);
        // Changed pixels span x 30..40, y 50..60; plus 3 on every side.
        QCOMPARE(r.rect, QRect(QPoint(27, 47), QPoint(43, 63)));
    }

    void paddingIsClampedToFrame() {
        QImage next = blank();
        next.setPixel(0, 0, qRgb(0, 0, 0));
        next.setPixel(99, 199, qRgb(0, 0, 0));
        QImage huge = blank(1000, 1000);
        QImage hugeNext = huge;
        hugeNext.setPixel(2, 3, qRgb(0, 0, 0));
        const DirtyResult r = computeDirtyRect(huge, hugeNext, 10);
        QVERIFY(!r.full);
        QCOMPARE(r.rect, QRect(QPoint(0, 0), QPoint(12, 13)));
        // The whole frame, found through the corners, is "full".
        QVERIFY(computeDirtyRect(blank(), next, 0).full);
    }

    void mostOfScreenChangedIsFull() {
        QImage next = blank();
        for (int y = 0; y < 150; ++y)
            for (int x = 0; x < 100; ++x) next.setPixel(x, y, qRgb(0, 0, 0));
        const DirtyResult r = computeDirtyRect(blank(), next, 0);
        QVERIFY(r.changed && r.full);
        QCOMPARE(r.rect, QRect(0, 0, 100, 200));
        // Half the frame stays partial.
        QImage half = blank();
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x) half.setPixel(x, y, qRgb(0, 0, 0));
        const DirtyResult p = computeDirtyRect(blank(), half, 0);
        QVERIFY(!p.full);
        QCOMPARE(p.rect, QRect(0, 0, 100, 100));
    }

    // ---- refresh mode state machine -----------------------------------------
    void fastDuringInteractionThenSettles() {
        RefreshPolicy policy;
        QCOMPARE(policy.state(), RefreshPolicy::State::Idle);
        QCOMPARE(policy.presentIntervalMs(), 250);

        RefreshPolicy::Action a = policy.penEvent(1000, true);
        QCOMPARE(a.setMode, int(RefreshFast));
        QCOMPARE(policy.state(), RefreshPolicy::State::Interacting);
        QCOMPARE(policy.presentIntervalMs(), 40);
        // Further pen events do not switch again.
        QCOMPARE(policy.penEvent(1020, true).setMode, -1);
        policy.framePresented();

        // Pen still down: never settles, however long.
        QCOMPARE(policy.tick(9000).setMode, -1);
        policy.penEvent(9000, false); // pen up
        // Not yet.
        a = policy.tick(9500);
        QCOMPARE(a.setMode, -1);
        QVERIFY(!a.cleanUpdate);
        QCOMPARE(policy.state(), RefreshPolicy::State::Interacting);
        // After the settle delay: back to the default waveform, one clean update.
        a = policy.tick(9000 + 1200);
        QCOMPARE(a.setMode, int(RefreshUi));
        QVERIFY(a.cleanUpdate);
        QCOMPARE(policy.state(), RefreshPolicy::State::Idle);
        QCOMPARE(policy.presentIntervalMs(), 250);
        // And only once.
        a = policy.tick(20000);
        QCOMPARE(a.setMode, -1);
        QVERIFY(!a.cleanUpdate);
    }

    void noCleanUpdateWhenNothingWasPresented() {
        RefreshPolicy policy;
        policy.penEvent(0, true);
        policy.penEvent(10, false);
        const RefreshPolicy::Action a = policy.tick(5000);
        QCOMPARE(a.setMode, int(RefreshUi));
        QVERIFY(!a.cleanUpdate);
    }

    void switchingCanBeDisabled() {
        RefreshPolicy::Config config;
        config.modeSwitching = false;
        RefreshPolicy policy(config);
        QCOMPARE(policy.penEvent(0, true).setMode, -1);
        policy.framePresented();
        policy.penEvent(10, false);
        const RefreshPolicy::Action a = policy.tick(5000);
        QCOMPARE(a.setMode, -1);
        QVERIFY(a.cleanUpdate); // still one clean update at the end
    }

    // ---- message encoding -----------------------------------------------------
    void encodesUpdateRegion() {
        const ClientMessageBytes m = encodeUpdatePartial(QRect(10, 20, 300, 400));
        QCOMPARE(int(m.size()), 24);
        QCOMPARE(int(m[0]), 1);             // MESSAGE_UPDATE
        QCOMPARE(le32(m, 4), 1);            // UPDATE_PARTIAL
        QCOMPARE(le32(m, 8), 10);
        QCOMPARE(le32(m, 12), 20);
        QCOMPARE(le32(m, 16), 300);
        QCOMPARE(le32(m, 20), 400);
        const ClientMessageBytes all = encodeUpdateAll(1620, 2160);
        QCOMPARE(int(all[0]), 1);
        QCOMPARE(le32(all, 4), 0);          // UPDATE_ALL
        QCOMPARE(le32(all, 16), 1620);
        QCOMPARE(le32(all, 20), 2160);
    }

    void encodesRefreshModeAndFullRefresh() {
        const ClientMessageBytes m = encodeSetRefreshMode(RefreshFast);
        QCOMPARE(int(m[0]), 5);
        QCOMPARE(le32(m, 4), 1);
        // 5 (SLEEP) would make AppLoad drop the connection: never sent.
        QCOMPARE(le32(encodeSetRefreshMode(5), 4), int(RefreshUi));
        QCOMPARE(le32(encodeSetRefreshMode(-1), 4), int(RefreshUi));
        const ClientMessageBytes f = encodeRequestFullRefresh();
        QCOMPARE(int(f[0]), 6);
        for (int i = 1; i < 24; ++i) QCOMPARE(int(f[i]), 0);
    }

    void decodesServerMessages() {
        std::uint8_t input[32] {};
        input[0] = 4;
        const int values[5] = {0x22, 0, 123, 456, 7};
        std::memcpy(input + 8, values, sizeof(values));
        ServerEvent e = decodeServerMessage(input, 32);
        QCOMPARE(int(e.kind), int(ServerEvent::Kind::Input));
        QCOMPARE(e.inputType, 0x22);
        QCOMPARE(e.x, 123);
        QCOMPARE(e.y, 456);
        QCOMPARE(e.d, 7);

        std::uint8_t state[32] {};
        state[0] = 7;
        const int rot[2] = {0, 2};
        std::memcpy(state + 8, rot, sizeof(rot));
        e = decodeServerMessage(state, 32);
        QCOMPARE(int(e.kind), int(ServerEvent::Kind::Rotation));
        QCOMPARE(e.rotation, 2);
        state[0] = 8; // DEVICE_STATE_INIT carries the same payload
        QCOMPARE(int(decodeServerMessage(state, 32).kind), int(ServerEvent::Kind::Rotation));

        std::uint8_t other[32] {};
        other[0] = 0;
        QCOMPARE(int(decodeServerMessage(other, 32).kind), int(ServerEvent::Kind::Ignored));
        QCOMPARE(int(decodeServerMessage(input, 8).kind), int(ServerEvent::Kind::Ignored));
    }

    // ---- virtual keyboard -------------------------------------------------------
    void keyboardLetters() {
        VkbTranslator t;
        auto press = t.translate(InputVkbPress, 65); // Key_A
        QVERIFY(press);
        QVERIFY(press->press);
        QCOMPARE(press->key, int(Qt::Key_A));
        QCOMPARE(press->text, QStringLiteral("a"));
        auto release = t.translate(InputVkbRelease, 65);
        QVERIFY(release && !release->press);
        QVERIFY(release->text.isEmpty());
    }

    void keyboardShiftHeld() {
        VkbTranslator t;
        QVERIFY(!t.translate(InputVkbPress, Qt::Key_Shift)); // modifier: state only
        QVERIFY(t.modifiers() & Qt::ShiftModifier);
        auto press = t.translate(InputVkbPress, 67);
        QCOMPARE(press->text, QStringLiteral("C"));
        QVERIFY(press->modifiers & Qt::ShiftModifier);
        QVERIFY(!t.translate(InputVkbRelease, Qt::Key_Shift));
        QVERIFY(!(t.modifiers() & Qt::ShiftModifier));
        QCOMPARE(t.translate(InputVkbPress, 67)->text, QStringLiteral("c"));
    }

    void keyboardPunctuationAndSpecialKeys() {
        VkbTranslator t;
        QCOMPARE(t.translate(InputVkbPress, 49)->text, QStringLiteral("1"));
        QCOMPARE(t.translate(InputVkbPress, 62)->text, QStringLiteral(">"));
        QCOMPARE(t.translate(InputVkbPress, 32)->text, QStringLiteral(" "));
        const auto back = t.translate(InputVkbPress, 16777219);
        QCOMPARE(back->key, int(Qt::Key_Backspace));
        QCOMPARE(back->text, QStringLiteral("\b"));
        QCOMPARE(t.translate(InputVkbPress, 16777220)->key, int(Qt::Key_Return));
        QCOMPARE(t.translate(InputVkbPress, 16777234)->key, int(Qt::Key_Left));
        QVERIFY(t.translate(InputVkbPress, 16777234)->text.isEmpty());
        QVERIFY(!t.translate(0x10, 65)); // not a keyboard event
        QVERIFY(!t.translate(InputVkbPress, 0));
    }

    void keyboardCtrlSuppressesText() {
        VkbTranslator t;
        t.translate(InputVkbPress, Qt::Key_Control);
        const auto press = t.translate(InputVkbPress, 65);
        QVERIFY(press->text.isEmpty());
        QVERIFY(press->modifiers & Qt::ControlModifier);
    }

    // ---- rotation -----------------------------------------------------------------
    void rotationWireValues() {
        QCOMPARE(int(rotationFromWire(0)), int(Rotation::Deg0));
        QCOMPARE(int(rotationFromWire(1)), int(Rotation::L90));
        QCOMPARE(int(rotationFromWire(2)), int(Rotation::R90));
        QCOMPARE(int(rotationFromWire(3)), int(Rotation::Deg180));
        QCOMPARE(int(rotationFromWire(9)), int(Rotation::Deg0));
        QCOMPARE(int(rotationFromWire(1, true)), int(Rotation::R90));
        QCOMPARE(int(rotationFromWire(2, true)), int(Rotation::L90));
    }

    void rotationSizes() {
        const QSize fb(1620, 2160);
        QCOMPARE(logicalSize(Rotation::Deg0, fb), QSize(1620, 2160));
        QCOMPARE(logicalSize(Rotation::Deg180, fb), QSize(1620, 2160));
        QCOMPARE(logicalSize(Rotation::L90, fb), QSize(2160, 1620));
        QCOMPARE(logicalSize(Rotation::R90, fb), QSize(2160, 1620));
    }

    void rotationPointMappingRoundTrips() {
        const QSize fb(1620, 2160);
        const Rotation all[] = {Rotation::Deg0, Rotation::L90, Rotation::R90, Rotation::Deg180};
        for (Rotation r : all) {
            const QSize logical = logicalSize(r, fb);
            for (const QPoint &p : {QPoint(0, 0), QPoint(5, 700), QPoint(1619, 2159), QPoint(800, 1)}) {
                const QPoint l = framebufferToLogical(r, p, fb);
                QVERIFY2(QRect(QPoint(0, 0), logical).contains(l), "maps inside the window");
                QCOMPARE(logicalToFramebuffer(r, l, fb), p);
            }
        }
        // L90: the top-left of the landscape window is the framebuffer's bottom-left.
        QCOMPARE(logicalToFramebuffer(Rotation::L90, QPoint(0, 0), fb), QPoint(1619, 0));
        QCOMPARE(framebufferToLogical(Rotation::L90, QPoint(1619, 0), fb), QPoint(0, 0));
        // R90: the top-left of the landscape window is the framebuffer's top-right.
        QCOMPARE(logicalToFramebuffer(Rotation::R90, QPoint(0, 0), fb), QPoint(0, 2159));
    }

    void rotationImageMatchesPointMapping() {
        const QSize fb(1620, 2160);
        const Rotation all[] = {Rotation::L90, Rotation::R90, Rotation::Deg180, Rotation::Deg0};
        for (Rotation r : all) {
            QImage window(logicalSize(r, fb), QImage::Format_RGB888);
            window.fill(Qt::white);
            const QPoint marked(37, 11);
            window.setPixel(marked, qRgb(255, 0, 0));
            const QImage out = logicalToFramebuffer(r, window);
            QCOMPARE(out.size(), fb);
            const QPoint at = logicalToFramebuffer(r, marked, fb);
            QCOMPARE(out.pixel(at), qRgb(255, 0, 0));
        }
    }
};

QTEST_MAIN(DisplayTests)
#include "test_display.moc"
