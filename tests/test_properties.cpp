#include "core/Molecule.h"
#include "core/Properties.h"

#include <QtTest>

namespace {
Properties::Report report(const QString &smiles, const QVector<int> &selection = {}) {
    Molecule molecule;
    if (!molecule.importSmiles(smiles, 0, 0)) qFatal("SMILES import failed: %s", qPrintable(smiles));
    return Properties::compute(molecule, selection);
}

double relativeAt(const Properties::Report &r, int offset) {
    for (const Properties::Peak &peak : r.pattern) if (peak.offset == offset) return peak.relative;
    return 0.0;
}
}

class PropertiesTest : public QObject {
    Q_OBJECT
private slots:
    void degreesOfUnsaturation_data() {
        QTest::addColumn<QString>("smiles");
        QTest::addColumn<double>("expected");
        QTest::newRow("benzene") << "c1ccccc1" << 4.0;
        QTest::newRow("cyclohexene") << "C1=CCCCC1" << 2.0;
        QTest::newRow("pyridine") << "c1ccncc1" << 4.0;
        QTest::newRow("acetonitrile") << "CC#N" << 2.0;
        QTest::newRow("chlorobenzene") << "Clc1ccccc1" << 4.0;
        QTest::newRow("ethanol") << "CCO" << 0.0;
        QTest::newRow("acetone") << "CC(C)=O" << 1.0;
        QTest::newRow("nitrobenzene") << "O=[N+]([O-])c1ccccc1" << 5.0;
        QTest::newRow("ammonium") << "[NH4+]" << 0.0;
    }
    void degreesOfUnsaturation() {
        QFETCH(QString, smiles);
        QFETCH(double, expected);
        QCOMPARE(report(smiles).degreesOfUnsaturation, expected);
    }

    void abbreviationsCountTowardDegreesOfUnsaturation() {
        Molecule molecule;
        const int c = molecule.addAtom(0, 0);
        QVERIFY(molecule.applyAbbreviation(c, "Ph") >= 0);
        // toluene-like: methyl carbon + Ph group -> C7H8, DoU 4
        const Properties::Report r = Properties::compute(molecule);
        QCOMPARE(r.formula, QStringLiteral("C7H8"));
        QCOMPARE(r.degreesOfUnsaturation, 4.0);
    }

    void hydrogenBonding_data() {
        QTest::addColumn<QString>("smiles");
        QTest::addColumn<int>("donors");
        QTest::addColumn<int>("acceptors");
        QTest::newRow("ethanol") << "CCO" << 1 << 1;
        QTest::newRow("acetamide") << "CC(N)=O" << 2 << 2;
        QTest::newRow("aspirin") << "CC(=O)Oc1ccccc1C(=O)O" << 1 << 4;
        QTest::newRow("hexane") << "CCCCCC" << 0 << 0;
    }
    void hydrogenBonding() {
        QFETCH(QString, smiles);
        QFETCH(int, donors);
        QFETCH(int, acceptors);
        const Properties::Report r = report(smiles);
        QCOMPARE(r.hydrogenBondDonors, donors);
        QCOMPARE(r.hydrogenBondAcceptors, acceptors);
    }

    void ruleOfFive() {
        QCOMPARE(report("CC(=O)Oc1ccccc1C(=O)O").ruleOfFiveViolations, 0);
        // Octitol: 8 donors exceed the limit of 5.
        const Properties::Report r = report("OCC(O)C(O)C(O)C(O)C(O)C(O)CO");
        QCOMPARE(r.hydrogenBondDonors, 8);
        QCOMPARE(r.hydrogenBondAcceptors, 8);
        QCOMPARE(r.ruleOfFiveViolations, 1); // donors > 5
    }

