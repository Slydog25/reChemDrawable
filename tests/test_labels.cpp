#include "core/Abbreviations.h"
#include "core/Molecule.h"
#include "core/MolfileReader.h"
#include "core/MolfileWriter.h"
#include "core/ReactionLayer.h"
#include "core/SmilesWriter.h"
#include "core/SvgWriter.h"

#include <QtTest>
#include <QTemporaryDir>

class LabelTests : public QObject {
    Q_OBJECT

    static const Atom &atomWithId(const Molecule &molecule, int id) {
        for (const Atom &atom : molecule.atomData()) if (atom.id == id) return atom;
        Q_ASSERT(false);
        return molecule.atomData().first();
    }

    // Carbon at the origin with the heteroatom `element` placed to the right or left.
    static int addPair(Molecule &molecule, const QString &element, bool heteroOnRight) {
        const int carbon = molecule.addAtom(0, 0);
        const int hetero = molecule.addAtom(heteroOnRight ? 40 : -40, 0, element);
        molecule.addBond(carbon, hetero);
        return hetero;
    }

private slots:
    void methanolLabelsHydrogenAwayFromBond() {
        Molecule right;
        const int oxygen = addPair(right, "O", true); // bond comes from the left
        auto display = right.atomDisplay(atomWithId(right, oxygen));
        QCOMPARE(display.text, QString("OH"));
        QCOMPARE(display.centerIndex, 0);
        QVERIFY(!display.labelLeft);

        Molecule left;
        const int oxygen2 = addPair(left, "O", false); // bond comes from the right
        display = left.atomDisplay(atomWithId(left, oxygen2));
        QCOMPARE(display.text, QString("HO"));
        QCOMPARE(display.centerIndex, 1);
        QVERIFY(display.labelLeft);
        QVERIFY(!left.atomDisplay(atomWithId(left, 1)).showLabel); // the carbon stays bare
    }

    void aminesAndAmmonium() {
        Molecule molecule;
        const int nitrogen = addPair(molecule, "N", true);
        QCOMPARE(molecule.atomDisplay(atomWithId(molecule, nitrogen)).text, QString("NH₂"));
        Molecule mirrored;
        const int nitrogen2 = addPair(mirrored, "N", false);
        const auto display = mirrored.atomDisplay(atomWithId(mirrored, nitrogen2));
        QCOMPARE(display.text, QString("H₂N"));
        QCOMPARE(display.centerIndex, 2);

        QVERIFY(molecule.setAtomCharge(nitrogen, 1));
        const auto ammonium = molecule.atomDisplay(atomWithId(molecule, nitrogen));
        QCOMPARE(ammonium.text, QString("NH₃"));
        QCOMPARE(ammonium.chargeText, QString("⁺"));
        QVERIFY(molecule.setAtomCharge(nitrogen, 1));
    }

    void loneCarbonShowsMethane() {
        Molecule molecule;
        const int carbon = molecule.addAtom(0, 0);
        const auto display = molecule.atomDisplay(atomWithId(molecule, carbon));
        QVERIFY(display.showLabel);
        QCOMPARE(display.text, QString("CH₄"));
    }

    void isotopesAndChargedCarbon() {
        Molecule molecule;
        const int carbon = molecule.addAtom(0, 0);
        const int other = molecule.addAtom(40, 0);
        molecule.addBond(carbon, other);
        molecule.setAtomIsotope(carbon, 13);
        auto display = molecule.atomDisplay(atomWithId(molecule, carbon));
        QCOMPARE(display.text, QString("H₃¹³C")); // bond to the right: H on the left
        QCOMPARE(display.centerIndex, 4);
        QVERIFY(molecule.setAtomCharge(other, -1));
        display = molecule.atomDisplay(atomWithId(molecule, other));
        QVERIFY(!display.showLabel);
        QCOMPARE(display.chargeText, QString("⁻"));
        QVERIFY(molecule.setAtomCharge(other, 1));
        QCOMPARE(molecule.atomDisplay(atomWithId(molecule, other)).chargeLabel, QString("+"));
    }

    void radicalReducesHydrogens() {
        Molecule molecule;
        const int carbon = molecule.addAtom(0, 0);
        QCOMPARE(molecule.implicitHydrogens(atomWithId(molecule, carbon)), 4);
        QVERIFY(molecule.cycleRadical(carbon));
        QCOMPARE(molecule.implicitHydrogens(atomWithId(molecule, carbon)), 3);
        QCOMPARE(molecule.formula(), QString("CH3"));
        QVERIFY(molecule.cycleRadical(carbon));
        QCOMPARE(molecule.implicitHydrogens(atomWithId(molecule, carbon)), 2);
        QVERIFY(molecule.cycleRadical(carbon));
        QCOMPARE(atomWithId(molecule, carbon).radical, 0);
        const auto map = molecule.atoms().first().toMap();
        QVERIFY(map.contains("radicalAngles"));
    }

