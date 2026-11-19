#include "core/Document.h"
#include "core/EditHistory.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "platform/DisplayController.h"
#include "platform/DisplayProtocol.h"
#include "platform/InputRouter.h"
#include "platform/PanZoomGesture.h"
#include "platform/PenTool.h"
#include "platform/QmlJitWorkaround.h"
#include "platform/QtfbClient.h"
#include "platform/QuitSignals.h"
#include "platform/RefreshPolicy.h"
#include "platform/TwoFingerTap.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QLibraryInfo>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

#include <memory>

namespace {
constexpr int PaperProWidth = 1620;
constexpr int PaperProHeight = 2160;
}

int main(int argc, char *argv[]) {
    avoidQmlJitOnPointerAuthCpus();
    QGuiApplication app(argc, argv);
    Molecule molecule;
    ReactionLayer reactionLayer(&molecule);
    EditHistory editHistory(&molecule, &reactionLayer);
    Document document(&molecule, &reactionLayer);
    DisplayController eink; // `eink` in QML: the toolbar's "Clean screen" button
    // Declared after the objects it exposes, so it is destroyed first and its
    // bindings never see them half torn down at exit.
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("molecule"), &molecule);
    engine.rootContext()->setContextProperty(QStringLiteral("reactionLayer"), &reactionLayer);
    engine.rootContext()->setContextProperty(QStringLiteral("editHistory"), &editHistory);
    engine.rootContext()->setContextProperty(QStringLiteral("document"), &document);
    engine.rootContext()->setContextProperty(QStringLiteral("eink"), &eink);
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
#ifdef RECHEMDRAW_EXTERNAL_QML
    engine.load(QUrl::fromLocalFile(QCoreApplication::applicationDirPath()
                                   + QStringLiteral("/qml/Main.qml")));
#else
    engine.load(QUrl(QStringLiteral("qrc:/src/qml/Main.qml")));
