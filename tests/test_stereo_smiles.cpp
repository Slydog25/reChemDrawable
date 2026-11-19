#include "core/Molecule.h"

#include <QSignalSpy>
#include <QtTest>

namespace {
// atom/bond id -> label for an imported molecule (ids follow SMILES order, from 1).
QMap<int, QString> atomLabels(const Molecule &m) {
    QMap<int, QString> out;
    for (const QVariant &v : m.stereoDescriptors()) {
        const QVariantMap d = v.toMap();
        if (d.value("kind").toString() == "atom") out.insert(d.value("atomId").toInt(), d.value("label").toString());
    }
    return out;
}
QMap<int, QString> bondLabels(const Molecule &m) {
    QMap<int, QString> out;
    for (const QVariant &v : m.stereoDescriptors()) {
        const QVariantMap d = v.toMap();
        if (d.value("kind").toString() == "bond") out.insert(d.value("bondId").toInt(), d.value("label").toString());
    }
    return out;
}
QStringList sortedLabels(const Molecule &m) {
    QStringList list;
    for (const QVariant &v : m.stereoDescriptors()) list << v.toMap().value("kind").toString() + v.toMap().value("label").toString();
    list.sort();
    return list;
}
QString onlyLabel(const QString &smiles) {
    Molecule m;
    if (!m.importSmiles(smiles, 0, 0)) return "<import failed>";
    const QStringList l = sortedLabels(m);
    return l.size() == 1 ? l.first() : QString("<%1 labels>").arg(l.size());
}
}

class StereoSmilesTest : public QObject {
    Q_OBJECT
private slots:
    void tetrahedral_data() {
        QTest::addColumn<QString>("smiles");
        QTest::addColumn<QString>("label");
        QTest::newRow("L-alanine") << "N[C@@H](C)C(=O)O" << "atomS";
        QTest::newRow("D-alanine") << "N[C@H](C)C(=O)O" << "atomR";
        QTest::newRow("L-alanine other order") << "C[C@H](N)C(=O)O" << "atomS";
        QTest::newRow("(R)-butan-2-ol") << "C[C@@H](O)CC" << "atomR";
        QTest::newRow("(S)-butan-2-ol") << "C[C@H](O)CC" << "atomS";
        QTest::newRow("implicit H first") << "[C@H](F)(Cl)Br" << "atomS";
        QTest::newRow("implicit H second") << "F[C@H](Cl)Br" << "atomR";
        QTest::newRow("four heavy neighbours") << "F[C@](Cl)(Br)I" << "atomS";
        QTest::newRow("TH1 form") << "F[C@TH1](Cl)(Br)I" << "atomS";
        QTest::newRow("ring closure after H") << "F[C@H]1CCCCO1" << "atomS";
        QTest::newRow("ring digit before branch") << "[C@H]1(F)CCCCO1" << "atomS";
        QTest::newRow("ring closure at closer") << "C1CCCO[C@H]1F" << "atomS";
    }
    void tetrahedral() {
        QFETCH(QString, smiles);
        QFETCH(QString, label);
        QCOMPARE(onlyLabel(smiles), label);
    }

    void ringStereo() {
        Molecule trans;
        QVERIFY(trans.importSmiles("C[C@@H]1CCCC[C@H]1O", 0, 0)); // (1R,2R)-2-methylcyclohexanol
        const QMap<int, QString> t = atomLabels(trans);
        QCOMPARE(t.size(), 2);
        QCOMPARE(t.value(2), QString("R"));
        QCOMPARE(t.value(7), QString("R"));
        Molecule cis;
        QVERIFY(cis.importSmiles("C[C@@H]1CCCC[C@@H]1O", 0, 0));
        const QMap<int, QString> c = atomLabels(cis);
        QCOMPARE(c.value(2), QString("R"));
        QCOMPARE(c.value(7), QString("S"));
    }

    void oneWedgePerCentreStartingAtIt() {
        Molecule m;
        QVERIFY(m.importSmiles("O[C@H]([C@@H](O)C(=O)O)C(=O)O", 0, 0));
        int centres = 0;
        for (const Atom &a : m.atomData()) {
            int n = 0;
            for (const Bond &b : m.bondData())
                if (b.atomA == a.id && (b.style == "wedge" || b.style == "dash")) ++n;
            if (n) ++centres;
            QVERIFY(n <= 1);
        }
        QCOMPARE(centres, 2);
        QCOMPARE(atomLabels(m).size(), 2);
    }

    void plainSmilesHasNoStereoBonds() {
        Molecule m;
        QVERIFY(m.importSmiles("CC(C)C(=O)O", 0, 0));
        for (const Bond &b : m.bondData()) QCOMPARE(b.style, QString("plain"));
    }

    void doubleBonds_data() {
        QTest::addColumn<QString>("smiles");
        QTest::addColumn<QString>("label");
        QTest::newRow("E-butene") << "C/C=C/C" << "bondE";
        QTest::newRow("Z-butene") << "C/C=C\\C" << "bondZ";
        QTest::newRow("F/C=C/F") << "F/C=C/F" << "bondE";
        QTest::newRow("F/C=C\\F") << "F/C=C\\F" << "bondZ";
        QTest::newRow("F\\C=C\\F") << "F\\C=C\\F" << "bondE";
        QTest::newRow("branch form") << "C(/F)=C/F" << "bondZ";
        QTest::newRow("branch form backslash") << "C(\\F)=C/F" << "bondE";
    }
    void doubleBonds() {
        QFETCH(QString, smiles);
        QFETCH(QString, label);
        QCOMPARE(onlyLabel(smiles), label);
    }

