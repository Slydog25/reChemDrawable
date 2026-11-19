#include "core/Molecule.h"

#include <QLineF>
#include <QtMath>
#include <QtTest>

class TemplatesTest : public QObject {
    Q_OBJECT
private:
    static QPointF pos(const Molecule &m, int id) {
        for (const Atom &a : m.atomData()) if (a.id == id) return QPointF(a.x, a.y);
        return QPointF();
    }
    static double angleAt(const Molecule &m, int center, int a, int b) {
        const QPointF c = pos(m, center);
        const double one = qAtan2(pos(m, a).y() - c.y(), pos(m, a).x() - c.x());
        const double two = qAtan2(pos(m, b).y() - c.y(), pos(m, b).x() - c.x());
        double d = qAbs(qRadiansToDegrees(one - two));
        return d > 180 ? 360 - d : d;
    }
private slots:
    void chainLengthsAndGeometry() {
        for (int count : {1, 2, 5, 8}) {
            Molecule m;
            QVERIFY(m.addChain(-1, 100, 100, 0, count, -1) >= 0);
            QCOMPARE(m.atomData().size(), count);
            QCOMPARE(m.bondData().size(), count - 1);
            for (const Bond &b : m.bondData())
                QVERIFY(qAbs(QLineF(pos(m, b.atomA), pos(m, b.atomB)).length() - 48.0) < 1e-6);
            for (int i = 1; i + 1 < count; ++i)
                QVERIFY(qAbs(angleAt(m, m.atomData()[i].id, m.atomData()[i - 1].id, m.atomData()[i + 1].id) - 120.0) < 1e-6);
        }
        // Axis along 0 degrees, first bend upward (negative y).
        Molecule m;
        m.addChain(-1, 0, 0, 0, 3, -1);
        QVERIFY(m.atomData()[1].y < m.atomData()[0].y);
        QVERIFY(qAbs(m.atomData()[2].y - m.atomData()[0].y) < 1e-6);
    }

    void chainFromAtomAvoidsExistingBondAndIsOneUndo() {
        Molecule m;
        const int a = m.addAtom(0, 0);
        const int b = m.extendFromAtom(a, 48, 0);
        const int before = m.atomData().size();
        int commits = 0;
        connect(&m, &Molecule::committed, this, [&commits] { ++commits; });
        // Drag along the existing bond: the first new bond must not overlap it.
        QVERIFY(m.addChain(a, 0, 0, 0, 3, 1) >= 0);
        QCOMPARE(commits, 1);
        QCOMPARE(m.atomData().size(), before + 3);
        const int first = m.atomData()[before].id;
        QVERIFY(angleAt(m, a, first, b) > 25.0);
        QVERIFY(m.undo());
        QCOMPARE(m.atomData().size(), before);
    }

    void chainRefusesFullValence() {
        Molecule m;
        const int a = m.addAtom(0, 0, "O");
        m.extendFromAtom(a, 48, 0);
        m.extendFromAtom(a, -48, 0);
        QSignalSpy spy(&m, &Molecule::message);
        QCOMPARE(m.addChain(a, 0, 0, 90, 3, 1), -1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m.atomData().size(), 3);
    }

    void chainOnGridAxis() {
        // 60 degree axes keep every atom on the triangular lattice.
        Molecule m;
        m.addChain(-1, 0, 0, 60, 5, -1);
        const double column = 48.0 * qSqrt(3.0) / 2.0;
        for (const Atom &a : m.atomData()) {
            const double c = a.x / column;
            QVERIFY(qAbs(c - qRound(c)) < 1e-6);
            const double offset = (qAbs(qRound(c)) % 2) ? 24.0 : 0.0;
            const double r = (a.y - offset) / 48.0;
            QVERIFY(qAbs(r - qRound(r)) < 1e-6);
        }
    }

