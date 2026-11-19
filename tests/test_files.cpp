#include "core/Document.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/ThumbnailWriter.h"

#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class FilesTest : public QObject {
    Q_OBJECT

private slots:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        m_molecule = std::make_unique<Molecule>();
        m_layer = std::make_unique<ReactionLayer>(m_molecule.get());
        m_doc = std::make_unique<Document>(m_molecule.get(), m_layer.get(), m_dir->path());
        m_doc->setAutosaveDelayMs(40);
    }

    void untitledIsNotAutosaved() {
        m_molecule->addAtom(0, 0);
        QVERIFY(m_doc->isDirty());
        QVERIFY(!m_doc->autosavePending());
        QTest::qWait(150);
        QVERIFY(m_doc->isDirty());
        QVERIFY(m_doc->currentPath().isEmpty());
        QVERIFY(m_doc->documents().isEmpty());
    }

    void autosaveFiresAfterDelayAndClearsDirty() {
        m_molecule->addAtom(0, 0);
        QVERIFY(m_doc->save());
        const QString path = m_doc->currentPath();
        QSignalSpy spy(m_doc.get(), &Document::autosaved);
        m_molecule->addAtom(50, 0);
        QVERIFY(m_doc->isDirty());
        QVERIFY(m_doc->autosavePending());
        QVERIFY(spy.wait(1000));
        QVERIFY(!m_doc->isDirty());
        QVERIFY(!m_doc->autosavePending());
        QCOMPARE(m_doc->currentPath(), path);
        Molecule check;
        ReactionLayer checkLayer(&check);
        QVERIFY(checkLayer.loadFromFile(path));
        QCOMPARE(check.atoms().size(), 2);
    }

    void autosaveIsDebounced() {
        m_molecule->addAtom(0, 0);
        QVERIFY(m_doc->save());
        QSignalSpy spy(m_doc.get(), &Document::autosaved);
        for (int i = 0; i < 5; ++i) {
            m_molecule->addAtom(i * 10, 10);
            QTest::qWait(20); // shorter than the delay, so the timer keeps restarting
        }
        QCOMPARE(spy.count(), 0);
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);
        QTest::qWait(150);
        QCOMPARE(spy.count(), 1);
    }

    void openedRcdIsAutosavedButMolIsNot() {
        m_molecule->addAtom(0, 0);
        QVERIFY(m_doc->save());
        const QString rcd = m_doc->currentPath();
        QVERIFY(m_doc->open(rcd));
        QVERIFY(!m_doc->isDirty());
        QTest::qWait(100);
        QVERIFY(!m_doc->autosavePending());
        m_molecule->addAtom(40, 0);
        QSignalSpy spy(m_doc.get(), &Document::autosaved);
        QVERIFY(spy.wait(1000));

        const QString mol = m_dir->filePath("ethanol.mol");
        QVERIFY(m_molecule->saveToFile(mol));
        QVERIFY(m_doc->open(mol));
        QVERIFY(m_doc->isDirty());
        m_molecule->addAtom(100, 100);
        QTest::qWait(200);
        QCOMPARE(spy.count(), 1);
        QVERIFY(m_doc->isDirty());
        QVERIFY(!QFile::exists(m_dir->filePath("ethanol.rcd")));
        // Saving manually turns it into a real file, which is then autosaved.
        QVERIFY(m_doc->save());
        m_molecule->addAtom(150, 100);
        QVERIFY(spy.wait(1000));
    }

    void flushAutosaveSavesImmediately() {
        m_molecule->addAtom(0, 0);
        QVERIFY(m_doc->save());
        m_molecule->addAtom(40, 0);
        QVERIFY(m_doc->flushAutosave());
        QVERIFY(!m_doc->isDirty());
        QVERIFY(!m_doc->flushAutosave());
    }

    void thumbnailsAreWrittenOnSave() {
        const int a = m_molecule->addAtom(0, 0);
        const int b = m_molecule->addAtom(60, 0, "O");
        m_molecule->addBond(a, b, 2);
        m_layer->addArrow(100, 0, 200, 0);
        m_layer->addLabel("heat", 150, -20);
        QVERIFY(m_doc->save());
        const QString thumb = Document::thumbnailPathFor(m_doc->currentPath());
        QVERIFY(thumb.contains("/.thumbs/"));
        QVERIFY(QFile::exists(thumb));
        QImage image(thumb);
        QCOMPARE(image.size(), QSize(ThumbnailWriter::DefaultWidth, ThumbnailWriter::DefaultHeight));
        bool hasInk = false;
        for (int y = 0; y < image.height() && !hasInk; ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qGray(image.pixel(x, y)) < 100) { hasInk = true; break; }
        QVERIFY(hasInk);

        // Autosave rewrites it.
        QFile::remove(thumb);
        m_molecule->addAtom(0, 80);
        QVERIFY(m_doc->flushAutosave());
        QVERIFY(QFile::exists(thumb));
    }

    void documentsListsThumbnailAndFormula() {
        const int a = m_molecule->addAtom(0, 0);
        const int b = m_molecule->addAtom(60, 0, "O");
        m_molecule->addBond(a, b);
        QVERIFY(m_doc->save());
        const QVariantList list = m_doc->documents();
        QCOMPARE(list.size(), 1);
        const QVariantMap entry = list.first().toMap();
        QCOMPARE(entry.value("thumbnail").toString(), Document::thumbnailPathFor(m_doc->currentPath()));
        QCOMPARE(entry.value("formula").toString(), m_molecule->formula());
        QVERIFY(!entry.value("formula").toString().isEmpty());
        // A file without a preview falls back to an empty thumbnail.
        QVERIFY(QFile::remove(entry.value("thumbnail").toString()));
        QCOMPARE(m_doc->documents().first().toMap().value("thumbnail").toString(), QString());
    }

    void deleteRemovesFileAndThumbnail() {
        m_molecule->addAtom(0, 0);
        QVERIFY(m_doc->save());
        const QString path = m_doc->currentPath();
        m_doc->newDocument();
        const QString thumb = Document::thumbnailPathFor(path);
        QVERIFY(QFile::exists(thumb));
        QVERIFY(m_doc->deleteDocument(path));
        QVERIFY(!QFile::exists(path));
        QVERIFY(!QFile::exists(thumb));
        QVERIFY(m_doc->documents().isEmpty());
        QVERIFY(!m_doc->deleteDocument(path));
        QVERIFY(!m_doc->deleteDocument(QDir::tempPath() + "/elsewhere.rcd"));
    }

    void deletingTheCurrentDrawingKeepsTheCanvas() {
        m_molecule->addAtom(0, 0);
        m_molecule->addAtom(40, 0);
        QVERIFY(m_doc->save());
        const QString path = m_doc->currentPath();
        QVERIFY(m_doc->deleteDocument(path));
        QCOMPARE(m_molecule->atoms().size(), 2);
        QCOMPARE(m_doc->title(), QString("Untitled"));
        QVERIFY(m_doc->currentPath().isEmpty());
        QVERIFY(m_doc->isDirty());
        // Not autosaved any more, but a manual save writes a new file.
        m_molecule->addAtom(80, 0);
        QTest::qWait(150);
        QVERIFY(!QFile::exists(path));
        QVERIFY(m_doc->save());
        QVERIFY(QFile::exists(m_doc->currentPath()));
    }

    void recoveryFilesAreListedAndDeletable() {
        m_molecule->addAtom(0, 0);
        const QString recovered = m_doc->saveRecoveryCopy();
        QVERIFY(!recovered.isEmpty());
        const QVariantMap entry = m_doc->documents().first().toMap();
        QVERIFY(entry.value("name").toString().startsWith("recovered-"));
        QVERIFY(!entry.value("thumbnail").toString().isEmpty());
        QVERIFY(m_doc->deleteDocument(recovered));
        QVERIFY(m_doc->documents().isEmpty());
        QVERIFY(m_doc->isDirty()); // unrelated to the current drawing
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Molecule> m_molecule;
    std::unique_ptr<ReactionLayer> m_layer;
    std::unique_ptr<Document> m_doc;
};

QTEST_MAIN(FilesTest)
#include "test_files.moc"
