// Every edit must be fully undoable. Several edits used to take a pointer into
// the atom/bond list before the undo snapshot, and Qt's implicitly shared
// containers then wrote the change into the snapshot too, so Undo restored the
// edited value.
#include "core/GeometryEngine.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"

#include <QtTest>

class UndoTest : public QObject {
    Q_OBJECT

    static QPointF position(const Molecule &molecule, int id) {
        for (const Atom &atom : molecule.atomData()) if (atom.id == id) return QPointF(atom.x, atom.y);
        return QPointF(-1, -1);
    }

private slots:
    void moveAtomUndoes() {
        Molecule molecule;
        const int atom = molecule.addAtom(10, 20);
        QVERIFY(molecule.moveAtom(atom, 100, 200));
        QVERIFY(molecule.undo());
        QCOMPARE(position(molecule, atom), QPointF(10, 20));
    }
    void rotateMirrorAlignUndo() {
        Molecule molecule;
        const int a = molecule.addAtom(0, 0), b = molecule.addAtom(48, 10);
        const QVariantList both = {a, b};
        QVERIFY(molecule.rotateAtoms(both, 90));
        QVERIFY(molecule.undo());
        QCOMPARE(position(molecule, b), QPointF(48, 10));
        QVERIFY(molecule.mirrorAtoms(both, true));
        QVERIFY(molecule.undo());
        QCOMPARE(position(molecule, b), QPointF(48, 10));
        QVERIFY(molecule.alignAtoms(both, "horizontal"));
        QVERIFY(molecule.undo());
        QCOMPARE(position(molecule, b), QPointF(48, 10));
    }
    void bondEditsUndo() {
        Molecule molecule;
        const int a = molecule.addAtom(0, 0), b = molecule.addAtom(48, 0);
        const int bond = molecule.addBond(a, b);
        QVERIFY(molecule.cycleBondOrder(bond));
        QCOMPARE(molecule.bondData().first().order, 2);
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.bondData().first().order, 1);
        QVERIFY(molecule.setBondStyle(bond, "wedge"));
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.bondData().first().style, QString("plain"));
    }
    void atomEditsUndo() {
        Molecule molecule;
        const int atom = molecule.addAtom(0, 0);
        QVERIFY(molecule.setAtomElement(atom, "N"));
        QVERIFY(molecule.setAtomCharge(atom, 1));
        QVERIFY(molecule.setAtomIsotope(atom, 15));
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().first().isotope, 0);
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().first().formalCharge, 0);
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().first().element, QString("C"));
    }
    void ringFusionUndoRestoresSharedBond() {
        Molecule molecule;
        const int a = molecule.addAtom(0, 0), b = molecule.addAtom(GeometryEngine::StandardBondLength, 0);
        const int bond = molecule.addBond(a, b);
        QVERIFY(molecule.stampRingAtBond("benzene", bond, 24, 100));
        QCOMPARE(molecule.bondData().first().order, 2);
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().size(), 2);
        QCOMPARE(molecule.bondData().first().order, 1);
        QCOMPARE(molecule.bondData().first().style, QString("plain"));
    }
    void annotationMoveUndoes() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int arrow = layer.addArrow(0, 0, 100, 0);
        QVERIFY(layer.translateItem(arrow, 30, 40));
        QVERIFY(layer.undo());
        QCOMPARE(layer.items().first().toMap().value("x1").toDouble(), 0.0);
        QCOMPARE(layer.items().first().toMap().value("y2").toDouble(), 0.0);
    }
};

QTEST_MAIN(UndoTest)
#include "test_undo.moc"
