#pragma once

#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVariantList>
#include <QVector>

class Molecule;

// Pure analysis of a drawn reaction scheme: connected components, the reaction
// balance check, and the plan for the tidy command. Nothing here edits anything.
namespace SchemeAnalysis {

struct Component {
    QVector<int> atomIndices; // indices into Molecule::atomData()
    QVector<int> atomIds;
    QPointF centroid;
    QRectF bounds;            // of the atom positions
};

// Atoms joined by bonds (partial bonds included) form one component.
QVector<Component> components(const Molecule &molecule);

struct Composition {
    QMap<QString, int> elements; // implicit hydrogens and abbreviations included
    int charge = 0;
};
Composition composition(const Molecule &molecule, const QVector<int> &atomIndices);
// Hill-style formula text of a composition ("C2H4Br2", charge appended).
QString formulaText(const Composition &composition);

// One map per straight reaction arrow (forward, equilibrium, noreaction, dashed):
// {arrow, kind, balanced, incomplete, reactants, products, text,
//  differences: [{element, reactants, products}], chargeReactants, chargeProducts}.
// Reactants are the components before the arrow's tail, products those after its
// head, each within a band around the arrow and not separated from it by another arrow.
QVariantList checkReactions(const Molecule &molecule, const QVariantList &items);

struct Tidy {
    struct AtomMove { QVector<int> atomIds; double dx = 0.0, dy = 0.0; };
    struct ItemMove { int id = -1; double dx = 0.0, dy = 0.0; };
    struct ArrowEnds { int id = -1; double x1 = 0, y1 = 0, x2 = 0, y2 = 0; };
    QVector<AtomMove> atoms;
    QVector<ArrowEnds> arrows;
    QVector<ItemMove> items;
    bool isEmpty() const { return atoms.isEmpty() && arrows.isEmpty() && items.isEmpty(); }
};
// Left-to-right layout on one baseline; see EditHistory::tidyScheme.
Tidy tidyPlan(const Molecule &molecule, const QVariantList &items);

} // namespace SchemeAnalysis
