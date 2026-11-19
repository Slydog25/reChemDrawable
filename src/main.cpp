#include "core/Document.h"
#include "core/EditHistory.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "platform/InputRouter.h"
#include "platform/QmlJitWorkaround.h"
#include "platform/QtfbClient.h"

#include <QGuiApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

namespace {
constexpr int PaperProWidth = 1620;
constexpr int PaperProHeight = 2160;
}

int main(int argc, char *argv[]) {
    avoidQmlJitOnPointerAuthCpus();
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    Molecule molecule;
    ReactionLayer reactionLayer(&molecule);
    EditHistory editHistory(&molecule, &reactionLayer);
    Document document(&molecule, &reactionLayer);
    engine.rootContext()->setContextProperty(QStringLiteral("molecule"), &molecule);
    engine.rootContext()->setContextProperty(QStringLiteral("reactionLayer"), &reactionLayer);
    engine.rootContext()->setContextProperty(QStringLiteral("editHistory"), &editHistory);
    engine.rootContext()->setContextProperty(QStringLiteral("document"), &document);
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
#ifdef RECHEMDRAW_EXTERNAL_QML
    engine.load(QUrl::fromLocalFile(QCoreApplication::applicationDirPath()
                                   + QStringLiteral("/qml/Main.qml")));
#else
    engine.load(QUrl(QStringLiteral("qrc:/src/qml/Main.qml")));
#endif
    if (engine.rootObjects().isEmpty()) return 1;

    QtfbClient qtfb;
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
        QObject::connect(&qtfbRefresh, &QTimer::timeout, &app, [&qtfb, window] {
            qtfb.present(window->grabWindow());
        });
        QPointer<QQuickItem> canvas = window->findChild<QQuickItem *>(QStringLiteral("moleculeCanvas"));
        if (!canvas) qWarning() << "qtfb: canvas not found; finger input will reach it";
        QObject::connect(&qtfbInput, &QTimer::timeout, &app, [&qtfb, window, canvas] {
            static Qt::MouseButtons pressedButtons;
            static InputRouter router;
            QtfbInputEvent input;
            while (qtfb.pollInput(input)) {
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
                const QPointF position(qBound(0, input.x, PaperProWidth - 1),
                                       qBound(0, input.y, PaperProHeight - 1));
                const QRectF canvasArea = canvas ? canvas->mapRectToScene(QRectF(0, 0, canvas->width(), canvas->height()))
                                                 : QRectF();
                if (!router.accept(isTouch ? InputRouter::Source::Touch : InputRouter::Source::Pen, phase, position,
                                   canvasArea, window->property("dialogOpen").toBool()))
                    continue;
                if (phase == InputRouter::Phase::Press) pressedButtons |= Qt::LeftButton;
                if (phase == InputRouter::Phase::Release) pressedButtons &= ~Qt::LeftButton;
                QMouseEvent mouseEvent(eventType, position, position, changedButton,
                                       pressedButtons, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &mouseEvent);
            }
        });
        qtfbRefresh.start(250);
        qtfbInput.start(8);
    }
    return app.exec();
}
