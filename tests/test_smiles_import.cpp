#include "core/Document.h"
#include "core/GeometryEngine.h"
#include "core/Layout2D.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/SmilesReader.h"

#include <QFile>
#include <QLineF>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
const QStringList layoutMolecules = {
    "CCO", "CC(C)C", "CC(=O)O", "c1ccccc1", "Cc1ccccc1", "c1ccc2ccccc2c1", "C1CCCCC1",
    "CC(C)Cc1ccc(cc1)C(C)C(=O)O", "CC(=O)Oc1ccccc1C(=O)O", "Cn1cnc2c1c(=O)n(C)c(=O)n2C",
    "C1CCC2(C1)CCCCC2", "CC#N", "C#CC", "c1ccc2c(c1)ccc1ccccc12", "CC12CCC3C(CCc4ccccc34)C1CCC2O",
};

QString importedFormula(const QString &smiles) {
    Molecule m;
    return m.importSmiles(smiles, 0, 0) ? m.formula() : QStringLiteral("<failed>");
}
}

class SmilesImportTest : public QObject {
    Q_OBJECT
private slots:
    void parseErrors_data() {
        QTest::addColumn<QString>("smiles");
        QTest::addColumn<int>("position"); // 1-based
        QTest::addColumn<QString>("reason");
        QTest::newRow("empty") << "" << 1 << "empty";
        QTest::newRow("unclosed ring") << "C1CC" << 2 << "ring bond 1";
        QTest::newRow("unmatched close") << "CC)C" << 3 << "matching";
        QTest::newRow("unclosed branch") << "CC(C" << 5 << "never closed";
        QTest::newRow("bad char") << "CC?" << 3 << "unexpected";
        QTest::newRow("double bond symbol") << "C==C" << 3 << "two bond symbols";
        QTest::newRow("trailing bond") << "CC=" << 3 << "no atom after";
        QTest::newRow("leading bond") << "=CC" << 1 << "no atom before";
        QTest::newRow("unclosed bracket") << "[NH4+" << 1 << "never closed";
        QTest::newRow("unknown element") << "[Xx]" << 2 << "unknown element";
        QTest::newRow("empty branch") << "C()C" << 3 << "empty branch";
        QTest::newRow("self closure") << "C11" << 3 << "itself";
        QTest::newRow("dot start") << ".C" << 1 << "empty component";
        QTest::newRow("short percent") << "C%1" << 2 << "two digits";
    }
    void parseErrors() {
        QFETCH(QString, smiles);
        QFETCH(int, position);
        QFETCH(QString, reason);
        const SmilesReader::Result result = SmilesReader::parse(smiles);
        QVERIFY(!result.ok);
        QCOMPARE(result.errorPosition + 1, position);
        QVERIFY2(result.error.contains(reason), qPrintable(result.error));
        QVERIFY(result.error.startsWith("SMILES error at position"));

        Molecule molecule;
        QSignalSpy messages(&molecule, &Molecule::message);
        if (!smiles.isEmpty()) {
            QVERIFY(!molecule.importSmiles(smiles, 0, 0));
            QCOMPARE(messages.size(), 1);
            QVERIFY(molecule.atomData().isEmpty());
        }
    }

