#pragma once

#include <QString>
#include <QVector>

class Molecule;

// CIP stereodescriptors perceived from the 2D drawing: R/S for tetrahedral
// centres drawn with a wedge or dash that starts at the centre, and E/Z for
// acyclic (or large-ring) double bonds. Ranking implements sequence rules 1a
// (atomic number, hierarchical digraph with duplicated atoms) and 1b (mass
// number). When a tie survives those rules the centre gets no descriptor.
namespace Stereo {

struct Descriptor {
    int atomId = -1;   // set for R/S
    int bondId = -1;   // set for E/Z
    QString label;     // "R", "S", "E" or "Z"
    double x = 0.0;    // suggested centre of the label, canvas coordinates
    double y = 0.0;
};

QVector<Descriptor> compute(const Molecule &molecule);

// One substituent of a tetrahedral centre for chiralParity(): its drawn 2D
// direction (screen coordinates, y down; need not be normalised), z = +1 for a
// wedge (toward the viewer), -1 for a dash, 0 for a plain bond. `implicit`
// marks the position of an implicit hydrogen or lone pair; its direction is
// taken as the opposite of the sum of the other arms.
struct Arm {
    double dx = 0.0;
    double dy = 0.0;
    int z = 0;
    bool implicit = false;
};

// Handedness of four arms listed in SMILES neighbour order: +1 when, looking
// from the first arm toward the centre, the other three run anticlockwise (SMILES
// "@"), -1 for clockwise ("@@"), 0 when the drawing is degenerate. Needs exactly
// four arms and at most one implicit.
int chiralParity(const QVector<Arm> &arms);

// chiralParity() for a drawn centre. `order` lists the neighbour atom ids in
// SMILES order, with 0 standing for the implicit hydrogen. Returns 0 unless the
// centre qualifies for R/S (a wedge or dash starts at it, no multiple bonds, a
// tetrahedral-capable atom) and the order matches its neighbours.
int chiralParity(const Molecule &molecule, int centerId, const QVector<int> &order);

// Atomic number used for ranking. Unknown symbols such as abbreviation labels
// ("OMe") are ranked by their leading element symbol; 0 when nothing matches.
int rankingAtomicNumber(const QString &element);

} // namespace Stereo
