#include "core/Molecule.h"
#include "core/Stereo.h"

#include <QtMath>
#include <QtTest>

namespace {
// Positions are given relative to a centre in screen coordinates (y down) and
// rotated by `angle` degrees about it.
struct Frame {
    double cx = 300, cy = 300, angle = 0;
    QPointF at(double dx, double dy) const {
        const double r = qDegreesToRadians(angle);
        return {cx + dx * qCos(r) - dy * qSin(r), cy + dx * qSin(r) + dy * qCos(r)};
    }
};

int addAt(Molecule &m, const Frame &f, double dx, double dy, const QString &element = QStringLiteral("C")) {
    const QPointF p = f.at(dx, dy);
    return m.addAtom(p.x(), p.y(), element);
}

int bond(Molecule &m, int a, int b, const QString &style = QStringLiteral("plain"), int order = 1) {
    const int id = m.addBond(a, b, order);
    if (style != QStringLiteral("plain")) m.setBondStyle(id, style);
    return id;
}

QString labelFor(const Molecule &m, int atomId, int bondId = -1) {
    for (const Stereo::Descriptor &d : Stereo::compute(m))
        if ((atomId >= 0 && d.atomId == atomId) || (bondId >= 0 && d.bondId == bondId)) return d.label;
    return QString();
}

// Centre with substituents at the three standard 2D positions: top, lower
// right, lower left. Returns the centre id; `ids` receives top, right, left.
const QString W = QStringLiteral("wedge"), D = QStringLiteral("dash"), P = QStringLiteral("plain");
}

class StereoTest : public QObject {
    Q_OBJECT
private slots:
    // Butan-2-ol: O top, ethyl lower right, methyl lower left, H implicit and
    // behind: O -> Et -> Me runs clockwise, so (R).
    void butanolRotationsAndWedgeSites_data() {
        QTest::addColumn<double>("angle");
        QTest::addColumn<int>("wedgeOn"); // 0 = O, 1 = ethyl, 2 = methyl
        QTest::addColumn<QString>("style");
        QTest::addColumn<QString>("expected");
        for (double angle : {0.0, 90.0, 137.0, 200.0, 301.0})
            for (int site = 0; site < 3; ++site) {
                QTest::addRow("wedge_%d_%g", site, angle) << angle << site << W << "R";
                QTest::addRow("dash_%d_%g", site, angle) << angle << site << D << "S";
            }
    }
    void butanolRotationsAndWedgeSites() {
        QFETCH(double, angle); QFETCH(int, wedgeOn); QFETCH(QString, style); QFETCH(QString, expected);
        Molecule m;
        Frame f; f.angle = angle;
        const int c = addAt(m, f, 0, 0);
        const int o = addAt(m, f, 0, -50, "O");
        const int et = addAt(m, f, 45, 25);
        const int et2 = addAt(m, f, 90, 0);
        const int me = addAt(m, f, -45, 25);
        const int ids[3] = {o, et, me};
        for (int i = 0; i < 3; ++i) bond(m, c, ids[i], i == wedgeOn ? style : P);
        bond(m, et, et2);
        QCOMPARE(labelFor(m, c), expected);
    }