    void formulas_data() {
        QTest::addColumn<QString>("smiles");
        QTest::addColumn<QString>("formula");
        QTest::newRow("ethanol") << "CCO" << "C2H6O";
        QTest::newRow("acetic acid") << "CC(=O)O" << "C2H4O2";
        QTest::newRow("benzene") << "c1ccccc1" << "C6H6";
        QTest::newRow("kekule benzene") << "C1=CC=CC=C1" << "C6H6";
        QTest::newRow("pyridine") << "c1ccncc1" << "C5H5N";
        QTest::newRow("pyrrole") << "c1cc[nH]c1" << "C4H5N";
        QTest::newRow("furan") << "c1ccoc1" << "C4H4O";
        QTest::newRow("thiophene") << "c1ccsc1" << "C4H4S";
        QTest::newRow("caffeine") << "Cn1cnc2c1c(=O)n(C)c(=O)n2C" << "C8H10N4O2";
        QTest::newRow("ibuprofen") << "CC(C)Cc1ccc(cc1)C(C)C(=O)O" << "C13H18O2";
        QTest::newRow("aspirin") << "CC(=O)Oc1ccccc1C(=O)O" << "C9H8O4";
        QTest::newRow("ammonium") << "[NH4+]" << "H4N+";
        QTest::newRow("13C methane") << "[13CH4]" << "[13C]H4";
        QTest::newRow("naphthalene") << "c1ccc2ccccc2c1" << "C10H8";
        QTest::newRow("biphenyl") << "c1ccccc1-c1ccccc1" << "C12H10";
        QTest::newRow("biphenyl implicit") << "c1ccccc1c1ccccc1" << "C12H10";
        QTest::newRow("imidazole") << "c1c[nH]cn1" << "C3H4N2";
        QTest::newRow("pyridone") << "O=c1cccc[nH]1" << "C5H5NO";
        QTest::newRow("benzoate") << "c1ccccc1C(=O)[O-]" << "C7H5O2-";
        QTest::newRow("pyridinium") << "C[n+]1ccccc1" << "C6H8N+";
        QTest::newRow("percent closure") << "C%12CCCCC%12" << "C6H12";
        QTest::newRow("mixed closure bonds") << "C=1CCCCC=1" << "C6H10";
        QTest::newRow("triple") << "CC#CC" << "C4H6";
        QTest::newRow("dot salt") << "[Na+].[Cl-]" << "ClNa";
        QTest::newRow("chirality ignored") << "N[C@@H](C)C(=O)O" << "C3H7NO2";
        QTest::newRow("stereo bonds") << "F/C=C/F" << "C2H2F2";
        QTest::newRow("spiro") << "C1CCC2(C1)CCCCC2" << "C10H18";
        QTest::newRow("explicit H atoms") << "[CH3][CH3]" << "C2H6";
        QTest::newRow("chlorine bromine") << "ClCBr" << "CH2BrCl";
        QTest::newRow("atom class") << "[CH4:1]" << "CH4";
    }
    void formulas() {
        QFETCH(QString, smiles);
        QFETCH(QString, formula);
        if (smiles == "ClCBr") formula = "CH2BrCl";
        Molecule molecule;
        QVERIFY(molecule.importSmiles(smiles, 0, 0));
        // The dot salt formula lists elements alphabetically.
        QCOMPARE(molecule.formula(), formula);
    }

    void explicitHydrogenCountIsHonoured() {
        // The editor would give [CH5] no extra hydrogens beyond valence 4; ask for more than implied.
        Molecule molecule;
        QVERIFY(molecule.importSmiles("[OH3+]", 0, 0));
        QCOMPARE(molecule.formula(), QStringLiteral("H3O+"));
        const SmilesReader::Result extra = SmilesReader::parse("[CH6]");
        QVERIFY(extra.ok);
        QVERIFY(!extra.warnings.isEmpty());
        int hydrogens = 0;
        for (const Atom &atom : extra.atoms) hydrogens += atom.element == "H";
        QCOMPARE(hydrogens, 2); // 6 asked for, 4 implied by carbon's valence
    }

    void aromaticChoices() {
        const SmilesReader::Result benzene = SmilesReader::parse("c1ccccc1");
        int doubles = 0;
        for (const Bond &bond : benzene.bonds) {
            QCOMPARE(bond.style, QStringLiteral("benzene"));
            doubles += bond.order == 2;
        }
        QCOMPARE(doubles, 3);
        // A ring system keeps plain bonds so the benzene style's inner line is not misplaced.
        const SmilesReader::Result naphthalene = SmilesReader::parse("c1ccc2ccccc2c1");
        for (const Bond &bond : naphthalene.bonds) QCOMPARE(bond.style, QStringLiteral("plain"));
        // The bond joining two rings of biphenyl is a single bond.
        const SmilesReader::Result biphenyl = SmilesReader::parse("c1ccccc1c1ccccc1");
        int singlesBetweenRings = 0;
        for (const Bond &bond : biphenyl.bonds) if (bond.atomA == 6 && bond.atomB == 7) singlesBetweenRings += bond.order == 1;
        QCOMPARE(singlesBetweenRings, 1);
    }

    void roundTrip_data() {
        QTest::addColumn<QString>("smiles");
        for (const QString &s : layoutMolecules) QTest::newRow(qPrintable(s)) << s;
        QTest::newRow("ammonium") << "[NH4+]";
        QTest::newRow("pyrrole") << "c1cc[nH]c1";
        QTest::newRow("salt") << "[Na+].[Cl-]";
        QTest::newRow("pyridinium") << "C[n+]1ccccc1";
    }
    void roundTrip() {
        QFETCH(QString, smiles);
        Molecule first;
        QVERIFY(first.importSmiles(smiles, 0, 0));
        const QString written = first.smiles();
        QVERIFY(!written.isEmpty());
        Molecule second;
        QVERIFY2(second.importSmiles(written, 0, 0), qPrintable(written));
        QCOMPARE(second.formula(), first.formula());
        QCOMPARE(second.atomData().size(), first.atomData().size());
        QCOMPARE(second.bondData().size(), first.bondData().size());
    }

