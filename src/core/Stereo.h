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

// Atomic number used for ranking. Unknown symbols such as abbreviation labels
// ("OMe") are ranked by their leading element symbol; 0 when nothing matches.
int rankingAtomicNumber(const QString &element);

} // namespace Stereo
