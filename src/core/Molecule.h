#pragma once

#include "Atom.h"
#include "Bond.h"

#include <QObject>
#include <QPointF>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

struct GenericRingTemplate;

class Molecule : public QObject {
    Q_OBJECT
    Q_PROPERTY(double standardBondLength READ standardBondLength CONSTANT)

public:
    // Oldest undo snapshots are dropped past this many steps; each one is a full
    // copy of the structure, and the Paper Pro has 2 GB of RAM.
    static constexpr int MaxUndoSteps = 200;

    explicit Molecule(QObject *parent = nullptr);

    const QVector<Atom> &atomData() const;
    const QVector<Bond> &bondData() const;
    void replaceContent(const QVector<Atom> &atoms, const QVector<Bond> &bonds);

    Q_INVOKABLE QVariantList atoms() const;
    Q_INVOKABLE QVariantList bonds() const;
    Q_INVOKABLE int addAtom(double x, double y, const QString &element = QStringLiteral("C"));
    Q_INVOKABLE int addBond(int atomA, int atomB, int order = 1);
    // Adds a new atom single-bonded to atomId as one undoable step. Returns the
    // new atom's id, or -1 (with no change) when atomId has no free valence.
    Q_INVOKABLE int extendFromAtom(int atomId, double x, double y, const QString &element = QStringLiteral("C"));
    Q_INVOKABLE bool cycleBondOrder(int bondId);
    Q_INVOKABLE bool removeAtom(int atomId);
    Q_INVOKABLE bool removeBond(int bondId);
    Q_INVOKABLE bool setAtomElement(int atomId, const QString &element);
    Q_INVOKABLE bool setAtomCharge(int atomId, int charge);
    Q_INVOKABLE bool setAtomIsotope(int atomId, int isotope);
    Q_INVOKABLE bool moveAtom(int atomId, double x, double y);
    Q_INVOKABLE bool translateAtoms(const QVariantList &atomIds, double dx, double dy);
    Q_INVOKABLE bool rotateAtoms(const QVariantList &atomIds, double degrees);
    Q_INVOKABLE bool mirrorAtoms(const QVariantList &atomIds, bool horizontal);
    Q_INVOKABLE bool alignAtoms(const QVariantList &atomIds, const QString &alignment);
    Q_INVOKABLE bool removeAtoms(const QVariantList &atomIds);
    Q_INVOKABLE bool copyAtoms(const QVariantList &atomIds);
    Q_INVOKABLE QVariantList pasteAtoms(double x, double y);
    Q_INVOKABLE bool setBondStyle(int bondId, const QString &style);
    Q_INVOKABLE bool cycleBondStyle(int bondId);
    // Double bonds: 0 centred, +1 / -1 second line to either side of the atom axis.
    Q_INVOKABLE bool setBondDoubleSide(int bondId, int side);
    Q_INVOKABLE bool cycleBondDoubleSide(int bondId);
    // Ids of the atoms inside a closed polygon of {x, y} points (even-odd rule).
    Q_INVOKABLE QVariantList atomsInPolygon(const QVariantList &points) const;
    // Copies the given atoms and the bonds between them, offset by (dx, dy), as
    // one undoable step. Does not touch the clipboard. Returns the new atom ids.
    Q_INVOKABLE QVariantList duplicateAtoms(const QVariantList &atomIds, double dx, double dy);
    Q_INVOKABLE int atomAt(double x, double y, double radius = 20.0) const;
    Q_INVOKABLE int bondAt(double x, double y, double tolerance = 12.0) const;
    Q_INVOKABLE QPointF snappedBondEnd(int atomId, double x, double y) const;
    Q_INVOKABLE QPointF suggestedBondEnd(int atomId) const;
    Q_INVOKABLE bool stampRing(const QString &templateName, double x, double y);
    Q_INVOKABLE bool stampRingAtAtom(const QString &templateName, int atomId, double x, double y);
    Q_INVOKABLE bool stampRingAtBond(const QString &templateName, int bondId, double x, double y);
    // Zig-zag chain: bonds one standard length long at +-30 degrees to the axis
    // (angle, in degrees, y down), first bend toward firstBendSign (+1 clockwise
    // on screen, -1 anticlockwise). chainPoints() lists the atom positions
    // (start atom first when startAtomId >= 0) so the UI can preview them;
    // addChain() creates `count` new carbons as one undoable step and returns
    // the last new atom's id, or -1 (with a message) if the start atom is full.
    Q_INVOKABLE QVariantList chainPoints(int startAtomId, double x, double y, double angle, int count, int firstBendSign) const;
    Q_INVOKABLE int addChain(int startAtomId, double x, double y, double angle, int count, int firstBendSign);
    Q_INVOKABLE bool stampFunctionalGroup(const QString &group, int atomId);
    Q_INVOKABLE bool cleanUpStructure();
    Q_INVOKABLE bool canAddBond(int atomA, int atomB, int order = 1) const;
    Q_INVOKABLE QString formula() const;
    Q_INVOKABLE QString smiles() const;
    // CIP labels perceived from the drawing: [{atomId or bondId, kind, label, text, x, y}].
    Q_INVOKABLE QVariantList stereoDescriptors() const;
    double standardBondLength() const;
    // Hydrogens implied on a non-hydrogen atom by its element, charge, and bonds.
    int implicitHydrogens(const Atom &atom) const;
    Q_INVOKABLE QVariantMap properties() const;
    Q_INVOKABLE bool saveToFile(const QString &path);
    Q_INVOKABLE bool loadFromFile(const QString &path);
    Q_INVOKABLE bool saveSvgToFile(const QString &path);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE void clear();

signals:
    void changed();
    void message(const QString &text);
    // Emitted once for every new undoable edit (not for undo/redo themselves).
    void committed();
    // Emitted when the undo/redo stacks are discarded, e.g. after loading a file.
    void historyReset();

private:
    struct Snapshot {
        QVector<Atom> atoms;
        QVector<Bond> bonds;
        int nextAtomId = 1;
        int nextBondId = 1;
    };

    static const GenericRingTemplate *findRingTemplate(const QString &name);
    // Stamps a generic template (heterocycle, chair, indole): free at (x, y) when
    // anchorId < 0, else as a substituent bonded to anchorId.
    bool stampTemplate(const GenericRingTemplate &ring, int anchorId, double x, double y);

    Atom *findAtom(int id);
    const Atom *findAtom(int id) const;
    Bond *findBond(int id);
    const Bond *findBond(int id) const;
    int valenceUsed(int atomId) const;
    int maxValence(const QString &element, int charge = 0) const;
    int maxValence(const Atom &atom) const;
    Snapshot snapshot() const;
    void restore(const Snapshot &snapshot);
    void commit(const Snapshot &before);

    QVector<Atom> m_atoms;
    QVector<Bond> m_bonds;
    QVector<Snapshot> m_undo;
    QVector<Snapshot> m_redo;
    QVector<Atom> m_clipboardAtoms;
    QVector<Bond> m_clipboardBonds;
    int m_nextAtomId = 1;
    int m_nextBondId = 1;
};