    void mirrorImageButanolIsS() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), o = addAt(m, f, 0, -50, "O");
        const int et = addAt(m, f, -45, 25), et2 = addAt(m, f, -90, 0), me = addAt(m, f, 45, 25);
        bond(m, c, o, W); bond(m, c, et); bond(m, c, me); bond(m, et, et2);
        QCOMPARE(labelFor(m, c), QString("S"));
    }

    void wedgeStartingAtTheNeighbourDoesNotCount() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), o = addAt(m, f, 0, -50, "O");
        const int et = addAt(m, f, 45, 25), et2 = addAt(m, f, 90, 0), me = addAt(m, f, -45, 25);
        bond(m, o, c, W); bond(m, c, et); bond(m, c, me); bond(m, et, et2);
        QVERIFY(labelFor(m, c).isEmpty());
    }

    void plainBondsGiveNoDescriptor() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), o = addAt(m, f, 0, -50, "O");
        const int et = addAt(m, f, 45, 25), et2 = addAt(m, f, 90, 0), me = addAt(m, f, -45, 25);
        bond(m, c, o); bond(m, c, et); bond(m, c, me); bond(m, et, et2);
        QVERIFY(Stereo::compute(m).isEmpty());
    }

    void lAlanineIsS() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), n = addAt(m, f, 0, -50, "N");
        const int acid = addAt(m, f, -45, 25), o1 = addAt(m, f, -90, 0, "O"), o2 = addAt(m, f, -45, 75, "O");
        const int me = addAt(m, f, 45, 25);
        bond(m, c, n, W); bond(m, c, acid); bond(m, c, me);
        bond(m, acid, o1, P, 2); bond(m, acid, o2);
        QCOMPARE(labelFor(m, c), QString("S"));
    }

    void dAlanineIsRWhenNitrogenDashed() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), n = addAt(m, f, 0, -50, "N");
        const int acid = addAt(m, f, -45, 25), o1 = addAt(m, f, -90, 0, "O"), o2 = addAt(m, f, -45, 75, "O");
        const int me = addAt(m, f, 45, 25);
        bond(m, c, n, D); bond(m, c, acid); bond(m, c, me);
        bond(m, acid, o1, P, 2); bond(m, acid, o2);
        QCOMPARE(labelFor(m, c), QString("R"));
    }

    void twoIdenticalSubstituentsGiveNone() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), o = addAt(m, f, 0, -50, "O");
        const int a = addAt(m, f, 45, 25), b = addAt(m, f, -45, 25);
        bond(m, c, o, W); bond(m, c, a); bond(m, c, b);
        QVERIFY(Stereo::compute(m).isEmpty());
    }

    void twoSubstituentsOnlyGiveNone() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), o = addAt(m, f, 0, -50, "O"), a = addAt(m, f, 45, 25);
        bond(m, c, o, W); bond(m, c, a);
        QVERIFY(Stereo::compute(m).isEmpty());
    }

    void dGlyceraldehydeIsR() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), oh = addAt(m, f, 0, -50, "O");
        const int cho = addAt(m, f, 45, 25), ox = addAt(m, f, 90, 0, "O");
        const int ch2 = addAt(m, f, -45, 25), oy = addAt(m, f, -90, 0, "O");
        bond(m, c, oh, W); bond(m, c, cho); bond(m, c, ch2);
        bond(m, cho, ox, P, 2); bond(m, ch2, oy);
        QCOMPARE(labelFor(m, c), QString("R"));
    }

    void bromochlorofluoromethaneBothHands() {
        for (const bool swap : {false, true}) {
            Molecule m;
            Frame f;
            const int c = addAt(m, f, 0, 0);
            const int br = addAt(m, f, 0, -50, "Br");
            const int cl = addAt(m, f, swap ? 45 : -45, 25, "Cl");
            const int fl = addAt(m, f, swap ? -45 : 45, 25, "F");
            bond(m, c, br, W); bond(m, c, cl); bond(m, c, fl);
            QCOMPARE(labelFor(m, c), swap ? QString("R") : QString("S"));
        }
    }

    void explicitHydrogenAgreesWithImplicit() {
        // Br toward the viewer, H at the bottom in the plane, Cl left, F right.
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), br = addAt(m, f, 0, -50, "Br");
        const int cl = addAt(m, f, -50, 0, "Cl"), fl = addAt(m, f, 50, 0, "F"), h = addAt(m, f, 0, 50, "H");
        bond(m, c, br, W); bond(m, c, cl); bond(m, c, fl); bond(m, c, h);
        QCOMPARE(labelFor(m, c), QString("R"));
        // Same picture with the wedge inverted flips the hand.
        m.setBondStyle(1, D);
        QCOMPARE(labelFor(m, c), QString("S"));
    }

    void isotopeBreaksTheTie() {
        // OH > CH3 > D > H, O top, CH3 lower right, D lower left, H behind: (R).
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), o = addAt(m, f, 0, -50, "O");
        const int me = addAt(m, f, 45, 25), d = addAt(m, f, -45, 25, "H");
        m.setAtomIsotope(d, 2);
        bond(m, c, o, W); bond(m, c, me); bond(m, c, d);
        QCOMPARE(labelFor(m, c), QString("R"));
    }

    void ringStereocentreIsRanked() {
        // 2-methylcyclopentanone-like centre: ring CH(CH3) bonded to C=O and CH2.
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), me = addAt(m, f, 0, -50);
        const int co = addAt(m, f, 45, 25), o = addAt(m, f, 90, 0, "O"), ch2 = addAt(m, f, -45, 25);
        const int ch3 = addAt(m, f, -90, 0);
        bond(m, c, me, W); bond(m, c, co); bond(m, c, ch2); bond(m, co, o, P, 2); bond(m, ch2, ch3);
        // C(=O) > CH2 > CH3 > H: lower right -> lower left -> top is clockwise: (R).
        QCOMPARE(labelFor(m, c), QString("R"));
    }

    void butene_data() {
        QTest::addColumn<double>("c4y");
        QTest::addColumn<QString>("expected");
        QTest::newRow("trans") << 40.0 << "E";
        QTest::newRow("cis") << -40.0 << "Z";
    }
    void butene() {
        QFETCH(double, c4y); QFETCH(QString, expected);
        Molecule m;
        Frame f;
        const int c2 = addAt(m, f, 0, 0), c3 = addAt(m, f, 50, 0);
        const int c1 = addAt(m, f, -25, -40), c4 = addAt(m, f, 75, c4y);
        const int db = bond(m, c2, c3, P, 2);
        bond(m, c2, c1); bond(m, c3, c4);
        QCOMPARE(labelFor(m, -1, db), expected);
    }

    void butenRotated() {
        for (double angle : {30.0, 123.0, 250.0}) {
            Molecule m;
            Frame f; f.angle = angle;
            const int c2 = addAt(m, f, 0, 0), c3 = addAt(m, f, 50, 0);
            const int c1 = addAt(m, f, -25, -40), c4 = addAt(m, f, 75, 40);
            const int db = bond(m, c2, c3, P, 2);
            bond(m, c2, c1); bond(m, c3, c4);
            QCOMPARE(labelFor(m, -1, db), QString("E"));
        }
    }

    void bromochloropropene_data() {
        QTest::addColumn<double>("methylY");
        QTest::addColumn<QString>("expected");
        QTest::newRow("Z") << -40.0 << "Z"; // CH3 on the same side as Br
        QTest::newRow("E") << 40.0 << "E";
    }
    void bromochloropropene() {
        QFETCH(double, methylY); QFETCH(QString, expected);
        Molecule m;
        Frame f;
        const int c1 = addAt(m, f, 0, 0), c2 = addAt(m, f, 50, 0);
        const int br = addAt(m, f, -25, -40, "Br"), cl = addAt(m, f, -25, 40, "Cl");
        const int me = addAt(m, f, 75, methylY);
        const int db = bond(m, c1, c2, P, 2);
        bond(m, c1, br); bond(m, c1, cl); bond(m, c2, me);
        QCOMPARE(labelFor(m, -1, db), expected);
    }

    void deeperRankingDecidesEthylVersusMethyl() {
        // CH3-CH=C(CH3)CH2CH3 with both methyls up: ethyl (higher) is down, so E.
        Molecule m;
        Frame f;
        const int c2 = addAt(m, f, 0, 0), c3 = addAt(m, f, 50, 0);
        const int a = addAt(m, f, -25, -40);
        const int me = addAt(m, f, 75, -40), et = addAt(m, f, 75, 40), et2 = addAt(m, f, 100, 80);
        const int db = bond(m, c2, c3, P, 2);
        bond(m, c2, a); bond(m, c3, me); bond(m, c3, et); bond(m, et, et2);
        QCOMPARE(labelFor(m, -1, db), QString("E"));
    }

    void isopropylOutranksEthylAtSecondSphere() {
        // Br-C(=)-... use C=C with H / ethyl on one end and H / isopropyl on... keep both ends two-way:
        // CH3CH2-C(Cl)=C(Br)-CH(CH3)2 is not needed; test the comparison via an end bearing Et and iPr.
        Molecule m;
        Frame f;
        const int c1 = addAt(m, f, 0, 0), c2 = addAt(m, f, 50, 0);
        const int et = addAt(m, f, -25, -40), et2 = addAt(m, f, -50, -80);
        const int ipr = addAt(m, f, -25, 40), m1 = addAt(m, f, -50, 80), m2 = addAt(m, f, 0, 80);
        const int cl = addAt(m, f, 75, -40, "Cl");
        const int db = bond(m, c1, c2, P, 2);
        bond(m, c1, et); bond(m, et, et2); bond(m, c1, ipr); bond(m, ipr, m1); bond(m, ipr, m2); bond(m, c2, cl);
        // iPr (down) outranks Et (up); Cl on the other end is up (higher than H): opposite sides -> E.
        QCOMPARE(labelFor(m, -1, db), QString("E"));
    }

    void identicalEndSubstituentsGiveNone() {
        Molecule m;
        Frame f;
        const int c1 = addAt(m, f, 0, 0), c2 = addAt(m, f, 50, 0);
        const int a = addAt(m, f, -25, -40), b = addAt(m, f, -25, 40), c = addAt(m, f, 75, -40);
        const int db = bond(m, c1, c2, P, 2);
        bond(m, c1, a); bond(m, c1, b); bond(m, c2, c);
        QVERIFY(labelFor(m, -1, db).isEmpty());
    }

    void wavyDoubleBondGivesNone() {
        Molecule m;
        Frame f;
        const int c2 = addAt(m, f, 0, 0), c3 = addAt(m, f, 50, 0);
        const int c1 = addAt(m, f, -25, -40), c4 = addAt(m, f, 75, 40);
        const int db = bond(m, c2, c3, "wavy", 2);
        bond(m, c2, c1); bond(m, c3, c4);
        QVERIFY(labelFor(m, -1, db).isEmpty());
    }

    void ringDoubleBondGivesNone() {
        Molecule m;
        QVERIFY(m.stampRing("cyclohexane", 300, 300));
        const QVector<Bond> bonds = m.bondData();
        QVERIFY(!bonds.isEmpty());
        m.cycleBondOrder(bonds.first().id);
        QCOMPARE(m.bondData().first().order, 2);
        QVERIFY(Stereo::compute(m).isEmpty());
    }

    void abbreviationRanksByFirstAtom() {
        QCOMPARE(Stereo::rankingAtomicNumber("OMe"), 8);
        QCOMPARE(Stereo::rankingAtomicNumber("Cl"), 17);
        QCOMPARE(Stereo::rankingAtomicNumber("NHBoc"), 7);
        QCOMPARE(Stereo::rankingAtomicNumber("Xx"), 0 + 0);
    }

    void invokableReturnsLabelsAndPositions() {
        Molecule m;
        Frame f;
        const int c = addAt(m, f, 0, 0), br = addAt(m, f, 0, -50, "Br");
        const int cl = addAt(m, f, -45, 25, "Cl"), fl = addAt(m, f, 45, 25, "F");
        bond(m, c, br, W); bond(m, c, cl); bond(m, c, fl);
        const QVariantList list = m.stereoDescriptors();
        QCOMPARE(list.size(), 1);
        const QVariantMap d = list.first().toMap();
        QCOMPARE(d.value("atomId").toInt(), c);
        QCOMPARE(d.value("text").toString(), QString("(S)"));
        // The label sits below the centre, away from the drawn bonds.
        QVERIFY(d.value("y").toDouble() > f.cy);
    }
};

QTEST_MAIN(StereoTest)
#include "test_stereo.moc"
