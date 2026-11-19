#include "core/EditHistory.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/SchemeAnalysis.h"
#include "core/SvgWriter.h"

#include <QTemporaryDir>
#include <QtTest>

class MechanismTest : public QObject {
    Q_OBJECT

    static QVariantMap itemById(const ReactionLayer &layer, int id) {
        for (const QVariant &value : layer.items())
            if (value.toMap().value("id").toInt() == id) return value.toMap();
        return {};
    }
    // Ethene centred at (x, y).
    static void ethene(Molecule &m, double x, double y) {
        const int a = m.addAtom(x - 24, y), b = m.addAtom(x + 24, y);
        m.addBond(a, b, 2);
    }
    // 1,2-dibromoethane (or with one Br missing, bromoethane) centred at (x, y).
    static void dibromoethane(Molecule &m, double x, double y, bool bothBromines = true) {
        const int c1 = m.addAtom(x - 24, y), c2 = m.addAtom(x + 24, y);
        m.addBond(c1, c2);
        const int br1 = m.addAtom(x - 72, y - 20, "Br");
        m.addBond(c1, br1);
        if (bothBromines) {
            const int br2 = m.addAtom(x + 72, y + 20, "Br");
            m.addBond(c2, br2);
        }
    }
    static void dibromine(Molecule &m, double x, double y) {
        const int a = m.addAtom(x - 24, y, "Br"), b = m.addAtom(x + 24, y, "Br");
        m.addBond(a, b);
    }

private slots:
    void markersRoundTripThroughRcdAndSvg() {
        QTemporaryDir dir;
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int dagger = layer.addDagger(10, 20);
        const int bracket = layer.addBracket(100, 200, 40, 80, true); // corners in any order
        const int plain = layer.addBracket(300, 80, 400, 200);
        const int orbital = layer.addOrbital(50, 300, 0, 5, true);
        const int plainOrbital = layer.addOrbital(150, 300, 1, 0);
        const int delta = layer.addLabel(QStringLiteral("δ+"), 70, 90);
        QVERIFY(dagger >= 0 && bracket >= 0 && plain >= 0 && orbital >= 0 && plainOrbital >= 0 && delta >= 0);
        QVERIFY(layer.addBracket(0, 0, 3, 100) < 0); // too thin
        const QVariantMap b = itemById(layer, bracket);
        QCOMPARE(b.value("x1").toDouble(), 40.0);
        QCOMPARE(b.value("y2").toDouble(), 200.0);

        const QString path = dir.filePath("mech.rcd");
        QVERIFY(layer.saveToFile(path));
        Molecule other;
        ReactionLayer loaded(&other);
        QVERIFY(loaded.loadFromFile(path));
        QCOMPARE(loaded.items().size(), 6);
        QCOMPARE(itemById(loaded, dagger).value("type").toString(), QStringLiteral("dagger"));
        QCOMPARE(itemById(loaded, bracket).value("kind").toString(), QStringLiteral("dagger"));
        QCOMPARE(itemById(loaded, plain).value("kind").toString(), QStringLiteral("plain"));
        QCOMPARE(itemById(loaded, orbital).value("kind").toString(), QStringLiteral("shaded"));
        QCOMPARE(itemById(loaded, plainOrbital).value("kind").toString(), QStringLiteral("plain"));
        const QVariantMap o = itemById(loaded, orbital);
        QCOMPARE(o.value("x1").toDouble(), 50.0);
        QCOMPARE(o.value("y2").toDouble(), 300.0 + ReactionLayer::OrbitalLobeLength);
        QCOMPARE(itemById(loaded, delta).value("text").toString(), QStringLiteral("δ+"));

        const QString svg = SvgWriter::write(molecule, layer.items());
        QCOMPARE(svg.count("class=\"dagger\""), 2); // stand-alone and the bracket's
        QCOMPARE(svg.count("class=\"bracket\""), 2);
        QCOMPARE(svg.count("class=\"orbital\""), 2);
        QVERIFY(svg.contains(QStringLiteral("δ+")));
        QVERIFY(svg.contains(QStringLiteral("fill=\"#111\" d=\"M50,300"))); // shaded lobe
    }