    void tpsa_data() {
        QTest::addColumn<QString>("smiles");
        QTest::addColumn<double>("expected");
        QTest::newRow("aspirin") << "CC(=O)Oc1ccccc1C(=O)O" << 63.6;
        QTest::newRow("paracetamol") << "CC(=O)Nc1ccc(O)cc1" << 49.3;
        QTest::newRow("caffeine") << "Cn1cnc2c1c(=O)n(C)c(=O)n2C" << 58.4;
        QTest::newRow("ethanol") << "CCO" << 20.2;
        QTest::newRow("acetamide") << "CC(N)=O" << 43.1;
        QTest::newRow("nitrobenzene") << "O=[N+]([O-])c1ccccc1" << 43.1;
        QTest::newRow("pyridine") << "c1ccncc1" << 12.9;
        QTest::newRow("acetonitrile") << "CC#N" << 23.8;
        QTest::newRow("diethyl ether") << "CCOCC" << 9.2;
        QTest::newRow("furan") << "c1ccoc1" << 13.1;
        QTest::newRow("pyrrole") << "c1cc[nH]c1" << 15.8;
        QTest::newRow("hexane") << "CCCCCC" << 0.0;
    }
    void tpsa() {
        QFETCH(QString, smiles);
        QFETCH(double, expected);
        const Properties::Report r = report(smiles);
        QVERIFY2(qAbs(r.tpsa - expected) <= 0.5, qPrintable(QStringLiteral("TPSA %1, expected %2").arg(r.tpsa).arg(expected)));
        QCOMPARE(r.tpsaUnmatched, 0);
    }

    void tpsaSulfurIsSeparate() {
        const Properties::Report r = report("CS(C)=O"); // DMSO: O= 17.07 (N,O only), S 19.21 extra
        QVERIFY(qAbs(r.tpsa - 17.07) < 0.01);
        QVERIFY(qAbs(r.tpsaWithSulfurPhosphorus - 36.28) < 0.01);
    }

    void elementalAnalysisSumsToHundred_data() {
        QTest::addColumn<QString>("smiles");
        QTest::newRow("aspirin") << "CC(=O)Oc1ccccc1C(=O)O";
        QTest::newRow("caffeine") << "Cn1cnc2c1c(=O)n(C)c(=O)n2C";
        QTest::newRow("chloroform") << "ClC(Cl)Cl";
        QTest::newRow("DMSO") << "CS(C)=O";
    }
    void elementalAnalysisSumsToHundred() {
        QFETCH(QString, smiles);
        const Properties::Report r = report(smiles);
        double sum = 0.0;
        for (const Properties::ElementShare &share : r.elements) sum += share.percent;
        QVERIFY2(qAbs(sum - 100.0) < 1e-9, qPrintable(QString::number(sum)));
    }

    void elementalAnalysisValues() {
        const Properties::Report r = report("CCO");
        QCOMPARE(r.elements.size(), 3);
        QCOMPARE(r.elements[0].symbol, QStringLiteral("C"));
        QCOMPARE(r.elements[1].symbol, QStringLiteral("H"));
        QCOMPARE(r.elements[2].symbol, QStringLiteral("O"));
        QVERIFY(qAbs(r.elements[0].percent - 52.14) < 0.05);
        QVERIFY(qAbs(r.elements[1].percent - 13.13) < 0.05);
        QVERIFY(qAbs(r.elements[2].percent - 34.73) < 0.05);
    }

    void isotopePatternDichloromethane() {
        const Properties::Report r = report("ClCCl");
        QCOMPARE(r.formula, QStringLiteral("CH2Cl2"));
        QCOMPARE(r.pattern.first().offset, 0);
        QCOMPARE(r.pattern.first().nominal, 84);
        QVERIFY(qAbs(r.pattern.first().mass - 83.9533) < 0.001);
        QCOMPARE(r.pattern.first().relative, 100.0);
        QVERIFY(qAbs(relativeAt(r, 2) - 64.0) < 1.5);
        QVERIFY(qAbs(relativeAt(r, 4) - 10.2) < 1.0);
    }

    void isotopePatternBromobenzene() {
        const Properties::Report r = report("Brc1ccccc1");
        QVERIFY2(qAbs(relativeAt(r, 2) - 97.3) < 1.0, qPrintable(QString::number(relativeAt(r, 2))));
        QVERIFY(qAbs(relativeAt(r, 1) - 6.6) < 0.8);
    }

    void isotopePatternHydrocarbon() {
        const Properties::Report r = report("CCCCCCCCCC"); // C10H22
        QCOMPARE(r.pattern.first().relative, 100.0);
        QVERIFY2(qAbs(relativeAt(r, 1) - 11.2) < 0.5, qPrintable(QString::number(relativeAt(r, 1))));
        QVERIFY(relativeAt(r, 2) < 1.0);
    }

    void isotopePatternLargeMoleculeIsNormalised() {
        // C200 alkane: the most intense peak is not M.
        QString chain(200, 'C');
        const Properties::Report r = report(chain);
        double top = 0.0;
        for (const Properties::Peak &peak : r.pattern) top = qMax(top, peak.relative);
        QCOMPARE(top, 100.0);
        QVERIFY(relativeAt(r, 0) < 100.0);
        QVERIFY(relativeAt(r, 1) > relativeAt(r, 0));
    }

