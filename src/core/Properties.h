#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class Molecule;

// Derived molecular properties for the property panel: degrees of unsaturation,
// Lipinski counts, TPSA, elemental analysis and the mass spectrum isotope
// pattern. Pure functions of a Molecule; no UI.
namespace Properties {

struct ElementShare {
    QString symbol;
    int count = 0;
    double mass = 0.0;    // total contribution to the molecular weight
    double percent = 0.0; // mass percent
};

struct Peak {
    int offset = 0;       // nominal mass above the lightest isotopologue (M, M+1, ...)
    int nominal = 0;      // nominal (integer) mass of the peak
    double mass = 0.0;    // intensity-weighted exact mass of the peak
    double relative = 0.0; // percent of the most intense peak
};

struct Adduct {
    QString label;
    double mz = 0.0;
};

struct Report {
    bool selectionOnly = false; // computed for selected connected components only
    int atomCount = 0;          // atoms counted (abbreviation groups count as one)
    QString formula;
    double molecularWeight = 0.0;
    double exactMass = 0.0;     // monoisotopic, same convention as Molecule::properties()
    // Rings + pi bonds. 1 + sum(n_i (v_i - 2) / 2) + charge / 2 with v the lowest
    // normal valence of each element; the charge term makes NH4+ and CH3- give 0.
    double degreesOfUnsaturation = 0.0;
    // Lipinski counting: donors = hydrogens on N and O; acceptors = N and O atoms.
    int hydrogenBondDonors = 0;
    int hydrogenBondAcceptors = 0;
    int ruleOfFiveViolations = 0; // of MW <= 500, HBD <= 5, HBA <= 10 (logP is not computed)
    // Ertl, Rohde, Selzer, J. Med. Chem. 2000, 43, 3714: N and O contributions,
    // and the same plus S and P.
    double tpsa = 0.0;
    double tpsaWithSulfurPhosphorus = 0.0;
    int tpsaUnmatched = 0;      // N/O atoms in environments missing from the table (counted as 0)
    QVector<ElementShare> elements;
    QVector<Peak> pattern;
    QVector<Adduct> adducts;    // [M+H]+, [M+Na]+, [M-H]-
    QStringList unknownElements; // no mass or isotope data; left out of masses and pattern
};

// Properties of the whole drawing, or, when atomIds is non-empty, of the
// connected components that contain those atoms.
Report compute(const Molecule &molecule, const QVector<int> &atomIds = {});

QVariantMap toVariant(const Report &report);

} // namespace Properties
