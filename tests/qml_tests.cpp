// Runs the QML tests in tests/qml against the real core objects, exposed under
// the same context-property names main.cpp uses.
#include "core/Document.h"
#include "core/EditHistory.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "platform/InputSettings.h"
#include "platform/QmlJitWorkaround.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQuickTest>

class Setup : public QObject {
    Q_OBJECT

public:
    // The setup object is constructed before quick_test_main creates its engine.
    Setup() { avoidQmlJitOnPointerAuthCpus(); }

public slots:
    void qmlEngineAvailable(QQmlEngine *engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("molecule"), &m_molecule);
        engine->rootContext()->setContextProperty(QStringLiteral("reactionLayer"), &m_reactionLayer);
        engine->rootContext()->setContextProperty(QStringLiteral("editHistory"), &m_history);
        engine->rootContext()->setContextProperty(QStringLiteral("document"), &m_document);
        engine->rootContext()->setContextProperty(QStringLiteral("inputSettings"), &m_inputSettings);
    }

private:
    QTemporaryDir m_directory;
    Molecule m_molecule;
    ReactionLayer m_reactionLayer{&m_molecule};
    EditHistory m_history{&m_molecule, &m_reactionLayer};
    Document m_document{&m_molecule, &m_reactionLayer, m_directory.path()};
    InputSettings m_inputSettings;
};

QUICK_TEST_MAIN_WITH_SETUP(canvas, Setup)

#include "qml_tests.moc"
