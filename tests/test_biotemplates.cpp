#include "core/EditHistory.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/TemplateLibrary.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class BioTemplatesTest : public QObject {
    Q_OBJECT

    static QString placedFormula(const QString &id) {
        Molecule m;
        TemplateLibrary lib(&m, QDir::tempPath() + "/rcd-bio-unused");
        return lib.place(id, 0, 0).isEmpty() ? QStringLiteral("<failed>") : m.formula();
    }

private slots:
    void aminoAcidFormulas_data() {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("formula");
        const QList<QPair<QString, QString>> rows = {
            {"gly", "C2H5NO2"}, {"ala", "C3H7NO2"}, {"val", "C5H11NO2"}, {"leu", "C6H13NO2"}, {"ile", "C6H13NO2"},
            {"pro", "C5H9NO2"}, {"phe", "C9H11NO2"}, {"trp", "C11H12N2O2"}, {"met", "C5H11NO2S"}, {"ser", "C3H7NO3"},
            {"thr", "C4H9NO3"}, {"cys", "C3H7NO2S"}, {"tyr", "C9H11NO3"}, {"asn", "C4H8N2O3"}, {"gln", "C5H10N2O3"},
            {"asp", "C4H7NO4"}, {"glu", "C5H9NO4"}, {"lys", "C6H14N2O2"}, {"arg", "C6H14N4O2"}, {"his", "C6H9N3O2"}};
        for (const auto &r : rows) QTest::newRow(qPrintable(r.first)) << "amino:" + r.first << r.second;
    }
    void aminoAcidFormulas() {
        QFETCH(QString, id);
        QFETCH(QString, formula);
        QCOMPARE(placedFormula(id), formula);
    }

    void twentyAminoAcidsListed() {
        Molecule m;
        QTemporaryDir dir;
        TemplateLibrary lib(&m, dir.path());
        QCOMPARE(lib.items("amino").size(), 20);
        QCOMPARE(lib.items("bases").size(), 5);
    }

    void lConfiguration_data() {
        QTest::addColumn<QString>("id");
        QTest::addColumn<int>("stereocentres");
        QTest::addColumn<QString>("alpha");
        for (const QString &code : {"ala", "val", "leu", "pro", "phe", "trp", "met", "ser", "tyr", "asn", "gln", "asp", "glu", "lys", "arg", "his", "ile", "thr"})
            QTest::newRow(qPrintable(code)) << "amino:" + code << (code == "ile" || code == "thr" ? 2 : 1) << "S";
        QTest::newRow("cys") << "amino:cys" << 1 << "R";
        QTest::newRow("gly") << "amino:gly" << 0 << "";
    }
    void lConfiguration() {
        QFETCH(QString, id);
        QFETCH(int, stereocentres);
        QFETCH(QString, alpha);
        Molecule m;
        QTemporaryDir dir;
        TemplateLibrary lib(&m, dir.path());
        QVERIFY(!lib.place(id, 0, 0).isEmpty());
        const QVariantList descriptors = m.stereoDescriptors();
        QCOMPARE(descriptors.size(), stereocentres);
        QStringList labels;
        for (const QVariant &v : descriptors) labels << v.toMap().value("label").toString();
        if (stereocentres) QVERIFY2(labels.contains(alpha), qPrintable(id + ": " + labels.join(",")));
        if (id == "amino:thr") QVERIFY2(labels.contains("R") && labels.contains("S"), qPrintable(labels.join(",")));   // 2S,3R
        if (id == "amino:ile") QCOMPARE(labels.count("S"), 2);  // 2S,3S
    }

    void nucleobaseFormulas_data() {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("formula");
        QTest::newRow("A") << "base:a" << "C5H5N5";
        QTest::newRow("G") << "base:g" << "C5H5N5O";
        QTest::newRow("C") << "base:c" << "C4H5N3O";
        QTest::newRow("T") << "base:t" << "C5H6N2O2";
        QTest::newRow("U") << "base:u" << "C4H4N2O2";
    }
    void nucleobaseFormulas() {
        QFETCH(QString, id);
        QFETCH(QString, formula);
        QCOMPARE(placedFormula(id), formula);
    }

    void reagentFormulas() {
        QCOMPARE(placedFormula("reagent:dmap"), QString("C7H10N2"));
        QCOMPARE(placedFormula("reagent:dcc"), QString("C13H22N2"));
        QCOMPARE(placedFormula("reagent:nbs"), QString("C4H4BrNO2"));
        QCOMPARE(placedFormula("reagent:tscl"), QString("C7H7ClO2S"));
    }

    // Haworth: ordinary atoms and bonds, front edge "bold".
    void haworth() {
        QCOMPARE(placedFormula("proj:alphaglucose"), QString("C6H12O6"));
        QCOMPARE(placedFormula("proj:betaglucose"), QString("C6H12O6"));
        QCOMPARE(placedFormula("proj:ribofuranose"), QString("C5H10O5"));
        Molecule m;
        QTemporaryDir dir;
        TemplateLibrary lib(&m, dir.path());
        lib.place("proj:betaglucose", 0, 0);
        int bold = 0;
        for (const Bond &b : m.bondData()) bold += b.style == "bold";
        QCOMPARE(bold, 3);
        QVERIFY(m.smiles().contains("O"));
    }

    // Newman: C2H6 with six hydrogens; the circle is a bond style, so the formula is unaffected.
    void newmanAndFischer() {
        QCOMPARE(placedFormula("proj:newman"), QString("C2H6"));
        Molecule m;
        QTemporaryDir dir;
        TemplateLibrary lib(&m, dir.path());
        lib.place("proj:newman", 0, 0);
        int circles = 0, back = 0;
        for (const Bond &b : m.bondData()) { circles += b.style == "newman"; back += b.style == "newmanback"; }
        QCOMPARE(circles, 1);
        QCOMPARE(back, 3);
        QCOMPARE(placedFormula("proj:fischer2"), QString("C2H6O2"));
        QCOMPARE(placedFormula("proj:dglyceraldehyde"), QString("C3H6O3"));
        QCOMPARE(placedFormula("proj:lglyceraldehyde"), QString("C3H6O3"));
        QCOMPARE(placedFormula("proj:derythrose"), QString("C4H8O4"));
        QCOMPARE(placedFormula("proj:dthreose"), QString("C4H8O4"));
        // Fischer carbons are real atoms at the crossings; no wedge or dash is drawn, so no CIP labels.
        Molecule f;
        TemplateLibrary lib2(&f, dir.path());
        lib2.place("proj:derythrose", 0, 0);
        QVERIFY(f.stereoDescriptors().isEmpty());
        QVERIFY(!f.smiles().isEmpty());
    }

    void specialStylesSurviveSaveAndLoad() {
        QTemporaryDir dir;
        Molecule m;
        ReactionLayer reactions(&m);
        TemplateLibrary lib(&m, dir.path());
        lib.place("proj:newman", 0, 0);
        lib.place("proj:betaglucose", 300, 0);
        const QString path = dir.filePath("x.rcd");
        QVERIFY(reactions.saveToFile(path));
        Molecule m2;
        ReactionLayer r2(&m2);
        QVERIFY(r2.loadFromFile(path));
        QCOMPARE(m2.bondData().size(), m.bondData().size());
        for (int i = 0; i < m.bondData().size(); ++i) QCOMPARE(m2.bondData()[i].style, m.bondData()[i].style);
        const QString svg = QStringLiteral("x");
        QVERIFY(m.saveSvgToFile(dir.filePath("x.svg")));
        QFile f(dir.filePath("x.svg"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray text = f.readAll();
        QVERIFY(text.contains("<circle"));
        QVERIFY(text.contains("stroke-width=\"7\""));
    }

    void placeIsOneUndoStep() {
        QTemporaryDir dir;
        Molecule m;
        ReactionLayer reactions(&m);
        EditHistory history(&m, &reactions);
        TemplateLibrary lib(&m, dir.path());
        const QVariantList ids = lib.place("amino:trp", 100, 50);
        QCOMPARE(ids.size(), 15);
        QVERIFY(history.undo());
        QCOMPARE(m.atomData().size(), 0);
        QVERIFY(history.redo());
        QCOMPARE(m.atomData().size(), 15);
        // Centred on the requested point.
        double cx = 0, cy = 0;
        for (const Atom &a : m.atomData()) { cx += a.x; cy += a.y; }
        QVERIFY(qAbs(cx / 15 - 100) < 0.01 && qAbs(cy / 15 - 50) < 0.01);
    }

    void myTemplatesRoundTrip() {
        QTemporaryDir dir;
        Molecule m;
        {
            TemplateLibrary lib(&m, dir.path());
            QVERIFY(!lib.saveSelection({}));
            QCOMPARE(lib.mineCount(), 0);
            QVERIFY(m.importSmiles("CC(=O)O", 0, 0));
            QVERIFY(m.setBondStyle(1, "wedge"));
            QVariantList all;
            for (const Atom &a : m.atomData()) all << a.id;
            QSignalSpy spy(&lib, &TemplateLibrary::mineChanged);
            QVERIFY(lib.saveSelection(all));
            QVERIFY(lib.saveSelection(all));
            QCOMPARE(spy.count(), 2);
            const QVariantList mine = lib.items("mine");
            QCOMPARE(mine.size(), 2);
            QCOMPARE(mine[0].toMap().value("name").toString(), QString("C2H4O2 #1"));
            QCOMPARE(mine[1].toMap().value("name").toString(), QString("C2H4O2 #2"));
            QCOMPARE(mine[0].toMap().value("formula").toString(), QString("C2H4O2"));
        }
        // A new library (new Molecule) sees the same templates.
        Molecule other;
        ReactionLayer reactions(&other);
        EditHistory history(&other, &reactions);
        TemplateLibrary lib(&other, dir.path());
        QCOMPARE(lib.mineCount(), 2);
        const QString id = lib.items("mine")[0].toMap().value("id").toString();
        QCOMPARE(lib.preview(id).value("atoms").toList().size(), 4);
        const QVariantList placed = lib.place(id, 200, 200);
        QCOMPARE(placed.size(), 4);
        QCOMPARE(other.formula(), QString("C2H4O2"));
        int wedges = 0;
        for (const Bond &b : other.bondData()) wedges += b.style == "wedge";
        QCOMPARE(wedges, 1);
        QVERIFY(history.undo());
        QCOMPARE(other.atomData().size(), 0);

        // Delete one; the other stays, and so does the counter-based naming.
        QVERIFY(lib.deleteMine(id));
        QVERIFY(!lib.deleteMine(id));
        QCOMPARE(lib.mineCount(), 1);
        TemplateLibrary again(&other, dir.path());
        QCOMPARE(again.mineCount(), 1);
        QCOMPARE(again.items("mine")[0].toMap().value("name").toString(), QString("C2H4O2 #2"));
        QVERIFY(QFile::exists(again.filePath()));
    }

    void unknownIdPlacesNothing() {
        Molecule m;
        QTemporaryDir dir;
        TemplateLibrary lib(&m, dir.path());
        QVERIFY(lib.place("nope", 0, 0).isEmpty());
        QCOMPARE(m.atomData().size(), 0);
    }
};

QTEST_MAIN(BioTemplatesTest)
#include "test_biotemplates.moc"
