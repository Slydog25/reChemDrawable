#include "core/Document.h"
#include "core/EditHistory.h"
#include "core/GeometryEngine.h"
#include "core/MolfileReader.h"
#include "core/MolfileWriter.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/SvgWriter.h"
#include "platform/InputRouter.h"

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
        // One horizontal bond: the next one leaves at 120 degrees, bending upward.
        Molecule molecule;
        const int center = molecule.addAtom(0, 0);
        const int right = molecule.addAtom(GeometryEngine::StandardBondLength, 0);
        QVERIFY(molecule.addBond(center, right) >= 0);
        const QPointF suggested = molecule.suggestedBondEnd(center);
        QVERIFY(qAbs(suggested.x() + GeometryEngine::StandardBondLength / 2.0) < 0.001);
        QVERIFY(qAbs(suggested.y() + GeometryEngine::StandardBondLength * 0.8660254) < 0.001);
    }
    void tapsGrowZigZagChainsAndBranchAtProperAngles() {
        const double length = GeometryEngine::StandardBondLength;
        const auto position = [](const Molecule &molecule, int id) {
            for (const Atom &atom : molecule.atomData()) if (atom.id == id) return QPointF(atom.x, atom.y);
            return QPointF();
        };
        const auto angleAt = [&position](const Molecule &molecule, int center, int a, int b) {
            const double degrees = QLineF(position(molecule, center), position(molecule, a))
                                       .angleTo(QLineF(position(molecule, center), position(molecule, b)));
            return degrees > 180.0 ? 360.0 - degrees : degrees;
        };
        const auto tap = [](Molecule &molecule, int atom) {
            const QPointF end = molecule.suggestedBondEnd(atom);
            return molecule.extendFromAtom(atom, end.x(), end.y());
        };

        // A lone atom starts up and to the right at 30 degrees.
        Molecule chain;
        QVector<int> ids = {chain.addAtom(0, 0)};
        const QPointF first = chain.suggestedBondEnd(ids.first());
        QVERIFY(qAbs(first.x() - length * 0.8660254) < 0.001);
        QVERIFY(qAbs(first.y() + length / 2.0) < 0.001);

        // Tapping the end atom repeatedly gives a zig-zag: every angle is 120
        // degrees and the atoms alternate up and down.
        for (int step = 0; step < 6; ++step) ids.append(tap(chain, ids.last()));
        for (int index = 1; index + 1 < ids.size(); ++index)
            QVERIFY2(qAbs(angleAt(chain, ids[index], ids[index - 1], ids[index + 1]) - 120.0) < 0.01, qPrintable(QString::number(index)));
        for (int index = 0; index + 2 < ids.size(); ++index)
            QVERIFY(qAbs(position(chain, ids[index]).y() - position(chain, ids[index + 2]).y()) < 0.001);
        QVERIFY(position(chain, ids.last()).x() > position(chain, ids.first()).x() + 4 * length);

        // Growing from the other end keeps zig-zagging, without doubling back.
        const int leftEnd = tap(chain, ids.first());
        QVERIFY(qAbs(angleAt(chain, ids.first(), leftEnd, ids[1]) - 120.0) < 0.01);
        QVERIFY(qAbs(position(chain, leftEnd).y() - position(chain, ids[1]).y()) < 0.001);

        // Tapping a middle atom branches at 120 degrees from both chain bonds.
        const int branch = tap(chain, ids[3]);
        QVERIFY(qAbs(angleAt(chain, ids[3], branch, ids[2]) - 120.0) < 0.01);
        QVERIFY(qAbs(angleAt(chain, ids[3], branch, ids[4]) - 120.0) < 0.01);

        // Next to a triple bond the chain continues straight.
        Molecule alkyne;
        const int a = alkyne.addAtom(0, 0), b = alkyne.addAtom(length, 0);
        QVERIFY(alkyne.addBond(a, b, 3) >= 0);
        QVERIFY(qAbs(angleAt(alkyne, b, a, tap(alkyne, b)) - 180.0) < 0.01);

        // A ring atom's substituent points straight out of the ring.
        Molecule ring;
        QVERIFY(ring.stampRing("cyclohexane", 0, 0));
        const int ringAtom = ring.atomData().first().id;
        const int substituent = tap(ring, ringAtom);
        const QLineF outward(QPointF(0, 0), position(ring, ringAtom));
        const QLineF bond(position(ring, ringAtom), position(ring, substituent));
        QVERIFY(qAbs(outward.angleTo(bond)) < 0.01 || qAbs(outward.angleTo(bond) - 360.0) < 0.01);
    }
    void stampsSubstituentsAlongTheBondDirection() {
        const auto position = [](const Molecule &molecule, int id) {
            for (const Atom &atom : molecule.atomData()) if (atom.id == id) return QPointF(atom.x, atom.y);
            return QPointF();
        };
        const auto angleAt = [&position](const Molecule &molecule, int center, int a, int b) {
            const double degrees = QLineF(position(molecule, center), position(molecule, a))
                                       .angleTo(QLineF(position(molecule, center), position(molecule, b)));
            return degrees > 180.0 ? 360.0 - degrees : degrees;
        };
        // Ethyl chain, then COOH on its end: the carboxyl carbon continues the
        // zig-zag and its three bonds sit at 120 degrees.
        Molecule molecule;
        const int c1 = molecule.addAtom(0, 0);
        const QPointF second = molecule.suggestedBondEnd(c1);
        const int c2 = molecule.extendFromAtom(c1, second.x(), second.y());
        QVERIFY(molecule.stampFunctionalGroup("COOH", c2));
        const QVector<Atom> &atoms = molecule.atomData();
        const int carboxyl = atoms[2].id, oxo = atoms[3].id, hydroxy = atoms[4].id;
        QVERIFY(qAbs(angleAt(molecule, c2, c1, carboxyl) - 120.0) < 0.01);
        QVERIFY(qAbs(angleAt(molecule, carboxyl, c2, oxo) - 120.0) < 0.01);
        QVERIFY(qAbs(angleAt(molecule, carboxyl, c2, hydroxy) - 120.0) < 0.01);
        QVERIFY(qAbs(angleAt(molecule, carboxyl, oxo, hydroxy) - 120.0) < 0.01);
        for (const Bond &bond : molecule.bondData())
            QVERIFY(qAbs(QLineF(position(molecule, bond.atomA), position(molecule, bond.atomB)).length() - GeometryEngine::StandardBondLength) < 0.01);
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
    void countsHydrogensOnSulfurAndPhosphorus() {
        Molecule thiol;
        const int thiolSulfur = thiol.addAtom(48, 0, "S");
        thiol.addBond(thiol.addAtom(0, 0), thiolSulfur);
        QCOMPARE(thiol.formula(), QString("CH4S"));
        QVERIFY(qAbs(thiol.properties().value("molecularWeight").toDouble() - 48.103) < 0.01);

        Molecule sulfoxide;
        const int sulfur = sulfoxide.addAtom(0, 0, "S");
        sulfoxide.addBond(sulfur, sulfoxide.addAtom(-48, 0));
        sulfoxide.addBond(sulfur, sulfoxide.addAtom(48, 0));
        sulfoxide.addBond(sulfur, sulfoxide.addAtom(0, -48, "O"), 2);
        QCOMPARE(sulfoxide.formula(), QString("C2H6OS"));
        sulfoxide.addBond(sulfur, sulfoxide.addAtom(0, 48, "O"), 2);
        QCOMPARE(sulfoxide.formula(), QString("C2H6O2S"));

        Molecule thiolate;
        const int thiolateSulfur = thiolate.addAtom(48, 0, "S");
        thiolate.addBond(thiolate.addAtom(0, 0), thiolateSulfur);
        QVERIFY(thiolate.setAtomCharge(thiolateSulfur, -1));
        QCOMPARE(thiolate.formula(), QString("CH3S-"));

        Molecule phosphine;
        const int phosphineP = phosphine.addAtom(48, 0, "P");
        phosphine.addBond(phosphine.addAtom(0, 0), phosphineP);
        QCOMPARE(phosphine.formula(), QString("CH5P"));

        Molecule phosphoricAcid;
        const int phosphorus = phosphoricAcid.addAtom(0, 0, "P");
        phosphoricAcid.addBond(phosphorus, phosphoricAcid.addAtom(0, -48, "O"), 2);
        for (int index = 0; index < 3; ++index)
            phosphoricAcid.addBond(phosphorus, phosphoricAcid.addAtom(48 * index - 48, 48, "O"));
        QCOMPARE(phosphoricAcid.formula(), QString("H3O4P"));
    }
    void extendsFromAnAtomAsOneUndoStep() {
        Molecule molecule;
        const int carbon = molecule.addAtom(0, 0);
        const int added = molecule.extendFromAtom(carbon, 48, 0);
        QVERIFY(added >= 0);
        QCOMPARE(molecule.bondData().size(), 1);
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().size(), 1);
        QCOMPARE(molecule.bondData().size(), 0);

        Molecule full;
        const int fluorine = full.addAtom(0, 0, "F");
        QVERIFY(full.extendFromAtom(fluorine, 48, 0) >= 0);
        QCOMPARE(full.extendFromAtom(fluorine, -48, 0), -1);
        QCOMPARE(full.atomData().size(), 2);
        // The refused extension must leave nothing behind for Undo to resurrect.
        QVERIFY(full.undo());
        QCOMPARE(full.atomData().size(), 1);
        QVERIFY(full.undo());
        QCOMPARE(full.atomData().size(), 0);
        QVERIFY(!full.undo());
    }
    void stampsNitroWithFormalCharges() {
        Molecule molecule;
        const int anchor = molecule.addAtom(0, 0);
        QVERIFY(molecule.stampFunctionalGroup("NO2", anchor));
        QCOMPARE(molecule.formula(), QString("CH3NO2"));
        int positive = 0, negative = 0;
        for (const Atom &atom : molecule.atomData()) {
            if (atom.element == QStringLiteral("N")) QCOMPARE(atom.formalCharge, 1);
            positive += atom.formalCharge > 0;
            negative += atom.formalCharge < 0;
        }
        QCOMPARE(positive, 1);
        QCOMPARE(negative, 1);
        int nitrogen = -1;
        for (const Atom &atom : molecule.atomData()) if (atom.element == QStringLiteral("N")) nitrogen = atom.id;
        QCOMPARE(molecule.extendFromAtom(nitrogen, 0, 100), -1);
    }
    void zeroMovesDoNotCreateUndoSteps() {
        Molecule molecule;
        const int atom = molecule.addAtom(0, 0);
        QVERIFY(!molecule.translateAtoms({atom}, 0, 0));
        QVERIFY(molecule.undo());
        QCOMPARE(molecule.atomData().size(), 0);

        Molecule host;
        ReactionLayer layer(&host);
        const int arrow = layer.addArrow(0, 0, 100, 0);
        QVERIFY(!layer.translateItem(arrow, 0, 0));
        QVERIFY(layer.undo());
        QCOMPARE(layer.items().size(), 0);
    }
    void readsWindowsLineEndingsAndRejectsV3000() {
        Molecule source;
        const int nitrogen = source.addAtom(0, 0, "N");
        QVERIFY(source.setAtomCharge(nitrogen, 1));
        QString crlf = MolfileWriter::writeV2000(source);
        crlf.replace("\n", "\r\n");
        Molecule loaded;
        QString error;
        QVERIFY(MolfileReader::readV2000(crlf, &loaded, &error));
        QCOMPARE(loaded.atomData().first().formalCharge, 1);

        const QString v3000 = "\n  Other\n\n  0  0  0     0  0            999 V3000\nM  V30 BEGIN CTAB\nM  END\n";
        Molecule rejected;
        QVERIFY(!MolfileReader::readV2000(v3000, &rejected, &error));
        QVERIFY(error.contains("V3000"));
    }
    void exportsWavyBondsAndSizeToSvg() {
        Molecule molecule;
        const int a = molecule.addAtom(0, 0);
        const int b = molecule.addAtom(48, 0);
        QVERIFY(molecule.setBondStyle(molecule.addBond(a, b), "wavy"));
        QCOMPARE(molecule.bondData().first().stereo, 4);
        const QString svg = SvgWriter::write(molecule);
        QVERIFY(svg.contains("<path fill=\"none\" d=\"M0,0 Q"));
        QVERIFY(svg.contains("width=\"112\" height=\"64\""));
    }
    void writesSmiles() {
        Molecule ethanol;
        const int c1 = ethanol.addAtom(0, 0);
        const int c2 = ethanol.extendFromAtom(c1, 48, 0);
        ethanol.extendFromAtom(c2, 96, 0, "O");
        QCOMPARE(ethanol.smiles(), QString("CCO"));

        Molecule acid;
        QVERIFY(acid.stampFunctionalGroup("COOH", acid.addAtom(0, 0)));
        QCOMPARE(acid.smiles(), QString("CC(=O)O"));

        Molecule cyclohexane;
        QVERIFY(cyclohexane.stampRing("cyclohexane", 0, 0));
        QCOMPARE(cyclohexane.smiles(), QString("C1CCCCC1"));

        Molecule benzene;
        QVERIFY(benzene.stampRing("benzene", 0, 0));
        QCOMPARE(benzene.smiles(), QString("C1=CC=CC=C1"));

        Molecule aromatic;
        QVERIFY(aromatic.stampRing("cyclohexane", 0, 0));
        for (const Bond &bond : aromatic.bondData()) QVERIFY(aromatic.setBondStyle(bond.id, "aromatic"));
        QCOMPARE(aromatic.smiles(), QString("c1ccccc1"));
        QCOMPARE(aromatic.formula(), QString("C6H6"));

        Molecule ions;
        QVERIFY(ions.setAtomCharge(ions.addAtom(0, 0, "N"), 1));
        QVERIFY(ions.setAtomIsotope(ions.addAtom(100, 0), 13));
        QVERIFY(ions.setAtomCharge(ions.addAtom(200, 0, "Na"), 1));
        QVERIFY(ions.setAtomCharge(ions.addAtom(300, 0, "Cl"), -1));
        QCOMPARE(ions.smiles(), QString("[NH4+].[13CH4].[Na+].[Cl-]"));
    }
    void handlesMoreElementsAndIsotopesInFormulas() {
        Molecule sodium;
        sodium.addAtom(0, 0, "Na");
        QCOMPARE(sodium.formula(), QString("Na"));
        QVERIFY(qAbs(sodium.properties().value("molecularWeight").toDouble() - 22.99) < 0.01);

        Molecule borohydride;
        QVERIFY(borohydride.setAtomCharge(borohydride.addAtom(0, 0, "B"), -1));
        QCOMPARE(borohydride.formula(), QString("H4B-"));

        Molecule silane;
        silane.addAtom(0, 0, "Si");
        QCOMPARE(silane.formula(), QString("H4Si"));

        Molecule labelled;
        const int carbon = labelled.addAtom(0, 0);
        labelled.extendFromAtom(carbon, 48, 0, "O");
        QVERIFY(labelled.setAtomIsotope(carbon, 13));
        QCOMPARE(labelled.formula(), QString("[13C]H4O"));

        Molecule platinum;
        platinum.addAtom(0, 0, "Pt");
        QCOMPARE(platinum.formula(), QString("Pt"));
        QCOMPARE(platinum.properties().value("unknownElements").toStringList(), QStringList{"Pt"});
    }
    void cleanUpFixesAnglesAndKeepsRingsRegular() {
        const auto angleAt = [](const Molecule &molecule, int centerId, int firstId, int secondId) {
            QPointF center, first, second;
            for (const Atom &atom : molecule.atomData()) {
                if (atom.id == centerId) center = QPointF(atom.x, atom.y);
                if (atom.id == firstId) first = QPointF(atom.x, atom.y);
                if (atom.id == secondId) second = QPointF(atom.x, atom.y);
            }
            return QLineF(center, first).angleTo(QLineF(center, second));
        };
        const auto normalized = [](double degrees) { return degrees > 180.0 ? 360.0 - degrees : degrees; };
        const auto bondsHaveStandardLength = [](const Molecule &molecule) {
            for (const Bond &bond : molecule.bondData()) {
                QPointF a, b;
                for (const Atom &atom : molecule.atomData()) {
                    if (atom.id == bond.atomA) a = QPointF(atom.x, atom.y);
                    if (atom.id == bond.atomB) b = QPointF(atom.x, atom.y);
                }
                if (qAbs(QLineF(a, b).length() - GeometryEngine::StandardBondLength) > 1.0) return false;
            }
            return true;
        };

        Molecule chain;
        const int a = chain.addAtom(0, 0), b = chain.addAtom(48, 0), c = chain.addAtom(96, 0);
        chain.addBond(a, b);
        chain.addBond(b, c);
        QVERIFY(chain.cleanUpStructure());
        QVERIFY(qAbs(normalized(angleAt(chain, b, a, c)) - 120.0) < 2.0);
        QVERIFY(bondsHaveStandardLength(chain));

        Molecule ring;
        QVERIFY(ring.stampRing("cyclopentane", 0, 0));
        QVERIFY(ring.moveAtom(1, ring.atomData().first().x + 20, ring.atomData().first().y + 12));
        QVERIFY(ring.cleanUpStructure());
        QVERIFY(bondsHaveStandardLength(ring));
        for (int center = 1; center <= 5; ++center) {
            const int previous = center == 1 ? 5 : center - 1, next = center == 5 ? 1 : center + 1;
            QVERIFY2(qAbs(normalized(angleAt(ring, center, previous, next)) - 108.0) < 2.0, qPrintable(QString::number(center)));
        }

        Molecule nitrile;
        const int methyl = nitrile.addAtom(0, 0), nitrileCarbon = nitrile.addAtom(40, 30), nitrogen = nitrile.addAtom(60, 80, "N");
        nitrile.addBond(methyl, nitrileCarbon);
        nitrile.addBond(nitrileCarbon, nitrogen, 3);
        QVERIFY(nitrile.cleanUpStructure());
        QVERIFY(qAbs(normalized(angleAt(nitrile, nitrileCarbon, methyl, nitrogen)) - 180.0) < 3.0);
    }
    void capsUndoHistory() {
        Molecule molecule;
        for (int index = 0; index < Molecule::MaxUndoSteps + 50; ++index) molecule.addAtom(index, 0);
        int steps = 0;
        while (molecule.undo()) ++steps;
        QCOMPARE(steps, Molecule::MaxUndoSteps);
        QCOMPARE(molecule.atomData().size(), 50);
    }
    void documentsSaveOpenAndExport() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Molecule molecule;
        ReactionLayer layer(&molecule);
        Document document(&molecule, &layer, directory.path());
        QCOMPARE(document.title(), QString("Untitled"));
        QVERIFY(!document.isDirty());

        molecule.addAtom(0, 0);
        QVERIFY(document.isDirty());
        QVERIFY(document.save());
        QVERIFY(!document.isDirty());
        const QString firstPath = document.currentPath();
        QVERIFY(firstPath.endsWith(".rcd"));
        QVERIFY(QFileInfo(firstPath).fileName().startsWith("drawing-"));
        layer.addArrow(0, 0, 100, 0);
        QVERIFY(document.isDirty());
        QVERIFY(document.save());
        QCOMPARE(document.currentPath(), firstPath);
        QCOMPARE(document.documents().size(), 1);

        // Opening a Molfile drops the old annotations and never overwrites the .mol.
        Molecule ethanol;
        const int c1 = ethanol.addAtom(0, 0);
        ethanol.extendFromAtom(ethanol.extendFromAtom(c1, 48, 0), 96, 0, "O");
        const QString molPath = QDir(directory.path()).filePath("ethanol.mol");
        QVERIFY(ethanol.saveToFile(molPath));
        const QByteArray original = [&molPath] { QFile file(molPath); file.open(QIODevice::ReadOnly); return file.readAll(); }();
        QVERIFY(document.open(molPath));
        QCOMPARE(molecule.atomData().size(), 3);
        QCOMPARE(layer.items().size(), 0);
        QCOMPARE(document.title(), QString("ethanol"));
        QVERIFY(document.isDirty());
        QVERIFY(document.save());
        QVERIFY(document.currentPath().endsWith("ethanol.rcd"));
        QFile molFile(molPath);
        QVERIFY(molFile.open(QIODevice::ReadOnly));
        QCOMPARE(molFile.readAll(), original);

        const QString smiPath = document.exportSmiles();
        QVERIFY(smiPath.contains("/exports/"));
        QFile smiFile(smiPath);
        QVERIFY(smiFile.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(smiFile.readAll()), QString("CCO ethanol\n"));
        QVERIFY(QFileInfo::exists(document.exportMolfile()));
        QVERIFY(QFileInfo::exists(document.exportSvg()));

        QVERIFY(document.open(firstPath));
        QCOMPARE(layer.items().size(), 1);
        QVERIFY(!document.isDirty());
        QCOMPARE(document.documents().size(), 3);

        document.newDocument();
        QCOMPARE(molecule.atomData().size(), 0);
        QCOMPARE(layer.items().size(), 0);
        QVERIFY(!document.isDirty());
        QCOMPARE(document.title(), QString("Untitled"));
    }
    void routesPenEverywhereAndFingerOnlyToMenus() {
        using Source = InputRouter::Source;
        using Phase = InputRouter::Phase;
        const QRectF canvas(300, 200, 1200, 1800);
        const QPointF onCanvas(800, 900), onMenu(100, 400);
        InputRouter router;

        // The pen works on the canvas and on the menus.
        QVERIFY(router.accept(Source::Pen, Phase::Press, onCanvas, canvas, false));
        QVERIFY(router.accept(Source::Pen, Phase::Move, onCanvas, canvas, false));
        QVERIFY(router.accept(Source::Pen, Phase::Release, onCanvas, canvas, false));
        QVERIFY(router.accept(Source::Pen, Phase::Press, onMenu, canvas, false));
        QVERIFY(router.accept(Source::Pen, Phase::Release, onMenu, canvas, false));

        // A finger on the canvas is ignored for the whole touch, even if it slides
        // onto the menu.
        QVERIFY(!router.accept(Source::Touch, Phase::Press, onCanvas, canvas, false));
        QVERIFY(!router.accept(Source::Touch, Phase::Move, onMenu, canvas, false));
        QVERIFY(!router.accept(Source::Touch, Phase::Release, onMenu, canvas, false));

        // A finger on the menu works, even if it slides over the canvas.
        QVERIFY(router.accept(Source::Touch, Phase::Press, onMenu, canvas, false));
        QVERIFY(router.accept(Source::Touch, Phase::Move, onCanvas, canvas, false));
        QVERIFY(router.accept(Source::Touch, Phase::Release, onCanvas, canvas, false));

        // With a dialog over the canvas, a finger may use it.
        QVERIFY(router.accept(Source::Touch, Phase::Press, onCanvas, canvas, true));
        QVERIFY(router.accept(Source::Touch, Phase::Release, onCanvas, canvas, true));

        // While the pen is down, a resting hand is ignored everywhere.
        QVERIFY(router.accept(Source::Pen, Phase::Press, onCanvas, canvas, false));
        QVERIFY(!router.accept(Source::Touch, Phase::Press, onMenu, canvas, false));
        QVERIFY(!router.accept(Source::Touch, Phase::Release, onMenu, canvas, false));
        QVERIFY(router.accept(Source::Pen, Phase::Release, onCanvas, canvas, false));
        QVERIFY(router.accept(Source::Touch, Phase::Press, onMenu, canvas, false));
        QVERIFY(router.accept(Source::Touch, Phase::Release, onMenu, canvas, false));

        // A stray move or release with no press is dropped.
        QVERIFY(!router.accept(Source::Touch, Phase::Move, onMenu, canvas, false));
        QVERIFY(!router.accept(Source::Touch, Phase::Release, onMenu, canvas, false));
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