    void lonePairCyclingAndUndo() {
        Molecule molecule;
        const int oxygen = addPair(molecule, "O", true);
        for (int expected = 1; expected <= 3; ++expected) {
            QVERIFY(molecule.cycleLonePairs(oxygen));
            QCOMPARE(atomWithId(molecule, oxygen).lonePairs, expected);
        }
        const auto display = molecule.atomDisplay(atomWithId(molecule, oxygen));
        QCOMPARE(display.lonePairAngles.size(), 3);
        for (double angle : display.lonePairAngles) QVERIFY(qAbs(angle) > 1.0); // away from the H side (0 degrees)
        QVERIFY(molecule.cycleLonePairs(oxygen));
        QCOMPARE(atomWithId(molecule, oxygen).lonePairs, 0);
        QVERIFY(molecule.undo());
        QCOMPARE(atomWithId(molecule, oxygen).lonePairs, 3);
    }

    void problemAtoms() {
        Molecule molecule;
        const int oxygen = molecule.addAtom(0, 0, "O");
        const int a = molecule.addAtom(40, 0), b = molecule.addAtom(-40, 0), c = molecule.addAtom(0, 40);
        molecule.addBond(oxygen, a); molecule.addBond(oxygen, b);
        QVERIFY(!molecule.atomDisplay(atomWithId(molecule, oxygen)).problem);
        QVERIFY(molecule.cycleRadical(oxygen)); // O with two bonds cannot also be a radical
        QCOMPARE(atomWithId(molecule, oxygen).radical, 0);
        Q_UNUSED(c)
        const int odd = molecule.addAtom(100, 100, "Xx");
        QVERIFY(molecule.atomDisplay(atomWithId(molecule, odd)).problem);
        QVERIFY(molecule.atoms().last().toMap().value("problem").toBool());
        // Over-bonded atoms arrive through Molfile import.
        Molecule loaded;
        QString error;
        const QString file = "x\n  y\n\n  5  4  0  0  0  0            999 V2000\n"
            "    0.0000    0.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0\n"
            "    1.0000    0.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0\n"
            "   -1.0000    0.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0\n"
            "    0.0000    1.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0\n"
            "    0.0000   -1.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0\n"
            "  1  2  1  0\n  1  3  1  0\n  1  4  1  0\n  1  5  2  0\nM  END\n";
        QVERIFY(MolfileReader::readV2000(file, &loaded, &error));
        QVERIFY(loaded.atomDisplay(loaded.atomData().first()).problem);
    }

    void anisoleFormulaAndWeight() {
        Molecule molecule;
        const int phenyl = molecule.addAtom(0, 0, "Ph");
        const int methoxy = molecule.addAtom(40, 0, "OMe");
        QVERIFY(molecule.addBond(phenyl, methoxy) > 0);
        QCOMPARE(molecule.formula(), QString("C7H8O"));
        QVERIFY(qAbs(molecule.properties().value("molecularWeight").toDouble() - 108.14) < 0.05);
        QVERIFY(molecule.properties().value("unknownElements").toStringList().isEmpty());

        Molecule ring; // benzene ring plus an OMe group on one carbon
        QVERIFY(ring.stampRing("benzene", 0, 0));
        const int ipso = ring.atomData().first().id;
        const int group = ring.extendFromAtom(ipso, 60, 0, "OMe");
        QVERIFY(group > 0);
        QCOMPARE(ring.formula(), QString("C7H8O"));
    }

    void abbreviationHasSingleBond() {
        Molecule molecule;
        const int group = molecule.addAtom(0, 0, "Boc");
        const int a = molecule.addAtom(40, 0), b = molecule.addAtom(-40, 0);
        QVERIFY(molecule.addBond(group, a) > 0);
        QCOMPARE(molecule.addBond(group, b), -1);
        QCOMPARE(molecule.implicitHydrogens(atomWithId(molecule, group)), 0);
        QVERIFY(!molecule.setAtomCharge(group, 1));
    }

    void abbreviationLabelsMirror() {
        Molecule molecule;
        const int carbon = molecule.addAtom(0, 0);
        const int group = molecule.addAtom(-40, 0, "CO2Et"); // bond comes from the right
        molecule.addBond(carbon, group);
        auto display = molecule.atomDisplay(atomWithId(molecule, group));
        QCOMPARE(display.text, QString("EtO₂C"));
        QCOMPARE(display.centerIndex, 4);
        const int other = molecule.addAtom(40, 0, "OMe");
        molecule.addBond(carbon, other);
        display = molecule.atomDisplay(atomWithId(molecule, other));
        QCOMPARE(display.text, QString("OMe"));
        QCOMPARE(display.centerIndex, 0);
    }

    void applyAbbreviationRelabelsOrExtends() {
        Molecule molecule;
        const int a = molecule.addAtom(0, 0), b = molecule.addAtom(40, 0);
        molecule.addBond(a, b);
        QCOMPARE(molecule.applyAbbreviation(b, "OMe"), b); // terminal carbon relabelled
        QCOMPARE(atomWithId(molecule, b).element, QString("OMe"));
        QCOMPARE(molecule.atomData().size(), 2);
        const int added = molecule.applyAbbreviation(a, "Boc"); // a has one bond too -> relabelled
        QCOMPARE(added, a);
        Molecule chain;
        const int x = chain.addAtom(0, 0), y = chain.addAtom(40, 0), z = chain.addAtom(80, 0);
        chain.addBond(x, y); chain.addBond(y, z);
        const int fresh = chain.applyAbbreviation(y, "Ts");
        QVERIFY(fresh > 0 && fresh != y);
        QCOMPARE(chain.atomData().size(), 4);
    }