    void layoutBondLengthsAndOverlaps_data() {
        QTest::addColumn<QString>("smiles");
        for (const QString &s : layoutMolecules) QTest::newRow(qPrintable(s)) << s;
        QTest::newRow("dot components") << "CCO.c1ccccc1.CC";
        QTest::newRow("long chain") << "CCCCCCCCCCCCCCCC";
        QTest::newRow("neopentane") << "CC(C)(C)C";
        QTest::newRow("biphenyl") << "c1ccccc1-c1ccccc1";
        QTest::newRow("ammonium explicit H") << "[CH6]";
    }
    void layoutBondLengthsAndOverlaps() {
        QFETCH(QString, smiles);
        Molecule molecule;
        QVERIFY(molecule.importSmiles(smiles, 300, 200));
        const double length = GeometryEngine::StandardBondLength;
        QHash<int, QPointF> position;
        for (const Atom &atom : molecule.atomData()) position.insert(atom.id, QPointF(atom.x, atom.y));
        for (const Bond &bond : molecule.bondData()) {
            const double d = QLineF(position[bond.atomA], position[bond.atomB]).length();
            QVERIFY2(qAbs(d - length) <= 0.5, qPrintable(QStringLiteral("%1: bond %2 is %3 long").arg(smiles).arg(bond.id).arg(d)));
        }
        const QVector<Atom> &atoms = molecule.atomData();
        for (int i = 0; i < atoms.size(); ++i)
            for (int j = i + 1; j < atoms.size(); ++j) {
                const double d = QLineF(position[atoms[i].id], position[atoms[j].id]).length();
                QVERIFY2(d >= 0.6 * length, qPrintable(QStringLiteral("%1: atoms %2 and %3 are %4 apart").arg(smiles).arg(i).arg(j).arg(d)));
            }
    }

    void importIsCentred() {
        Molecule molecule;
        QVERIFY(molecule.importSmiles("CCCC", 500, -120));
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (const Atom &atom : molecule.atomData()) {
            minX = qMin(minX, atom.x); maxX = qMax(maxX, atom.x);
            minY = qMin(minY, atom.y); maxY = qMax(maxY, atom.y);
        }
        QVERIFY(qAbs((minX + maxX) / 2 - 500) < 0.01);
        QVERIFY(qAbs((minY + maxY) / 2 + 120) < 0.01);
    }

    void ringsAreRegular() {
        const double length = GeometryEngine::StandardBondLength;
        for (const QString &smiles : {QStringLiteral("c1ccccc1"), QStringLiteral("C1CCCC1"), QStringLiteral("C1CC1"),
                                      QStringLiteral("c1ccc2ccccc2c1"), QStringLiteral("C1CCC2(C1)CCCCC2")}) {
            Molecule molecule;
            QVERIFY(molecule.importSmiles(smiles, 0, 0));
            QVector<Layout2D::Edge> edges;
            QHash<int, int> index;
            for (int i = 0; i < molecule.atomData().size(); ++i) index.insert(molecule.atomData()[i].id, i);
            for (const Bond &bond : molecule.bondData()) edges.append({index[bond.atomA], index[bond.atomB], bond.order});
            const auto rings = Layout2D::findRings(molecule.atomData().size(), edges);
            QVERIFY(!rings.isEmpty());
            for (const QVector<int> &ring : rings) {
                const int n = ring.size();
                QPointF centre;
                for (const int a : ring) centre += QPointF(molecule.atomData()[a].x, molecule.atomData()[a].y);
                centre /= n;
                const double radius = length / (2.0 * qSin(M_PI / n));
                for (const int a : ring) {
                    const double d = QLineF(centre, QPointF(molecule.atomData()[a].x, molecule.atomData()[a].y)).length();
                    QVERIFY2(qAbs(d - radius) < 0.5, qPrintable(smiles));
                }
            }
        }
        QCOMPARE(Layout2D::findRings(10, {{0, 1, 1}, {1, 2, 1}, {2, 3, 1}, {3, 4, 1}, {4, 5, 1}, {5, 0, 1},
                                          {5, 6, 1}, {6, 7, 1}, {7, 8, 1}, {8, 9, 1}, {9, 5, 1}}).size(), 2);
    }

