#pragma once

#include <QString>
#include <QVector>

// Per-element data shared by valence checks, hydrogen counting, masses, and
// SMILES output. Elements not listed here are accepted (e.g. from an imported
// Molfile) but get a permissive valence, no implicit hydrogens, and no mass.
namespace Elements {

// How formal charge changes the number of bonds an atom can make.
enum class ChargeRule {
    LoseEither,   // C, Si, H, metals: any charge removes a bonding site (C+, C-, Na+)
    Group13,      // B: a negative charge adds a site (BH4-), a positive one removes it
    Group15To17,  // N, O, S, halogens: a positive charge adds a site (NH4+), negative removes
};

struct Info {
    QString symbol;
    QVector<int> normalValences; // ascending; the last entry is the bond limit
    ChargeRule rule;
    bool metal;
    double averageMass;
    double monoisotopicMass;
};

const Info *find(const QString &symbol);

// Most bond-order units the atom may carry with this formal charge.
int maxValence(const QString &symbol, int charge);

// Hydrogens implied on a non-hydrogen atom: fill up to the lowest normal valence
// (shifted by charge) that fits the bond-order sum. Zero for metals, hydrogen,
// and unknown elements.
int implicitHydrogens(const QString &symbol, int charge, int bondOrderSum);

// Elements SMILES may write without brackets.
bool isOrganicSubset(const QString &symbol);

// Elements that may appear lowercase (aromatic) in SMILES.
bool canBeAromatic(const QString &symbol);

} // namespace Elements
