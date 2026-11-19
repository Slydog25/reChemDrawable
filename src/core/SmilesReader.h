#pragma once

#include "Atom.h"
#include "Bond.h"

#include <QString>
#include <QStringList>
#include <QVector>

// SMILES import. parse() builds the graph (atom ids 1..n, no coordinates);
// build() also lays it out in 2D.
//
// Aromatic rings are kekulized: lowercase atoms become alternating single and
// double bonds, so every formula comes out right with the editor's own hydrogen
// counting (pyrrole's [nH] is a plain NH between two single bonds). A lone
// six-membered aromatic ring gets the "benzene" bond style; other kekulized
// ring systems use plain bonds, because the benzene style draws its inner line
// towards the centre of the whole connected system. If an aromatic system
// cannot be kekulized its bonds stay "aromatic" (order 1, counted as 1.5 for
// hydrogens) and a warning is added.
//
// The editor computes implicit hydrogens itself. A bracket atom that asks for
// more hydrogens than that gets explicit H atoms; one that asks for fewer keeps
// the editor's count and gets a warning.
namespace SmilesReader {

struct Result {
    bool ok = false;
    QString error;          // "SMILES error at position N: reason" (N is 1-based)
    int errorPosition = -1; // 0-based index into the input
    QVector<Atom> atoms;
    QVector<Bond> bonds;
    QStringList warnings;
};

Result parse(const QString &smiles);

// Parses, lays out and centres the structure on (centerX, centerY). Only the
// first whitespace-separated token is read. Returns false and sets *error on failure.
bool build(const QString &smiles, double centerX, double centerY, QVector<Atom> &atoms,
           QVector<Bond> &bonds, QString *error, QStringList *warnings = nullptr);

} // namespace SmilesReader
