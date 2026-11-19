#include "EditHistory.h"

#include "Molecule.h"
#include "ReactionLayer.h"
#include "SchemeAnalysis.h"

#include <algorithm>

EditHistory::EditHistory(Molecule *molecule, ReactionLayer *reactions, QObject *parent)
    : QObject(parent), m_molecule(molecule), m_reactions(reactions) {
    connect(m_molecule, &Molecule::committed, this, [this] { record(Layer::Molecule); });
    connect(m_molecule, &Molecule::historyReset, this, [this] { forget(Layer::Molecule); });
    connect(m_reactions, &ReactionLayer::committed, this, [this] { record(Layer::Reactions); });
    connect(m_reactions, &ReactionLayer::historyReset, this, [this] { forget(Layer::Reactions); });
}

bool EditHistory::undo() {
    while (!m_undo.isEmpty()) {
        const Step step = m_undo.takeLast();
        bool applied = false;
        for (auto it = step.crbegin(); it != step.crend(); ++it) applied = undoLayer(*it) || applied;
        if (applied) {
            m_redo.append(step);
            return true;
        }
    }
    return false;
}

bool EditHistory::redo() {
    while (!m_redo.isEmpty()) {
        const Step step = m_redo.takeLast();
        bool applied = false;
        for (const Layer layer : step) applied = redoLayer(layer) || applied;
        if (applied) {
            m_undo.append(step);
            return true;
        }
    }
    return false;
}

void EditHistory::clearAll() {
    beginGroup();
    m_molecule->clear();
    m_reactions->clear();
    endGroup();
}

bool EditHistory::tidyScheme() {
    if (m_grouping) return false;
    const SchemeAnalysis::Tidy plan = SchemeAnalysis::tidyPlan(*m_molecule, m_reactions->items());
    if (plan.isEmpty()) return false;
    beginGroup();
    for (const SchemeAnalysis::Tidy::ArrowEnds &ends : plan.arrows) m_reactions->setArrowEnds(ends.id, ends.x1, ends.y1, ends.x2, ends.y2);
    for (const SchemeAnalysis::Tidy::AtomMove &move : plan.atoms) {
        QVariantList ids;
        for (int id : move.atomIds) ids.append(id);
        m_molecule->translateAtoms(ids, move.dx, move.dy);
    }
    for (const SchemeAnalysis::Tidy::ItemMove &move : plan.items) m_reactions->translateItem(move.id, move.dx, move.dy);
    endGroup();
    return true;
}

void EditHistory::beginGroup() {
    if (m_grouping) return;
    m_grouping = true;
    m_group.clear();
}

void EditHistory::endGroup() {
    if (!m_grouping) return;
    m_grouping = false;
    if (m_group.isEmpty()) return;
    m_undo.append(m_group);
    if (m_undo.size() > MaxSteps) m_undo.removeFirst();
    m_redo.clear();
    m_group.clear();
}

void EditHistory::record(Layer layer) {
    if (m_grouping) {
        m_group.append(layer);
        return;
    }
    m_undo.append(Step{layer});
    // Each layer keeps at most its own MaxUndoSteps; tags older than that would
    // only be skipped by undo(), so don't keep many more of them.
    if (m_undo.size() > MaxSteps) m_undo.removeFirst();
    m_redo.clear();
}

void EditHistory::forget(Layer layer) {
    const auto strip = [layer](QVector<Step> &steps) {
        for (Step &step : steps) step.removeAll(layer);
        steps.erase(std::remove_if(steps.begin(), steps.end(), [](const Step &step) { return step.isEmpty(); }),
                    steps.end());
    };
    strip(m_undo);
    strip(m_redo);
}

bool EditHistory::undoLayer(Layer layer) {
    return layer == Layer::Molecule ? m_molecule->undo() : m_reactions->undo();
}

bool EditHistory::redoLayer(Layer layer) {
    return layer == Layer::Molecule ? m_molecule->redo() : m_reactions->redo();
}