    void smilesRoundTrips() {
        Molecule molecule;
        const int c1 = molecule.addAtom(0, 0), c2 = molecule.addAtom(40, 0), tosylate = molecule.addAtom(80, 0, "OTs");
        molecule.addBond(c1, c2); molecule.addBond(c2, tosylate);
        QCOMPARE(molecule.smiles(), QString("CCOS(=O)(=O)c%91ccc(C)cc%91"));
        QCOMPARE(molecule.formula(), QString("C9H12O3S"));

        // The component never starts on a group when another atom exists.
        Molecule starts;
        const int group = starts.addAtom(0, 0, "OMe");
        const int carbon = starts.addAtom(40, 0);
        starts.addBond(group, carbon);
        QCOMPARE(starts.smiles(), QString("COC"));

        // Two linked groups: the child hangs off the first fragment atom.
        Molecule pair;
        const int ph = pair.addAtom(0, 0, "Ph");
        const int me = pair.addAtom(40, 0, "OMe");
        pair.addBond(ph, me);
        QCOMPARE(pair.smiles(), QString("c%91(OC)ccccc%91"));
    }

    void fragmentsInsideRingsKeepDigitsDistinct() {
        Molecule molecule;
        QVERIFY(molecule.stampRing("cyclohexane", 0, 0));
        const QVector<Atom> ring = molecule.atomData();
        molecule.extendFromAtom(ring[0].id, -80, 0, "Fmoc");
        molecule.extendFromAtom(ring[3].id, 80, 0, "Ts");
        const QString smiles = molecule.smiles();
        QVERIFY(smiles.contains("%91"));
        QVERIFY(smiles.contains("%92"));
        QVERIFY(smiles.contains("C1"));
        QCOMPARE(smiles.count("%91"), 4); // Fmoc opens/closes it once, Ts once
    }

    void molfileAliasAndRadicalRoundTrip() {
        Molecule molecule;
        const int carbon = molecule.addAtom(0, 0);
        const int group = molecule.addAtom(40, 0, "OMe");
        const int other = molecule.addAtom(-40, 0);
        molecule.addBond(carbon, group); molecule.addBond(carbon, other);
        QVERIFY(molecule.cycleRadical(other));
        QVERIFY(molecule.cycleRadical(other));
        const QString text = MolfileWriter::writeV2000(molecule, "t");
        QVERIFY(text.contains(" R  "));
        QVERIFY(text.contains("A  "));
        QVERIFY(text.contains("M  RAD  1   3   3"));
        Molecule loaded;
        QString error;
        QVERIFY(MolfileReader::readV2000(text, &loaded, &error));
        QCOMPARE(loaded.atomData()[1].element, QString("OMe"));
        QCOMPARE(loaded.atomData()[2].radical, 2);
        QCOMPARE(loaded.formula(), molecule.formula());
        // A plain singlet value (1) is read as two dots.
        QString singlet = text;
        singlet.replace("M  RAD  1   3   3", "M  RAD  1   3   1");
        Molecule loaded2;
        QVERIFY(MolfileReader::readV2000(singlet, &loaded2, &error));
        QCOMPARE(loaded2.atomData()[2].radical, 2);
    }

    void rcdRoundTripOfLonePairsAndCircledCharges() {
        QTemporaryDir directory;
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int oxygen = addPair(molecule, "O", true);
        molecule.cycleLonePairs(oxygen);
        molecule.cycleLonePairs(oxygen);
        molecule.setCircledCharges(true);
        const QString path = directory.filePath("scheme.rcd");
        QVERIFY(layer.saveToFile(path));

        Molecule loaded;
        ReactionLayer loadedLayer(&loaded);
        QVERIFY(loadedLayer.loadFromFile(path));
        QCOMPARE(loaded.atomData()[1].lonePairs, 2);
        QVERIFY(loaded.circledCharges());

        // Older files without the keys still load.
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        root.remove("lonePairs");
        root.remove("circledCharges");
        QFile legacy(directory.filePath("old.rcd"));
        QVERIFY(legacy.open(QIODevice::WriteOnly));
        legacy.write(QJsonDocument(root).toJson());
        legacy.close();
        QVERIFY(loadedLayer.loadFromFile(directory.filePath("old.rcd")));
        QCOMPARE(loaded.atomData()[1].lonePairs, 0);
        QVERIFY(!loaded.circledCharges());
    }

    void svgUsesLabels() {
        Molecule molecule;
        addPair(molecule, "O", false);
        const QString svg = SvgWriter::write(molecule);
        QVERIFY(svg.contains(">H<"));
        QVERIFY(svg.contains(">O<"));
    }
};

QTEST_MAIN(LabelTests)
#include "test_labels.moc"
