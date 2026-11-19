#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/SvgWriter.h"
#include "platform/PanZoomGesture.h"
#include "platform/TwoFingerTap.h"

#include <QTemporaryDir>
#include <QtTest>

class InteractionTests : public QObject {
    Q_OBJECT

private:
    static int doubleBond(Molecule &molecule) {
        const int a = molecule.addAtom(0, 0), b = molecule.addAtom(50, 0);
        return molecule.addBond(a, b, 2);
    }

private slots:
    void doubleBondSideCycles() {
        Molecule molecule;
        const int bond = doubleBond(molecule);
        QCOMPARE(molecule.bondData().first().doubleSide, 0);
        QVERIFY(molecule.cycleBondStyle(bond));
        QCOMPARE(molecule.bondData().first().doubleSide, 1);
        QVERIFY(molecule.cycleBondStyle(bond));
        QCOMPARE(molecule.bondData().first().doubleSide, -1);
        QVERIFY(molecule.cycleBondStyle(bond));
        QCOMPARE(molecule.bondData().first().doubleSide, 0);
        QCOMPARE(molecule.bondData().first().style, QStringLiteral("plain"));
        const int c = molecule.addAtom(0, 60), d = molecule.addAtom(50, 60);
        const int single = molecule.addBond(c, d, 1);
        QVERIFY(molecule.cycleBondStyle(single));
        QCOMPARE(molecule.bondData().last().style, QStringLiteral("wedge"));
        QCOMPARE(molecule.bondData().last().doubleSide, 0);
    }

    void doubleBondSideUndoCopyAndSvg() {
        Molecule molecule;
        const int bond = doubleBond(molecule);
        const QString centred = SvgWriter::write(molecule, {});
        molecule.cycleBondStyle(bond);
        const QString offset = SvgWriter::write(molecule, {});
        molecule.cycleBondStyle(bond);
        const QString other = SvgWriter::write(molecule, {});
        QVERIFY(centred != offset);
        QVERIFY(offset != other);
        QVERIFY(centred != other);
        QVERIFY(offset.contains(QStringLiteral("x1=\"0\" y1=\"0\" x2=\"50\" y2=\"0\"")));
        QVERIFY(!centred.contains(QStringLiteral("x1=\"0\" y1=\"0\" x2=\"50\" y2=\"0\"")));

        QVERIFY(molecule.undo());
        QCOMPARE(molecule.bondData().first().doubleSide, 1);

        QVERIFY(molecule.copyAtoms(QVariantList{molecule.atomData()[0].id, molecule.atomData()[1].id}));
        molecule.pasteAtoms(0, 200);
        QCOMPARE(molecule.bondData().size(), 2);
        QCOMPARE(molecule.bondData().last().doubleSide, 1);
    }

    void doubleBondSideRoundTripsThroughRcd() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int first = doubleBond(molecule);
        const int c = molecule.addAtom(0, 60), d = molecule.addAtom(50, 60);
        const int second = molecule.addBond(c, d, 2);
        molecule.cycleBondStyle(first);
        molecule.cycleBondStyle(second);
        molecule.cycleBondStyle(second); // -1
        const QString path = directory.filePath(QStringLiteral("sides.rcd"));
        QVERIFY(layer.saveToFile(path));

        Molecule loaded;
        ReactionLayer loadedLayer(&loaded);
        QVERIFY(loadedLayer.loadFromFile(path));
        QCOMPARE(loaded.bondData().size(), 2);
        QCOMPARE(loaded.bondData()[0].doubleSide, 1);
        QCOMPARE(loaded.bondData()[1].doubleSide, -1);