    void adductsOfCaffeine() {
        const Properties::Report r = report("Cn1cnc2c1c(=O)n(C)c(=O)n2C");
        QCOMPARE(r.formula, QStringLiteral("C8H10N4O2"));
        QVERIFY(qAbs(r.exactMass - 194.0804) < 0.001);
        QCOMPARE(r.adducts.size(), 3);
        QCOMPARE(r.adducts[0].label, QStringLiteral("[M+H]+"));
        QVERIFY2(qAbs(r.adducts[0].mz - 195.088) <= 0.001, qPrintable(QString::number(r.adducts[0].mz, 'f', 4)));
        QVERIFY(qAbs(r.adducts[1].mz - 217.0696) < 0.001); // [M+Na]+
        QVERIFY(qAbs(r.adducts[2].mz - 193.0731) < 0.001); // [M-H]-
    }

    void selectionOnlyCoversConnectedComponents() {
        Molecule molecule;
        QVERIFY(molecule.importSmiles("CCO", 0, 0));
        const int ethanolAtoms = molecule.atomData().size();
        QVERIFY(molecule.importSmiles("c1ccccc1", 300, 0));
        QCOMPARE(molecule.atomData().size(), ethanolAtoms + 6);

        const Properties::Report whole = Properties::compute(molecule);
        QVERIFY(!whole.selectionOnly);
        QCOMPARE(whole.formula, QStringLiteral("C8H12O"));

        // One atom of the benzene selects the whole benzene component.
        const int benzeneAtom = molecule.atomData().last().id;
        const Properties::Report ring = Properties::compute(molecule, {benzeneAtom});
        QVERIFY(ring.selectionOnly);
        QCOMPARE(ring.atomCount, 6);
        QCOMPARE(ring.formula, QStringLiteral("C6H6"));
        QCOMPARE(ring.degreesOfUnsaturation, 4.0);
        QCOMPARE(ring.hydrogenBondAcceptors, 0);

        const Properties::Report alcohol = Properties::compute(molecule, {molecule.atomData().first().id});
        QCOMPARE(alcohol.formula, QStringLiteral("C2H6O"));
        QCOMPARE(alcohol.hydrogenBondDonors, 1);

        // Both selected: the sum.
        const Properties::Report both = Properties::compute(molecule, {molecule.atomData().first().id, benzeneAtom});
        QCOMPARE(both.formula, QStringLiteral("C8H12O"));

        // Unknown ids fall back to the whole drawing.
        QVERIFY(!Properties::compute(molecule, {9999}).selectionOnly);
    }

    void emptyMoleculeIsHarmless() {
        Molecule molecule;
        const Properties::Report r = Properties::compute(molecule);
        QCOMPARE(r.atomCount, 0);
        QVERIFY(r.pattern.isEmpty());
        QVERIFY(molecule.propertyReport().value("formula").toString().isEmpty());
    }

    void unknownElementsAreReportedNotFatal() {
        Molecule molecule;
        molecule.addAtom(0, 0, "Fe");
        const Properties::Report r = Properties::compute(molecule);
        QCOMPARE(r.unknownElements, QStringList{"Fe"});
        QVERIFY(r.pattern.isEmpty());
    }

    void variantExposesEveryField() {
        Molecule molecule;
        QVERIFY(molecule.importSmiles("CCO", 0, 0));
        const QVariantMap map = molecule.propertyReport();
        for (const char *key : {"formula", "molecularWeight", "exactMass", "degreesOfUnsaturation", "hydrogenBondDonors",
                                "hydrogenBondAcceptors", "ruleOfFiveViolations", "tpsa", "elements", "pattern", "adducts"}) {
            QVERIFY2(map.contains(key), key);
        }
        QCOMPARE(map.value("pattern").toList().first().toMap().value("label").toString(), QStringLiteral("M"));
    }

    void labelledIsotopesShiftThePattern() {
        Molecule molecule;
        const int c = molecule.addAtom(0, 0);
        QVERIFY(molecule.setAtomIsotope(c, 13));
        const Properties::Report r = Properties::compute(molecule); // 13CH4
        QCOMPARE(r.pattern.first().nominal, 17);
        QVERIFY(r.formula.contains("[13C]"));
    }
};

QTEST_MAIN(PropertiesTest)

#include "test_properties.moc"
