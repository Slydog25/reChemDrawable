#pragma once

#include <QObject>
#include <QVector>

class Molecule;
class ReactionLayer;

// One undo/redo timeline across the molecule and its reaction annotations.
// Each layer keeps its own snapshots; this class only records which layer
// each edit belonged to, so Undo always reverses the most recent edit.
class EditHistory : public QObject {
    Q_OBJECT

public:
    EditHistory(Molecule *molecule, ReactionLayer *reactions, QObject *parent = nullptr);

    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    // Clears the molecule and the annotations as a single undoable step.
    Q_INVOKABLE void clearAll();
    // Edits between beginGroup() and endGroup() undo and redo as one step
    // (e.g. everything one eraser stroke removed). Groups do not nest.
    Q_INVOKABLE void beginGroup();
    Q_INVOKABLE void endGroup();
    // Lays the scheme out left to right on one baseline (molecules vertically centred,
    // arrows ~3 bond lengths long between them, arrow labels moved along) as one undo
    // step. Returns false when there is nothing to move.
    Q_INVOKABLE bool tidyScheme();

private:
    static constexpr int MaxSteps = 400;
    enum class Layer { Molecule, Reactions };
    using Step = QVector<Layer>;

    void record(Layer layer);
    void forget(Layer layer);
    bool undoLayer(Layer layer);
    bool redoLayer(Layer layer);

    Molecule *m_molecule;
    ReactionLayer *m_reactions;
    QVector<Step> m_undo;
    QVector<Step> m_redo;
    bool m_grouping = false;
    Step m_group;
};
