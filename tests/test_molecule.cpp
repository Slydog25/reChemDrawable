#include "core/EditHistory.h"
#include "core/GeometryEngine.h"
#include "core/MolfileReader.h"
#include "core/MolfileWriter.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/SvgWriter.h"

#include <QLineF>
#include <QtTest>
#include <QTemporaryDir>

class MoleculeTest : public QObject {
    Q_OBJECT
private slots:
    void usesStableIdsAndUndo() {
        Molecule molecule;
        const int first = molecule.addAtom(10, 10);
        const int second = molecule.addAtom(58, 10);
        const int bond = molecule.addBond(first, second);
        QCOMPARE(bond, 1);
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.bonds().size(), 0);
        QVERIFY(molecule.redo());
        QCOMPARE(molecule.bonds().size(), 1);
    }
    void guardsBasicValence() {
        Molecule molecule;
        const int carbon = molecule.addAtom(0, 0);
        for (int index = 0; index < 4; ++index) {
            const int hydrogen = molecule.addAtom(50 + index, 0, "H");
            QVERIFY(molecule.addBond(carbon, hydrogen) >= 0);
        }
        const int extra = molecule.addAtom(99, 0, "H");
        QCOMPARE(molecule.addBond(carbon, extra), -1);
    }
    void roundTripsMolfileCoordinates() {
        Molecule source;
        const int carbon = source.addAtom(48, 96);
        const int oxygen = source.addAtom(96, 96, "O");
        source.addBond(carbon, oxygen, 2);
        Molecule loaded;
        QString error;
        QVERIFY(MolfileReader::readV2000(MolfileWriter::writeV2000(source), &loaded, &error));
        QCOMPARE(loaded.atoms().size(), 2);
        QCOMPARE(loaded.bonds().size(), 1);
        QCOMPARE(loaded.atoms().at(1).toMap().value("element").toString(), QString("O"));
    }
    void savesAndLoadsMolfile() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("test.mol");
        Molecule source;
        const int carbon = source.addAtom(0, 0);
        const int oxygen = source.addAtom(48, 0, "O");
        source.addBond(carbon, oxygen);
        QVERIFY(source.saveToFile(path));

        Molecule loaded;
        QVERIFY(loaded.loadFromFile(path));
        QCOMPARE(loaded.atoms().size(), 2);
        QCOMPARE(loaded.bonds().size(), 1);
    }
    void ringTemplatesUseStandardBondLengths() {
        Molecule molecule;
        QVERIFY(molecule.stampRing("cyclopentane", 100, 100));
        QCOMPARE(molecule.atomData().size(), 5);
        for (const Bond &bond : molecule.bondData()) {
            const Atom &a = molecule.atomData().at(bond.atomA - 1);
            const Atom &b = molecule.atomData().at(bond.atomB - 1);
            QVERIFY(qAbs(QLineF(QPointF(a.x, a.y), QPointF(b.x, b.y)).length() - GeometryEngine::StandardBondLength) < 0.001);
        }
    }
    void ringCanAttachToSelectedAtom() {
        Molecule molecule;
        const int anchor = molecule.addAtom(100, 100);
        QVERIFY(molecule.stampRingAtAtom("cyclohexane", anchor, 200, 100));
        QCOMPARE(molecule.atomData().size(), 6);
        QCOMPARE(molecule.bondData().size(), 6);
        QCOMPARE(molecule.atomData().first().id, anchor);
    }
    void ringCanAttachToExistingBond() {
        Molecule molecule;
        const int first = molecule.addAtom(0, 0);
        const int second = molecule.addAtom(GeometryEngine::StandardBondLength, 0);
        const int sharedBond = molecule.addBond(first, second);

        QVERIFY(molecule.stampRingAtBond("cyclohexane", sharedBond,
                                         GeometryEngine::StandardBondLength / 2.0, 100));
        QCOMPARE(molecule.atomData().size(), 6);
        QCOMPARE(molecule.bondData().size(), 6);
        QCOMPARE(molecule.bondData().at(0).id, sharedBond);
        QCOMPARE(molecule.bondData().at(0).atomA, first);
        QCOMPARE(molecule.bondData().at(0).atomB, second);

        Molecule benzene;
        const int benzeneFirst = benzene.addAtom(0, 0);
        const int benzeneSecond = benzene.addAtom(GeometryEngine::StandardBondLength, 0);
        const int benzeneSharedBond = benzene.addBond(benzeneFirst, benzeneSecond);
        QVERIFY(benzene.stampRingAtBond("benzene", benzeneSharedBond,
                                        GeometryEngine::StandardBondLength / 2.0, 100));
        QCOMPARE(benzene.bondData().at(0).style, QString("benzene"));
        QCOMPARE(benzene.bondData().at(0).order, 2);
    }
    void suggestsAnOpenBondDirection() {
        Molecule molecule;
        const int center = molecule.addAtom(0, 0);
        const int right = molecule.addAtom(GeometryEngine::StandardBondLength, 0);
        QVERIFY(molecule.addBond(center, right) >= 0);
        const QPointF suggested = molecule.suggestedBondEnd(center);
        QVERIFY(qAbs(suggested.x() + GeometryEngine::StandardBondLength) < 0.001);
        QVERIFY(qAbs(suggested.y()) < 0.001);
    }
    void storesNotationAndCalculatesBasicProperties() {
        Molecule molecule;
        const int carbon = molecule.addAtom(0, 0);
        const int oxygen = molecule.addAtom(48, 0, "O");
        const int bond = molecule.addBond(carbon, oxygen);
        QVERIFY(molecule.setAtomCharge(oxygen, -1));
        QVERIFY(molecule.setAtomIsotope(carbon, 13));
        QVERIFY(molecule.setBondStyle(bond, "wedge"));
        QCOMPARE(molecule.bonds().first().toMap().value("style").toString(), QString("wedge"));
        const QVariantMap values = molecule.properties();
        QVERIFY(values.value("formula").toString().contains("C"));
        QVERIFY(values.value("molecularWeight").toDouble() > 25.0);

        Molecule loaded;
        QString error;
        QVERIFY(MolfileReader::readV2000(MolfileWriter::writeV2000(molecule), &loaded, &error));
        QCOMPARE(loaded.atoms().at(0).toMap().value("isotope").toInt(), 13);
        QCOMPARE(loaded.atoms().at(1).toMap().value("charge").toInt(), -1);
        QCOMPARE(loaded.bonds().at(0).toMap().value("style").toString(), QString("wedge"));
        QVERIFY(SvgWriter::write(molecule).contains("<svg"));
    }
    void stampsFunctionalGroupsAndCleansBondLengths() {
        Molecule molecule;
        const int anchor = molecule.addAtom(0, 0);
        QVERIFY(molecule.stampFunctionalGroup("COOH", anchor));
        QCOMPARE(molecule.atomData().size(), 4);
        QCOMPARE(molecule.bondData().size(), 3);
        QVERIFY(molecule.moveAtom(anchor, -200, 0));
        QVERIFY(molecule.cleanUpStructure());
        for (const Bond &bond : molecule.bondData()) {
            const Atom *a = nullptr;
            const Atom *b = nullptr;
            for (const Atom &atom : molecule.atomData()) {
                if (atom.id == bond.atomA) a = &atom;
                if (atom.id == bond.atomB) b = &atom;
            }
            QVERIFY(a && b);
            QVERIFY(qAbs(QLineF(QPointF(a->x, a->y), QPointF(b->x, b->y)).length() - GeometryEngine::StandardBondLength) < 1.0);
        }
    }
    void stampsCommonFragmentTemplates() {
        Molecule molecule;
        const int anchor = molecule.addAtom(0, 0);
        QVERIFY(molecule.stampFunctionalGroup("CN", anchor));
        QCOMPARE(molecule.atomData().size(), 3);

        Molecule phenylMolecule;
        const int phenylAnchor = phenylMolecule.addAtom(0, 0);
        QVERIFY(phenylMolecule.stampFunctionalGroup("Ph", phenylAnchor));
        QCOMPARE(phenylMolecule.atomData().size(), 7);
        QCOMPARE(phenylMolecule.bondData().size(), 7);

        Molecule tertButylMolecule;
        const int tertButylAnchor = tertButylMolecule.addAtom(0, 0);
        QVERIFY(tertButylMolecule.stampFunctionalGroup("tBu", tertButylAnchor));
        QCOMPARE(tertButylMolecule.atomData().size(), 5);
        QCOMPARE(tertButylMolecule.bondData().size(), 4);
    }
    void transformsAndDeletesSelections() {
        Molecule molecule;
        const int first = molecule.addAtom(0, 0);
        const int second = molecule.addAtom(48, 0);
        molecule.addBond(first, second);
        const QVariantList selection = {first, second};
        QVERIFY(molecule.translateAtoms(selection, 10, 20));
        QCOMPARE(molecule.atomData().at(0).x, 10.0);
        QVERIFY(molecule.rotateAtoms(selection, 180));
        QVERIFY(molecule.mirrorAtoms(selection, true));
        QVERIFY(molecule.alignAtoms(selection, "horizontal"));
        QVERIFY(molecule.removeAtoms(selection));
        QCOMPARE(molecule.atomData().size(), 0);
        QCOMPARE(molecule.bondData().size(), 0);
    }
    void copiesAndPastesSelectedFragments() {
        Molecule molecule;
        const int first = molecule.addAtom(0, 0);
        const int second = molecule.addAtom(48, 0, "O");
        molecule.addBond(first, second, 2);
        QVERIFY(molecule.copyAtoms({first, second}));
        const QVariantList pasted = molecule.pasteAtoms(200, 100);
        QCOMPARE(pasted.size(), 2);
        QCOMPARE(molecule.atomData().size(), 4);
        QCOMPARE(molecule.bondData().size(), 2);
        QCOMPARE(molecule.atomData().at(2).x, 176.0);
        QCOMPARE(molecule.atomData().at(3).element, QString("O"));
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().size(), 2);
    }
    void savesReactionSchemesWithAnnotations() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("scheme.rcd");
        Molecule source;
        source.addAtom(10, 10);
        ReactionLayer layer(&source);
        QVERIFY(layer.addArrow(40, 10, 120, 10) >= 0);
        QVERIFY(layer.addLabel("heat", 80, -20) >= 0);
        QVERIFY(layer.translateItem(1, 10, 0));
        QVERIFY(layer.saveToFile(path));

        Molecule loaded;
        ReactionLayer loadedLayer(&loaded);
        QVERIFY(loadedLayer.loadFromFile(path));
        QCOMPARE(loaded.atoms().size(), 1);
        QCOMPARE(loadedLayer.items().size(), 2);
        QVERIFY(SvgWriter::write(loaded, loadedLayer.items()).contains("polyline"));
    }
    void undoCoversMoleculeAndAnnotationsInOrder() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        EditHistory history(&molecule, &layer);
        molecule.addAtom(0, 0);
        QVERIFY(layer.addArrow(40, 0, 120, 0) >= 0);
        molecule.addAtom(200, 0);

        QVERIFY(history.undo());
        QCOMPARE(molecule.atomData().size(), 1);
        QCOMPARE(layer.items().size(), 1);
        QVERIFY(history.undo());
        QCOMPARE(layer.items().size(), 0);
        QCOMPARE(molecule.atomData().size(), 1);
        QVERIFY(history.undo());
        QCOMPARE(molecule.atomData().size(), 0);
        QVERIFY(!history.undo());

        QVERIFY(history.redo());
        QCOMPARE(molecule.atomData().size(), 1);
        QVERIFY(history.redo());
        QCOMPARE(layer.items().size(), 1);
        QVERIFY(history.redo());
        QCOMPARE(molecule.atomData().size(), 2);
        QVERIFY(!history.redo());
    }
    void clearAllUndoesInOneStep() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        EditHistory history(&molecule, &layer);
        molecule.addAtom(0, 0);
        QVERIFY(layer.addLabel("heat", 80, -20) >= 0);
        history.clearAll();
        QCOMPARE(molecule.atomData().size(), 0);
        QCOMPARE(layer.items().size(), 0);
        QVERIFY(history.undo());
        QCOMPARE(molecule.atomData().size(), 1);
        QCOMPARE(layer.items().size(), 1);
    }
    void loadingASchemeResetsSharedHistory() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("scheme.rcd");
        Molecule molecule;
        ReactionLayer layer(&molecule);
        EditHistory history(&molecule, &layer);
        molecule.addAtom(0, 0);
        QVERIFY(layer.addArrow(40, 0, 120, 0) >= 0);
        QVERIFY(layer.saveToFile(path));
        QVERIFY(layer.loadFromFile(path));
        QVERIFY(!history.undo());
        QCOMPARE(molecule.atomData().size(), 1);
        QCOMPARE(layer.items().size(), 1);
    }
    void valenceLimitsFollowFormalCharge() {
        Molecule molecule;
        const int nitrogen = molecule.addAtom(0, 0, "N");
        for (int index = 0; index < 3; ++index)
            QVERIFY(molecule.addBond(nitrogen, molecule.addAtom(48 * (index + 1), 0)) >= 0);
        const int fourth = molecule.addAtom(0, 48);
        QCOMPARE(molecule.addBond(nitrogen, fourth), -1);
        QVERIFY(molecule.setAtomCharge(nitrogen, 1));
        QVERIFY(molecule.addBond(nitrogen, fourth) >= 0);
        QVERIFY(!molecule.setAtomCharge(nitrogen, 0));

        Molecule ether;
        const int oxygen = ether.addAtom(0, 0, "O");
        ether.addBond(oxygen, ether.addAtom(48, 0));
        ether.addBond(oxygen, ether.addAtom(-48, 0));
        QVERIFY(!ether.setAtomCharge(oxygen, -1));

        Molecule ammonium;
        QVERIFY(ammonium.setAtomCharge(ammonium.addAtom(0, 0, "N"), 1));
        QCOMPARE(ammonium.formula(), QString("H4N+"));
        Molecule hydroxide;
        QVERIFY(hydroxide.setAtomCharge(hydroxide.addAtom(0, 0, "O"), -1));
        QCOMPARE(hydroxide.formula(), QString("HO-"));
        Molecule methylCation;
        QVERIFY(methylCation.setAtomCharge(methylCation.addAtom(0, 0), 1));
        QCOMPARE(methylCation.formula(), QString("CH3+"));
    }
    void preservesWavyBondsInMolfiles() {
        Molecule source;
        const int a = source.addAtom(0, 0);
        const int b = source.addAtom(48, 0);
        const int c = source.addAtom(96, 0);
        QVERIFY(source.setBondStyle(source.addBond(a, b), "wavy"));
        QVERIFY(source.setBondStyle(source.addBond(b, c, 2), "wavy"));
        const QString molfile = MolfileWriter::writeV2000(source);
        QVERIFY(molfile.contains("  1  2  1  4"));
        QVERIFY(molfile.contains("  2  3  2  3"));

        Molecule loaded;
        QString error;
        QVERIFY(MolfileReader::readV2000(molfile, &loaded, &error));
        QCOMPARE(loaded.bondData().at(0).style, QString("wavy"));
        QCOMPARE(loaded.bondData().at(1).style, QString("wavy"));
        QCOMPARE(loaded.bondData().at(1).order, 2);
    }
};

QTEST_MAIN(MoleculeTest)
#include "test_molecule.moc"
