#pragma once

#include <QString>

class Molecule;

// Writes a (non-canonical) SMILES string for the drawn structure. Atoms are
// visited in drawing order, so the same drawing always gives the same string.
// Bonds drawn with the "aromatic" style become lowercase aromatic atoms; the
// benzene template is written in Kekule form. Wedge/dash stereo is not encoded.
namespace SmilesWriter {
QString write(const Molecule &molecule);
}
