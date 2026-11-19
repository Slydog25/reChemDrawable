#pragma once

#include <QPointF>
#include <QVector>

// Turns a bare molecular graph into 2D coordinates (y down, bonds one standard
// bond length). Rings are placed as regular polygons, fused rings are built onto
// the shared bond on the outside, spiro rings around the shared atom, and chains
// and substituents are zig-zagged into the largest free gap.
namespace Layout2D {

struct Edge {
    int a = 0;
    int b = 0;
    int order = 1;
};

// A smallest set of smallest rings; each ring lists its atoms in cycle order.
QVector<QVector<int>> findRings(int atomCount, const QVector<Edge> &edges);

// One position per atom. Disconnected components are laid out side by side,
// centred on the origin.
QVector<QPointF> place(int atomCount, const QVector<Edge> &edges, double bondLength = 48.0);

} // namespace Layout2D
