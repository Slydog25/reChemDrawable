#pragma once

#include "Atom.h"
#include "Bond.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class Molecule;

// Ready-made fragments the user can place with one tap, in categories:
//   projections  Newman, Fischer and Haworth drawings
//   amino        the 20 standard L-amino acids (neutral forms, zig-zag backbone, L wedge/dash)
//   bases        adenine, guanine, cytosine, thymine, uracil
//   reagents     a few common reagents
//   mine         fragments the user saved from the canvas ("My templates")
//
// Built-in templates are generated in code (SMILES plus the project's own 2D layout, or fixed
// coordinates for the drawing conventions) and cached. "My templates" are kept in
// templates.json in the documents folder: atoms and bonds with positions relative to the
// fragment's centroid, plus the bond drawing data a Molfile cannot hold.
//
// Placing a template is one undoable step on the Molecule.
//
// Drawing conventions use ordinary atoms and bonds so formula, MW and SMILES stay right:
//   Haworth  ring bonds on the front edge have the "bold" style.
//   Newman   the front carbon (three bonds) and the rear carbon sit about 2 px apart at the
//            circle centre; the bond between them has the "newman" style, which draws the
//            circle, and the rear carbon's three bonds have the "newmanback" style, which
//            starts them at the circle.
//   Fischer  vertical chain of carbons (hidden, drawn at the crossings) with horizontal
//            bonds to explicit H/OH atoms.
class TemplateLibrary : public QObject {
    Q_OBJECT
    Q_PROPERTY(int mineCount READ mineCount NOTIFY mineChanged)

public:
    // An empty directory means Documents/reChemDrawable.
    explicit TemplateLibrary(Molecule *molecule, const QString &directory = QString(), QObject *parent = nullptr);

    QString filePath() const;
    int mineCount() const { return m_mine.size(); }

    // [{id, name}] in display order; "mine" is always last.
    Q_INVOKABLE QVariantList categories() const;
    // [{id, name, label, formula, category, deletable}] for one category.
    Q_INVOKABLE QVariantList items(const QString &category) const;
    // {atoms: [{x, y, element, charge}], bonds: [{a, b, order, style}]} (indices, canvas
    // coordinates around the centroid) for drawing a small preview; empty for an unknown id.
    Q_INVOKABLE QVariantMap preview(const QString &id) const;
    Q_INVOKABLE QString formulaOf(const QString &id) const;
    // Places the template with its centre at (x, y) as one undoable step. Returns the new
    // atom ids (empty for an unknown id).
    Q_INVOKABLE QVariantList place(const QString &id, double x, double y);
    // Stores the selected atoms and the bonds between them as a new "mine" template.
    // Returns false (with a message) for an empty selection or a failed write.
    Q_INVOKABLE bool saveSelection(const QVariantList &atomIds);
    Q_INVOKABLE bool deleteMine(const QString &id);

    // Formula of a fragment, computed without touching the canvas molecule.
    static QString fragmentFormula(const QVector<Atom> &atoms, const QVector<Bond> &bonds);

    struct Fragment {
        QString id, category, name, label, formula;
        QVector<Atom> atoms;   // ids 1..n, positions relative to the centroid
        QVector<Bond> bonds;   // ends are atom ids
    };

signals:
    void mineChanged();
    void message(const QString &text);

private:
    static const QVector<Fragment> &builtins();
    const Fragment *find(const QString &id) const;
    void loadMine();
    bool writeMine();

    Molecule *m_molecule;
    QString m_directory;
    QVector<Fragment> m_mine;
    int m_nextMineId = 1;
};