    void hitTestEraseMoveAndUndo() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int bracket = layer.addBracket(100, 100, 220, 180);
        const int dagger = layer.addDagger(400, 100);
        const int orbital = layer.addOrbital(300, 300, 1, 0);
        QCOMPARE(layer.itemAt(100, 140), bracket);       // left stroke
        QCOMPARE(layer.itemAt(220, 120), bracket);       // right stroke
        QCOMPARE(layer.itemAt(160, 140), -1);            // inside: atoms stay reachable
        QCOMPARE(layer.itemAt(402, 104), dagger);
        QCOMPARE(layer.itemAt(300 + 25, 300), orbital);  // on a lobe
        QCOMPARE(layer.itemAt(300, 340), -1);

        QVERIFY(layer.translateItem(bracket, 10, 20));
        QCOMPARE(itemById(layer, bracket).value("x1").toDouble(), 110.0);
        QCOMPARE(itemById(layer, bracket).value("y2").toDouble(), 200.0);
        QVERIFY(layer.translateItem(orbital, 5, 5));
        QCOMPARE(itemById(layer, orbital).value("x2").toDouble(), 300.0 + 5 + ReactionLayer::OrbitalLobeLength);
        QVERIFY(layer.removeItem(dagger));
        QCOMPARE(layer.items().size(), 2);
        QVERIFY(layer.undo());
        QCOMPARE(layer.items().size(), 3);
        QVERIFY(layer.undo()); // move of the orbital
        QCOMPARE(itemById(layer, orbital).value("x1").toDouble(), 300.0);
    }

    void partialBondsAreIgnoredByHydrogensAndSmiles() {
        Molecule molecule;
        const int c1 = molecule.addAtom(0, 0), c2 = molecule.addAtom(48, 0);
        const int bond = molecule.addBond(c1, c2);
        QCOMPARE(molecule.formula(), QStringLiteral("C2H6"));
        QCOMPARE(molecule.smiles(), QStringLiteral("CC"));
        QVERIFY(molecule.setBondStyle(bond, "partial"));
        QCOMPARE(molecule.bondData().first().order, 1);
        QCOMPARE(molecule.formula(), QStringLiteral("C2H8"));   // no valence used: both carbons fill up
        QCOMPARE(molecule.smiles(), QStringLiteral("C.C"));      // not a bond in the SMILES
        QVERIFY(molecule.canAddBond(c1, molecule.addAtom(0, 48), 3)); // full valence still free
        // Style tool cycle reaches it from aromatic and wraps around.
        QVERIFY(molecule.setBondStyle(bond, "aromatic"));
        QVERIFY(molecule.cycleBondStyle(bond));
        QCOMPARE(molecule.bondData().first().style, QStringLiteral("partial"));
        QVERIFY(molecule.cycleBondStyle(bond));
        QCOMPARE(molecule.bondData().first().style, QStringLiteral("plain"));
        // A double bond turned partial becomes a single dashed one.
        const int d1 = molecule.addAtom(500, 0), d2 = molecule.addAtom(548, 0);
        const int dbl = molecule.addBond(d1, d2, 2);
        QVERIFY(molecule.setBondStyle(dbl, "partial"));
        for (const Bond &b : molecule.bondData()) if (b.id == dbl) QCOMPARE(b.order, 1);
    }

    void partialBondsRoundTripAndDrawDashed() {
        QTemporaryDir dir;
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int a = molecule.addAtom(0, 0), b = molecule.addAtom(48, 0), c = molecule.addAtom(96, 0, "Br");
        molecule.addBond(a, b);
        const int partial = molecule.addBond(b, c);
        QVERIFY(molecule.setBondStyle(partial, "partial"));
        const QString path = dir.filePath("ts.rcd");
        QVERIFY(layer.saveToFile(path));
        Molecule other;
        ReactionLayer loaded(&other);
        QVERIFY(loaded.loadFromFile(path));
        QCOMPARE(other.bondData().size(), 2);
        QCOMPARE(other.bondData()[0].style, QStringLiteral("plain"));
        QCOMPARE(other.bondData()[1].style, QStringLiteral("partial"));
        QCOMPARE(other.formula(), molecule.formula());
        QVERIFY(SvgWriter::write(molecule, {}).contains(QStringLiteral("stroke-dasharray=\"6 5\" class=\"bond-partial\"")));
        QVERIFY(!SvgWriter::write(other, {}).isEmpty());
    }

    void balancedAndUnbalancedReactions() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        ethene(molecule, 0, 0);
        dibromine(molecule, 160, 0);
        dibromoethane(molecule, 600, 0);
        layer.addArrowKind("forward", 300, 0, 450, 0);
        QVariantList results = layer.checkBalance();
        QCOMPARE(results.size(), 1);
        QVERIFY2(results[0].toMap().value("balanced").toBool(), qPrintable(results[0].toMap().value("text").toString()));
        QCOMPARE(results[0].toMap().value("reactants").toInt(), 2);
        QCOMPARE(results[0].toMap().value("products").toInt(), 1);
        QVERIFY(results[0].toMap().value("text").toString().contains("Balanced"));

        // Bromoethane (a missing Br) is not balanced: C2H4 + Br2 -> C2H5Br.
        Molecule molecule2;
        ReactionLayer layer2(&molecule2);
        ethene(molecule2, 0, 0);
        dibromine(molecule2, 160, 0);
        dibromoethane(molecule2, 600, 0, false);
        layer2.addArrowKind("equilibrium", 300, 0, 450, 0);
        results = layer2.checkBalance();
        QCOMPARE(results.size(), 1);
        const QVariantMap bad = results[0].toMap();
        QVERIFY(!bad.value("balanced").toBool());
        const QVariantList differences = bad.value("differences").toList();
        QCOMPARE(differences.size(), 2); // H and Br
        QCOMPARE(differences[0].toMap().value("element").toString(), QStringLiteral("H"));
        QCOMPARE(differences[0].toMap().value("products").toInt() - differences[0].toMap().value("reactants").toInt(), 1);
        QCOMPARE(differences[1].toMap().value("element").toString(), QStringLiteral("Br"));
        QCOMPARE(differences[1].toMap().value("reactants").toInt() - differences[1].toMap().value("products").toInt(), 1);
        QVERIFY(bad.value("text").toString().contains("Not balanced"));
    }

    void chargeAndAbbreviationsCount() {
        // CH3-Br + OH- -> CH3-OH + Br-: balanced including charge.
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int c = molecule.addAtom(0, 0), br = molecule.addAtom(48, 0, "Br");
        molecule.addBond(c, br);
        const int oh = molecule.addAtom(140, 0, "O");
        molecule.setAtomCharge(oh, -1);
        const int c2 = molecule.addAtom(500, 0), o2 = molecule.addAtom(548, 0, "O");
        molecule.addBond(c2, o2);
        const int bromide = molecule.addAtom(640, 0, "Br");
        molecule.setAtomCharge(bromide, -1);
        layer.addArrowKind("forward", 250, 0, 400, 0);
        QVariantList results = layer.checkBalance();
        QCOMPARE(results.size(), 1);
        QVERIFY2(results[0].toMap().value("balanced").toBool(), qPrintable(results[0].toMap().value("text").toString()));
        // Lose the charge on the bromide: elements still balance, the charge does not.
        molecule.setAtomCharge(bromide, 0);
        results = layer.checkBalance();
        QVERIFY(!results[0].toMap().value("balanced").toBool());
        QCOMPARE(results[0].toMap().value("differences").toList().size(), 1); // bromide now gains an H
        QCOMPARE(results[0].toMap().value("chargeReactants").toInt(), -1);
        QCOMPARE(results[0].toMap().value("chargeProducts").toInt(), 0);

        // An abbreviation counts through its composition: CH3-OMe is C2H6O.
        Molecule groups;
        const int methyl = groups.addAtom(0, 0), ome = groups.addAtom(48, 0, "C");
        groups.addBond(methyl, ome);
        QVERIFY(groups.applyAbbreviation(ome, "OMe") >= 0);
        const auto parts = SchemeAnalysis::components(groups);
        QCOMPARE(parts.size(), 1);
        QCOMPARE(SchemeAnalysis::formulaText(SchemeAnalysis::composition(groups, parts[0].atomIndices)), QStringLiteral("C2H6O"));
    }

    void multiStepAndSkippedArrows() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        // A -> B -> C where B differs from A: each arrow checks only its neighbours.
        ethene(molecule, 0, 0);
        dibromine(molecule, 160, 0);
        dibromoethane(molecule, 600, 0);
        layer.addArrowKind("forward", 300, 0, 450, 0);
        layer.addArrowKind("retro", 300, 200, 450, 200);
        layer.addArrowKind("resonance", 300, 300, 450, 300);
        layer.addArrowKind("curved", 300, 400, 450, 400);
        QCOMPARE(layer.checkBalance().size(), 1); // only the forward arrow is a reaction arrow
    }

    void tidyLaysOutLeftToRightAsOneUndoStep() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        EditHistory history(&molecule, &layer);
        // Deliberately ragged: different heights, uneven gaps, a slanted arrow.
        ethene(molecule, 100, 40);
        dibromine(molecule, 330, 160);
        dibromoethane(molecule, 900, 70);
        const int plus = layer.addLabel("+", 215, 100);
        const int arrow = layer.addArrowKind("forward", 520, 150, 700, 120);
        const int label = layer.addLabelSnapped("CH2Cl2, rt", 610, 100); // snaps above the arrow
        QVERIFY(plus >= 0 && arrow >= 0 && label >= 0);
        const QVector<Atom> atomsBefore = molecule.atomData();
        const QVariantMap labelBefore = itemById(layer, label);

        QVERIFY(history.tidyScheme());
        const QVector<SchemeAnalysis::Component> parts = SchemeAnalysis::components(molecule);
        QCOMPARE(parts.size(), 3);
        // Order by x must match the order drawn: ethene, Br2, product.
        QVERIFY(parts[0].bounds.center().x() < parts[1].bounds.center().x());
        QVERIFY(parts[1].bounds.center().x() < parts[2].bounds.center().x());
        const double baseline = parts[0].bounds.center().y();
        for (const auto &part : parts) QVERIFY(qAbs(part.bounds.center().y() - baseline) < 1e-6);
        const QVariantMap plusAfter = itemById(layer, plus), arrowAfter = itemById(layer, arrow), labelAfter = itemById(layer, label);
        // Monotonic: ethene < plus < Br2 < arrow < product.
        QVERIFY(parts[0].bounds.right() < plusAfter.value("x1").toDouble());
        QVERIFY(plusAfter.value("x1").toDouble() < parts[1].bounds.left());
        QVERIFY(parts[1].bounds.right() < arrowAfter.value("x1").toDouble());
        QVERIFY(arrowAfter.value("x2").toDouble() < parts[2].bounds.left());
        QCOMPARE(arrowAfter.value("y1").toDouble(), baseline);
        QCOMPARE(arrowAfter.value("y2").toDouble(), baseline);
        QVERIFY(qAbs(plusAfter.value("y1").toDouble() - baseline) < 1e-6);
        QVERIFY(arrowAfter.value("x2").toDouble() - arrowAfter.value("x1").toDouble() >= 3 * molecule.standardBondLength() - 1e-6);
        // The condition label kept its offset from the arrow's midpoint x and stayed above it.
        const double labelMidX = (arrowAfter.value("x1").toDouble() + arrowAfter.value("x2").toDouble()) / 2;
        QVERIFY(qAbs(labelAfter.value("x1").toDouble() - labelMidX) < 60);
        QVERIFY(labelAfter.value("y1").toDouble() < baseline);
        QVERIFY(qAbs(labelAfter.value("y1").toDouble() - baseline) < 40);

        // The balance check still reads the tidied scheme the same way.
        QVERIFY(layer.checkBalance().first().toMap().value("balanced").toBool());

        // One undo step restores molecule and annotations together.
        QVERIFY(history.undo());
        const QVector<Atom> atomsUndone = molecule.atomData();
        for (int i = 0; i < atomsBefore.size(); ++i) {
            QCOMPARE(atomsUndone[i].x, atomsBefore[i].x);
            QCOMPARE(atomsUndone[i].y, atomsBefore[i].y);
        }
        QCOMPARE(itemById(layer, arrow).value("x1").toDouble(), 520.0);
        QCOMPARE(itemById(layer, arrow).value("y2").toDouble(), 120.0);
        QCOMPARE(itemById(layer, label).value("x1").toDouble(), labelBefore.value("x1").toDouble());
        QCOMPARE(itemById(layer, label).value("y1").toDouble(), labelBefore.value("y1").toDouble());
        QVERIFY(history.redo());
        QCOMPARE(itemById(layer, arrow).value("y1").toDouble(), baseline);
        // Tidying an already tidy scheme changes nothing.
        QVERIFY(!history.tidyScheme());
    }
};

QTEST_MAIN(MechanismTest)
#include "test_mechanism.moc"