    void conjugatedDiene() {
        Molecule ee;
        QVERIFY(ee.importSmiles("C/C=C/C=C/C", 0, 0));
        QCOMPARE(sortedLabels(ee), QStringList({"bondE", "bondE"}));
        Molecule zz;
        QVERIFY(zz.importSmiles("C/C=C\\C=C/C", 0, 0));
        QCOMPARE(sortedLabels(zz), QStringList({"bondZ", "bondZ"}));
    }

    void ringDoubleBondIgnored() {
        Molecule m;
        QVERIFY(m.importSmiles("C1=C/CCCCCC1", 0, 0));
        QVERIFY(m.atomData().size() == 8);
    }

    void roundTrip_data() {
        QTest::addColumn<QString>("smiles");
        const QStringList list = {
            "N[C@@H](C)C(=O)O", "N[C@H](C)C(=O)O", "C[C@@H](O)CC", "C[C@H](O)CC", "F[C@H](Cl)Br",
            "F[C@](Cl)(Br)I", "F[C@H]1CCCCO1", "C[C@@H]1CCCC[C@H]1O", "C[C@@H]1CCCC[C@@H]1O",
            "CC(C)[C@@H]1CC[C@@H](C)C[C@H]1O", "OC[C@H]1OC(O)[C@H](O)[C@@H](O)[C@@H]1O",
            "O=C(O)[C@H](O)[C@@H](O)C(=O)O", "C/C=C/C", "C/C=C\\C", "F/C=C/F", "F/C=C\\F",
            "C/C=C/C=C/C", "C/C=C\\C=C/C", "C[C@H](O)/C=C/CCCC", "C[C@H](O)/C=C\\CCCC",
            "N[C@@H](Cc1ccccc1)C(=O)O",
        };
        for (const QString &s : list) QTest::newRow(qPrintable(s)) << s;
    }
    void roundTrip() {
        QFETCH(QString, smiles);
        Molecule a;
        QVERIFY(a.importSmiles(smiles, 0, 0));
        const QStringList before = sortedLabels(a);
        QVERIFY(!before.isEmpty());
        const QString written = a.smiles();
        Molecule b;
        QVERIFY2(b.importSmiles(written, 0, 0), qPrintable(written));
        QCOMPARE(sortedLabels(b), before);
        if (smiles.contains('@')) QVERIFY2(written.contains('@'), qPrintable(written));
        if (smiles.contains('/') || smiles.contains('\\')) QVERIFY2(written.contains('/') || written.contains('\\'), qPrintable(written));
    }

    void writerLeavesPlainMoleculesAlone() {
        Molecule m;
        QVERIFY(m.importSmiles("CC(=O)Oc1ccccc1C(=O)O", 0, 0));
        QVERIFY(!m.smiles().contains('@'));
        QVERIFY(!m.smiles().contains('/'));
    }

    void invertStereocentre() {
        Molecule m;
        QVERIFY(m.importSmiles("N[C@@H](C)C(=O)O", 0, 0));
        QCOMPARE(atomLabels(m).value(2), QString("S"));
        QSignalSpy committed(&m, &Molecule::committed);
        QVERIFY(m.invertStereocentre(2));
        QCOMPARE(committed.count(), 1);
        QCOMPARE(atomLabels(m).value(2), QString("R"));
        QVERIFY(m.smiles().contains("@"));
        QVERIFY(m.undo());
        QCOMPARE(atomLabels(m).value(2), QString("S"));
        QVERIFY(m.invertStereocentre(2));
        QVERIFY(m.invertStereocentre(2));
        QCOMPARE(atomLabels(m).value(2), QString("S"));
    }

    void invertWithoutWedgeDoesNothing() {
        Molecule m;
        QVERIFY(m.importSmiles("CCO", 0, 0));
        QSignalSpy committed(&m, &Molecule::committed);
        QVERIFY(!m.invertStereocentre(2));
        QCOMPARE(committed.count(), 0);
    }

    void invertDoubleBond() {
        Molecule m;
        QVERIFY(m.importSmiles("C/C=C/C", 0, 0));
        QCOMPARE(bondLabels(m).value(2), QString("E"));
        QSignalSpy committed(&m, &Molecule::committed);
        QVERIFY(m.invertDoubleBond(2));
        QCOMPARE(committed.count(), 1);
        QCOMPARE(bondLabels(m).value(2), QString("Z"));
        QVERIFY(m.undo());
        QCOMPARE(bondLabels(m).value(2), QString("E"));
    }

    void invertDoubleBondKeepsOtherCentres() {
        Molecule m;
        QVERIFY(m.importSmiles("C[C@H](O)/C=C/CCCC", 0, 0));
        const QMap<int, QString> before = atomLabels(m);
        QCOMPARE(before.size(), 1);
        QCOMPARE(bondLabels(m).value(4), QString("E"));
        QVERIFY(m.invertDoubleBond(4));
        QCOMPARE(bondLabels(m).value(4), QString("Z"));
        QCOMPARE(atomLabels(m), before);
    }

    void invertRingDoubleBondRefused() {
        Molecule m;
        QVERIFY(m.importSmiles("C1=CCCCC1", 0, 0));
        QSignalSpy committed(&m, &Molecule::committed);
        QVERIFY(!m.invertDoubleBond(1));
        QCOMPARE(committed.count(), 0);
    }
};

QTEST_MAIN(StereoSmilesTest)
#include "test_stereo_smiles.moc"