        Molecule plain;
        ReactionLayer plainLayer(&plain);
        doubleBond(plain);
        const QString oldPath = directory.filePath(QStringLiteral("plain.rcd"));
        QVERIFY(plainLayer.saveToFile(oldPath));
        QFile file(oldPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains("bondSides"));
        file.close();
        QVERIFY(loadedLayer.loadFromFile(oldPath));
        QCOMPARE(loaded.bondData().first().doubleSide, 0);
    }

    void lassoSelectsInsideConcavePolygon() {
        Molecule molecule;
        QVariantList polygon;
        for (const QPointF p : {QPointF(0, 0), QPointF(30, 0), QPointF(30, 70), QPointF(70, 70), QPointF(70, 0),
                                QPointF(100, 0), QPointF(100, 100), QPointF(0, 100)})
            polygon.append(QVariantMap{{QStringLiteral("x"), p.x()}, {QStringLiteral("y"), p.y()}});
        const int leftArm = molecule.addAtom(15, 20);
        const int rightArm = molecule.addAtom(85, 20);
        const int bottom = molecule.addAtom(50, 85);
        molecule.addAtom(50, 30);   // in the notch: outside
        molecule.addAtom(120, 50);
        molecule.addAtom(50, -10);
        const QVariantList expected{leftArm, rightArm, bottom};
        QCOMPARE(molecule.atomsInPolygon(polygon), expected);
        QVERIFY(molecule.atomsInPolygon({}).isEmpty());
    }

    void duplicateIsOneOffsetUndoStep() {
        Molecule molecule;
        const int a = molecule.addAtom(10, 10), b = molecule.addAtom(60, 10);
        const int bond = molecule.addBond(a, b, 2);
        molecule.cycleBondStyle(bond);
        molecule.addAtom(500, 500);
        const QVariantList created = molecule.duplicateAtoms(QVariantList{a, b}, 80, 90);
        QCOMPARE(created.size(), 2);
        QCOMPARE(molecule.atomData().size(), 5);
        QCOMPARE(molecule.bondData().size(), 2);
        QCOMPARE(molecule.bondData().last().doubleSide, 1);
        const QVariantMap copy = molecule.atoms()[3].toMap();
        QCOMPARE(copy.value(QStringLiteral("x")).toDouble(), 90.0);
        QCOMPARE(copy.value(QStringLiteral("y")).toDouble(), 100.0);
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().size(), 3);
        QCOMPARE(molecule.bondData().size(), 1);
        QVERIFY(molecule.duplicateAtoms({}, 1, 1).isEmpty());
    }

    void tapsCountFingers() {
        using Phase = TwoFingerTap::Phase;
        TwoFingerTap tap;
        const QPointF p(500, 500);
        QCOMPARE(tap.feed(1, Phase::Press, p, 0), 0);
        QCOMPARE(tap.feed(2, Phase::Press, p + QPointF(80, 0), 40), 0);
        QCOMPARE(tap.feed(1, Phase::Release, p, 150), 0);
        QCOMPARE(tap.feed(2, Phase::Release, p, 160), 2);

        QCOMPARE(tap.feed(1, Phase::Press, p, 1000), 0);
        QCOMPARE(tap.feed(2, Phase::Press, p + QPointF(80, 0), 1030), 0);
        QCOMPARE(tap.feed(3, Phase::Press, p + QPointF(160, 0), 1060), 0);
        QCOMPARE(tap.feed(2, Phase::Release, p, 1150), 0);
        QCOMPARE(tap.feed(3, Phase::Release, p, 1160), 0);
        QCOMPARE(tap.feed(1, Phase::Release, p, 1170), 3);

        // Four fingers, and a slow third finger, are not taps.
        for (int id = 1; id <= 4; ++id) QCOMPARE(tap.feed(id, Phase::Press, p, 2000 + id * 10), 0);
        for (int id = 1; id <= 4; ++id) QCOMPARE(tap.feed(id, Phase::Release, p, 2100 + id), 0);
        QCOMPARE(tap.feed(1, Phase::Press, p, 3000), 0);
        QCOMPARE(tap.feed(2, Phase::Press, p, 3010), 0);
        QCOMPARE(tap.feed(3, Phase::Press, p, 3400), 0);
        for (int id = 1; id <= 3; ++id) QCOMPARE(tap.feed(id, Phase::Release, p, 3450 + id), 0);
    }

    void panZoomMath() {
        using Phase = PanZoomGesture::Phase;
        const QRectF canvas(300, 0, 1300, 2000);
        PanZoomGesture gesture;
        QVERIFY(!gesture.feed(1, Phase::Press, QPointF(600, 800), canvas, false).valid);
        QVERIFY(!gesture.feed(2, Phase::Press, QPointF(800, 800), canvas, false).valid);
        QVERIFY(!gesture.feed(2, Phase::Move, QPointF(805, 800), canvas, false).valid);
        QVERIFY(!gesture.engaged());
        // Right finger to x = 1000: distance 200 -> 400; the first step covers all movement so far.
        PanZoomGesture::Step step = gesture.feed(2, Phase::Move, QPointF(1000, 800), canvas, false);
        QVERIFY(step.valid);
        QVERIFY(gesture.engaged());
        QCOMPARE(step.scale, 2.0);
        QCOMPARE(step.center, QPointF(700, 800));
        QCOMPARE(step.dx, 100.0);
        QCOMPARE(step.dy, 0.0);
        // Moving both fingers down by 50 is a pure pan (reported as two steps of 25).
        step = gesture.feed(1, Phase::Move, QPointF(600, 850), canvas, false);
        QVERIFY(step.valid);
        QCOMPARE(step.dy, 25.0);
        step = gesture.feed(2, Phase::Move, QPointF(1000, 850), canvas, false);
        QVERIFY(step.valid);
        QVERIFY(qAbs(step.scale - 1.0) < 0.02); // finger-by-finger steps: ratio near 1
        QCOMPARE(step.dy, 25.0);
    }

    void panZoomIgnoresOffCanvasPenAndThirdFinger() {
        using Phase = PanZoomGesture::Phase;
        const QRectF canvas(300, 0, 1300, 2000);
        PanZoomGesture gesture;
        gesture.feed(1, Phase::Press, QPointF(600, 800), canvas, false);
        gesture.feed(2, Phase::Press, QPointF(100, 800), canvas, false); // on the menu
        QVERIFY(!gesture.feed(1, Phase::Move, QPointF(700, 800), canvas, false).valid);
        QVERIFY(!gesture.feed(2, Phase::Move, QPointF(200, 800), canvas, false).valid);
        gesture.feed(1, Phase::Release, QPointF(700, 800), canvas, false);
        gesture.feed(2, Phase::Release, QPointF(200, 800), canvas, false);

        gesture.feed(1, Phase::Press, QPointF(600, 800), canvas, true);
        gesture.feed(2, Phase::Press, QPointF(800, 800), canvas, true);
        QVERIFY(!gesture.feed(2, Phase::Move, QPointF(1000, 800), canvas, true).valid);
        gesture.feed(1, Phase::Release, QPointF(600, 800), canvas, true);
        gesture.feed(2, Phase::Release, QPointF(1000, 800), canvas, true);

        gesture.feed(1, Phase::Press, QPointF(600, 800), canvas, false);
        gesture.feed(2, Phase::Press, QPointF(800, 800), canvas, false);
        gesture.feed(3, Phase::Press, QPointF(900, 900), canvas, false);
        QVERIFY(!gesture.feed(2, Phase::Move, QPointF(1000, 800), canvas, false).valid);
    }

    void pinchIsNotATap() {
        using Phase = PanZoomGesture::Phase;
        const QRectF canvas(300, 0, 1300, 2000);
        PanZoomGesture gesture;
        TwoFingerTap tap;
        int taps = 0;
        const auto feed = [&](int id, Phase phase, QPointF pos, qint64 t) {
            gesture.feed(id, phase, pos, canvas, false);
            if (tap.feed(id, static_cast<TwoFingerTap::Phase>(phase), pos, t) && !gesture.engaged()) ++taps;
        };
        // A 30 px pan is under the tap tolerance but still engages the pan.
        feed(1, Phase::Press, QPointF(600, 800), 0);
        feed(2, Phase::Press, QPointF(800, 800), 30);
        feed(1, Phase::Move, QPointF(600, 830), 100);
        feed(2, Phase::Move, QPointF(800, 830), 110);
        feed(1, Phase::Release, QPointF(600, 830), 200);
        feed(2, Phase::Release, QPointF(800, 830), 210);
        QCOMPARE(taps, 0);
        feed(1, Phase::Press, QPointF(600, 800), 1000);
        feed(2, Phase::Press, QPointF(800, 800), 1030);
        feed(1, Phase::Release, QPointF(600, 800), 1100);
        feed(2, Phase::Release, QPointF(800, 800), 1110);
        QCOMPARE(taps, 1);
    }
};

QTEST_MAIN(InteractionTests)
#include "test_interaction.moc"
