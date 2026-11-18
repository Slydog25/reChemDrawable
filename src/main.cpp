#include "core/EditHistory.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "platform/InputSettings.h"
#include "platform/QtfbClient.h"

#include <QGuiApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

namespace {
constexpr int PaperProWidth = 1620;
constexpr int PaperProHeight = 2160;
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    Molecule molecule;
    ReactionLayer reactionLayer(&molecule);
    EditHistory editHistory(&molecule, &reactionLayer);
    InputSettings inputSettings;
    engine.rootContext()->setContextProperty(QStringLiteral("molecule"), &molecule);
    engine.rootContext()->setContextProperty(QStringLiteral("reactionLayer"), &reactionLayer);
    engine.rootContext()->setContextProperty(QStringLiteral("editHistory"), &editHistory);
    engine.rootContext()->setContextProperty(QStringLiteral("inputSettings"), &inputSettings);
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
        QObject::connect(&qtfbInput, &QTimer::timeout, &app, [&qtfb, &inputSettings, window] {
            static Qt::MouseButtons pressedButtons;
            QtfbInputEvent input;
            while (qtfb.pollInput(input)) {
                const bool isTouch = input.type >= 0x10 && input.type <= 0x12;
                if (isTouch && !inputSettings.touchDrawingEnabled())
                    continue;
                QEvent::Type eventType = QEvent::MouseMove;
                Qt::MouseButton changedButton = Qt::NoButton;
                switch (input.type) {
                case 0x10: // touch press
                case 0x20: // pen press
                    eventType = QEvent::MouseButtonPress;
                    changedButton = Qt::LeftButton;
                    pressedButtons |= Qt::LeftButton;
                    break;
                case 0x11: // touch release
                case 0x21: // pen release
                    eventType = QEvent::MouseButtonRelease;
                    changedButton = Qt::LeftButton;
                    pressedButtons &= ~Qt::LeftButton;
                    break;
                case 0x12: // touch move
                case 0x22: // pen move
                    break;
                default:
                    continue;
                }
                const QPointF position(qBound(0, input.x, PaperProWidth - 1),
                                       qBound(0, input.y, PaperProHeight - 1));
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