    void templateFormulas_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("formula");
        QTest::newRow("pyridine") << "pyridine" << "C5H5N";
        QTest::newRow("pyrimidine") << "pyrimidine" << "C4H4N2";
        QTest::newRow("furan") << "furan" << "C4H4O";
        QTest::newRow("thiophene") << "thiophene" << "C4H4S";
        QTest::newRow("pyrrole") << "pyrrole" << "C4H5N";
        QTest::newRow("imidazole") << "imidazole" << "C3H4N2";
        QTest::newRow("indole") << "indole" << "C8H7N";
        QTest::newRow("cycloheptane") << "cycloheptane" << "C7H14";
        QTest::newRow("cyclooctane") << "cyclooctane" << "C8H16";
        QTest::newRow("chair") << "chair" << "C6H12";
    }
    void templateFormulas() {
        QFETCH(QString, name);
        QFETCH(QString, formula);
        Molecule m;
        QVERIFY(m.stampRing(name, 100, 100));
        QCOMPARE(m.formula(), formula);
        QVERIFY(!m.smiles().isEmpty());
        QVERIFY(m.undo());
        QCOMPARE(m.atomData().size(), 0);
    }

    void smilesIsSensible() {
        Molecule m;
        m.stampRing("pyridine", 0, 0);
        QVERIFY2(m.smiles().contains("N"), qPrintable(m.smiles()));
        QVERIFY2(m.smiles().contains("1"), qPrintable(m.smiles()));
        Molecule f;
        f.stampRing("furan", 0, 0);
        QVERIFY2(f.smiles().contains("O"), qPrintable(f.smiles()));
        Molecule p;
        p.stampRing("pyrrole", 0, 0);
        QVERIFY2(p.smiles().contains("N"), qPrintable(p.smiles()));
    }

    void attachAsSubstituent_data() {
        QTest::addColumn<QString>("name");
        QTest::newRow("chair") << "chair";
        QTest::newRow("pyridine") << "pyridine";
        QTest::newRow("furan") << "furan";
        QTest::newRow("indole") << "indole";
        QTest::newRow("imidazole") << "imidazole";
    }
    void attachAsSubstituent() {
        QFETCH(QString, name);
        Molecule m;
        const int a = m.addAtom(0, 0);
        QVERIFY(m.stampRingAtAtom(name, a, 100, 0));
        QVERIFY(m.atomData().size() >= 6);
        // Exactly one bond leaves the anchor, of standard length.
        int count = 0;
        for (const Bond &b : m.bondData()) {
            if (b.atomA == a || b.atomB == a) {
                ++count;
                QVERIFY(qAbs(QLineF(pos(m, b.atomA), pos(m, b.atomB)).length() - 48.0) < 1e-6);
                QCOMPARE(b.order, 1);
            }
        }
        QCOMPARE(count, 1);
        QVERIFY(m.undo());
        QCOMPARE(m.atomData().size(), 1);
    }

    void tappingABondWithAHeterocycleAttachesAtTheNearerEnd() {
        Molecule m;
        const int a = m.addAtom(0, 0);
        m.extendFromAtom(a, 48, 0);
        const int bond = m.bondAt(24, 0);
        QVERIFY(m.stampRingAtBond("furan", bond, 40, 0));
        QCOMPARE(m.atomData().size(), 2 + 5);
        QCOMPARE(m.bondData().size(), 1 + 5 + 1);
    }

    void cycloheptaneFusesOntoBenzene() {
        Molecule m;
        QVERIFY(m.stampRing("benzene", 0, 0));
        const int bond = m.bondData().first().id;
        QVERIFY(m.stampRingAtBond("cycloheptane", bond, 0, 0));
        QCOMPARE(m.atomData().size(), 6 + 5);
        QCOMPARE(m.formula(), QString("C11H14"));
        // Spiro onto an atom as well.
        Molecule s;
        s.stampRing("cyclohexane", 0, 0);
        QVERIFY(s.stampRingAtAtom("cyclooctane", s.atomData().first().id, 100, 100));
        QCOMPARE(s.atomData().size(), 6 + 7);
    }
};

QTEST_MAIN(TemplatesTest)
#include "test_templates.moc"