    void zigzagChainAlternates() {
        Molecule molecule;
        QVERIFY(molecule.importSmiles("CCCCCC", 0, 0));
        const QVector<Atom> &atoms = molecule.atomData();
        int ups = 0;
        for (int i = 1; i + 1 < atoms.size(); ++i) {
            const QPointF a(atoms[i].x - atoms[i - 1].x, atoms[i].y - atoms[i - 1].y);
            const QPointF b(atoms[i + 1].x - atoms[i].x, atoms[i + 1].y - atoms[i].y);
            const double angle = qRadiansToDegrees(qAcos((a.x() * b.x() + a.y() * b.y()) / (QLineF(QPointF(), a).length() * QLineF(QPointF(), b).length())));
            QVERIFY2(qAbs(angle - 60.0) < 1.0, "chain turns by 60 degrees at each atom (120 degree bond angle)");
            ups += (a.x() * b.y() - a.y() * b.x()) > 0 ? 1 : -1;
        }
        QVERIFY(qAbs(ups) <= 1); // turns alternate
    }

    void alkyneIsLinear() {
        Molecule molecule;
        QVERIFY(molecule.importSmiles("CC#CC", 0, 0));
        const QVector<Atom> &a = molecule.atomData();
        const QPointF v1(a[1].x - a[0].x, a[1].y - a[0].y), v2(a[2].x - a[1].x, a[2].y - a[1].y);
        QVERIFY(qAbs(v1.x() * v2.y() - v1.y() * v2.x()) < 1.0);
    }

    void importIsOneUndoStep() {
        Molecule molecule;
        molecule.addAtom(0, 0);
        QSignalSpy committed(&molecule, &Molecule::committed);
        QVERIFY(molecule.importSmiles("Cc1ccccc1", 100, 100));
        QCOMPARE(committed.size(), 1);
        QCOMPARE(molecule.atomData().size(), 8);
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().size(), 1);
        QVERIFY(molecule.redo());
        QCOMPARE(molecule.atomData().size(), 8);
        // Ids stay unique when importing into an existing drawing.
        QVERIFY(molecule.importSmiles("CC", 0, 0));
        QSet<int> ids;
        for (const Atom &atom : molecule.atomData()) ids.insert(atom.id);
        QCOMPARE(ids.size(), molecule.atomData().size());
        for (const Bond &bond : molecule.bondData()) { QVERIFY(ids.contains(bond.atomA)); QVERIFY(ids.contains(bond.atomB)); }
    }

    void failedImportLeavesNoUndoStep() {
        Molecule molecule;
        QSignalSpy committed(&molecule, &Molecule::committed);
        QVERIFY(!molecule.importSmiles("C1CC", 0, 0));
        QCOMPARE(committed.size(), 0);
        QVERIFY(!molecule.undo());
    }

    void onlyFirstTokenIsRead() {
        QCOMPARE(importedFormula("  CCO ethanol\n"), QStringLiteral("C2H6O"));
    }

    void smiFileOpensAsNewDrawing() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath("caffeine.smi"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Cn1cnc2c1c(=O)n(C)c(=O)n2C caffeine\nignored\n");
        file.close();
        QFile bad(dir.filePath("bad.smi"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("C1CC\n");
        bad.close();

        Molecule molecule;
        ReactionLayer reactions(&molecule);
        Document document(&molecule, &reactions, dir.path());
        molecule.addAtom(0, 0);

        bool listed = false;
        for (const QVariant &entry : document.documents()) {
            const QVariantMap map = entry.toMap();
            if (map["name"] == "caffeine") { listed = true; QCOMPARE(map["kind"].toString(), QStringLiteral("smiles")); }
        }
        QVERIFY(listed);

        QSignalSpy messages(&document, &Document::message);
        QVERIFY(!document.open(dir.filePath("bad.smi")));
        QVERIFY(!messages.isEmpty());
        QCOMPARE(molecule.atomData().size(), 1); // untouched

        QVERIFY(document.open(dir.filePath("caffeine.smi")));
        QCOMPARE(molecule.formula(), QStringLiteral("C8H10N4O2"));
        QVERIFY(document.currentPath().endsWith(".rcd")); // never overwrites the .smi
        QVERIFY(document.isDirty());                       // not saved yet
        QVERIFY(!molecule.undo());                         // fresh history
        QVERIFY(QFile::exists(dir.filePath("caffeine.smi")));
    }
};

QTEST_MAIN(SmilesImportTest)

#include "test_smiles_import.moc"