#endif
    if (engine.rootObjects().isEmpty()) return 1;

    // Closing the app from AppLoad (a signal, or the display connection going
    // away) can't show the unsaved-changes dialog, so keep a recovery copy.
    installQuitSignalHandlers();
    QTimer quitPoll;
    // exit(), not quit(): in Qt 6 quit() first asks the windows to close, and the
    // main window refuses while there are unsaved changes (to show its dialog,
    // which AppLoad has hidden), so the app would never stop.
    QObject::connect(&quitPoll, &QTimer::timeout, &app, [] {
        if (quitSignalReceived()) QCoreApplication::exit(0);
    });
    quitPoll.start(100);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &document, [&document] {
        // A drawing with a file and a pending autosave just gets saved; only
        // untitled work (or a failed save) needs a separate recovery copy.
        document.flushAutosave();
        const QString recovered = document.saveRecoveryCopy();
        if (!recovered.isEmpty()) qWarning() << "Saved unsaved changes to" << recovered;
    });

    QtfbClient qtfb;
    // E-ink state (only used when QTFB_KEY is set). AppLoad's framebuffer is
    // always portrait; `rotation` is how AppLoad currently shows it, and the
    // window is 2160x1620 while landscape (see DisplayProtocol.h).
    const QSize framebufferSize(PaperProWidth, PaperProHeight);
    display::Rotation rotation = display::Rotation::Deg0;
    QSize logicalSize = framebufferSize;
    RefreshPolicy::Config refreshConfig;
    // RECHEMDRAW_REFRESH_SWITCH=0 keeps AppLoad's default waveform throughout.
    if (qEnvironmentVariableIsSet("RECHEMDRAW_REFRESH_SWITCH"))
        refreshConfig.modeSwitching = qEnvironmentVariableIntValue("RECHEMDRAW_REFRESH_SWITCH") != 0;
    RefreshPolicy refreshPolicy(refreshConfig);
    display::VkbTranslator keyboard;
    QElapsedTimer displayClock;
    displayClock.start();
    bool forceFullPresent = true;
    std::unique_ptr<PenTool> penTool;
    QTimer qtfbRefresh;
    QTimer qtfbInput;
    if (qEnvironmentVariableIsSet("QTFB_KEY")) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (!window) {
            qWarning() << "qtfb: root object is not a QQuickWindow";
            return 1;
        }
        if (!qtfb.connect(PaperProWidth, PaperProHeight)) {
            qWarning() << "qtfb: framebuffer connection failed";
            return 1;
        }
        window->resize(PaperProWidth, PaperProHeight);
        // Follow AppLoad's rotation: resize the window and redraw everything.
        auto followRotation = [&qtfb, &rotation, &logicalSize, &forceFullPresent, framebufferSize, window] {
            if (qtfb.rotation() == rotation) return;
            rotation = qtfb.rotation();
            logicalSize = display::logicalSize(rotation, framebufferSize);
            window->resize(logicalSize);
            qtfb.invalidate();
            forceFullPresent = true;
        };
        // "Clean screen": AppLoad's full refresh, then resend the whole picture.
        QObject::connect(&eink, &DisplayController::cleanScreenRequested, &app, [&qtfb, &forceFullPresent] {
            qtfb.requestFullRefresh();
            forceFullPresent = true;
        });
        // Presents the window. Runs every 40 ms while the pen is busy and every
        // 250 ms otherwise; the next run is scheduled after this one finishes,
        // so a slow grab just lowers the rate.
        qtfbRefresh.setSingleShot(true);
        QObject::connect(&qtfbRefresh, &QTimer::timeout, &app, [&, window, followRotation] {
            followRotation();
            const RefreshPolicy::Action action = refreshPolicy.tick(displayClock.elapsed());
            if (action.setMode >= 0) qtfb.setRefreshMode(action.setMode);
            const bool force = forceFullPresent;
            forceFullPresent = false;
            if (qtfb.present(display::logicalToFramebuffer(rotation, window->grabWindow()), force))
                refreshPolicy.framePresented();
            // After the fast waveform: one clean update of what was drawn with it.
            if (action.cleanUpdate && qtfb.presentClean()) refreshPolicy.framePresented();
            qtfbRefresh.start(refreshPolicy.presentIntervalMs());
        });
        QPointer<QQuickItem> canvas = window->findChild<QQuickItem *>(QStringLiteral("moleculeCanvas"));
        if (!canvas) qWarning() << "qtfb: canvas not found; finger input will reach it";
        penTool = std::make_unique<PenTool>();
        if (!penTool->available()) qWarning() << "pen eraser: no pen input device found; the eraser end will draw";
        QObject::connect(&qtfbInput, &QTimer::timeout, &app, [&qtfb, &editHistory, &rotation, &logicalSize, &refreshPolicy, &keyboard, &displayClock, followRotation, framebufferSize, window, canvas, pen = penTool.get()] {
            static Qt::MouseButtons pressedButtons;
            static InputRouter router;
            static TwoFingerTap twoFingerTap;
            static PanZoomGesture panZoom;
            static QElapsedTimer clock;
            if (!clock.isValid()) clock.start();
            static bool eraserStroke = false;
            QtfbInputEvent input;
            if (qtfb.serverHungUp()) {
                QCoreApplication::exit(0); // see quitPoll above
                return;
            }
            followRotation();
            while (qtfb.pollInput(input)) {
                if (input.type == display::InputVkbPress || input.type == display::InputVkbRelease) {
                    // AppLoad's on-screen keyboard: x is a Qt::Key. Deliver it to
                    // the focused item (the label or SMILES field).
                    if (const auto key = keyboard.translate(input.type, input.x)) {
                        QKeyEvent keyEvent(key->press ? QEvent::KeyPress : QEvent::KeyRelease, key->key, key->modifiers, key->text);
                        QQuickItem *focused = window->activeFocusItem();
                        QCoreApplication::sendEvent(focused ? static_cast<QObject *>(focused) : static_cast<QObject *>(window), &keyEvent);
                    }
                    continue;
                }
                // 0x1n are finger events, 0x2n pen events; n is press/release/move.
                const bool isTouch = input.type >= 0x10 && input.type <= 0x12;
                QEvent::Type eventType = QEvent::MouseMove;
                Qt::MouseButton changedButton = Qt::NoButton;
                InputRouter::Phase phase = InputRouter::Phase::Move;
                switch (input.type) {
                case 0x10: // touch press
                case 0x20: // pen press
                    eventType = QEvent::MouseButtonPress;
                    changedButton = Qt::LeftButton;
                    phase = InputRouter::Phase::Press;
                    break;
                case 0x11: // touch release
                case 0x21: // pen release
                    eventType = QEvent::MouseButtonRelease;
                    changedButton = Qt::LeftButton;
                    phase = InputRouter::Phase::Release;
                    break;
                case 0x12: // touch move
                case 0x22: // pen move
                    break;
                default:
                    continue;
                }
                // Positions arrive in framebuffer pixels; turn them into window
                // coordinates when AppLoad shows the app rotated.
                const QPointF position = display::framebufferToLogical(
                    rotation, QPoint(qBound(0, input.x, framebufferSize.width() - 1), qBound(0, input.y, framebufferSize.height() - 1)),
                    framebufferSize);
                const QRectF canvasArea = canvas ? canvas->mapRectToScene(QRectF(0, 0, canvas->width(), canvas->height()))
                                                 : QRectF();
                if (isTouch) {
                    // Two-finger tap anywhere = Undo, three-finger tap = Redo (not while the pen is down).
                    const int tapped = twoFingerTap.feed(input.devId, static_cast<TwoFingerTap::Phase>(phase),
                                                          position, clock.elapsed());
                    if (twoFingerTap.fingersDown() >= 2 && router.cancelTouch() && (pressedButtons & Qt::LeftButton)) {
                        // The first finger had pressed a menu button: release it
                        // over the canvas instead, which cancels that press.
                        pressedButtons &= ~Qt::LeftButton;
                        const QPointF away = canvasArea.isValid() ? canvasArea.center() : QPointF(logicalSize.width() / 2.0, logicalSize.height() / 2.0);
                        QMouseEvent cancel(QEvent::MouseButtonRelease, away, away, Qt::LeftButton, pressedButtons, Qt::NoModifier);
                        QCoreApplication::sendEvent(window, &cancel);
                    }
                    // Two fingers moving on the canvas pan and pinch-zoom it. A drag that
                    // engaged a pan is never also a tap.
                    const PanZoomGesture::Step step = panZoom.feed(input.devId, static_cast<PanZoomGesture::Phase>(phase),
                                                                   position, canvasArea, router.penDown());
                    if (step.valid && canvas && !window->property("dialogOpen").toBool()) {
                        const QPointF local = canvas->mapFromScene(step.center);
                        QMetaObject::invokeMethod(canvas.data(), "panZoomBy", Q_ARG(QVariant, step.dx), Q_ARG(QVariant, step.dy),
                                                  Q_ARG(QVariant, step.scale), Q_ARG(QVariant, local.x()), Q_ARG(QVariant, local.y()));
                    }
                    if (tapped && !router.penDown() && !panZoom.engaged()) {
                        if (tapped == 3) editHistory.redo();
                        else editHistory.undo();
                    }
                }
                if (!router.accept(isTouch ? InputRouter::Source::Touch : InputRouter::Source::Pen, phase, position,
                                   canvasArea, window->property("dialogOpen").toBool(), input.devId))
                    continue;
                // The eraser end arrives as ordinary pen events; ask the pen which
                // end is down when a stroke starts and send eraser strokes as the
                // right button, which the canvas treats as erasing.
                // Pen activity switches the panel to its fast waveform.
                if (!isTouch) {
                    const RefreshPolicy::Action action = refreshPolicy.penEvent(clock.elapsed(), phase != InputRouter::Phase::Release);
                    if (action.setMode >= 0) qtfb.setRefreshMode(action.setMode);
                }
                if (!isTouch && phase == InputRouter::Phase::Press) eraserStroke = pen->eraserActive();
                const Qt::MouseButton button = !isTouch && eraserStroke ? Qt::RightButton : Qt::LeftButton;
                if (changedButton != Qt::NoButton) changedButton = button;
                if (phase == InputRouter::Phase::Press) pressedButtons |= button;
                if (phase == InputRouter::Phase::Release) pressedButtons &= ~button;
                if (!isTouch && phase == InputRouter::Phase::Release) eraserStroke = false;
                QMouseEvent mouseEvent(eventType, position, position, changedButton,
                                       pressedButtons, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &mouseEvent);
            }
        });
        qtfbRefresh.start(refreshPolicy.presentIntervalMs());
        qtfbInput.start(8);
    }
    return app.exec();
}
