#include "SmilesWriter.h"

#include "Abbreviations.h"
#include "Elements.h"
#include "Molecule.h"
#include "Stereo.h"

#include <algorithm>
#include <QHash>
#include <QPointF>
#include <QSet>
#include <QVector>

namespace {
struct Edge {
    int neighbor;
    int bondIndex;
};

struct Closure {
    int opener;
    int closer;
    int bondIndex;
};

class Writer {
public:
    explicit Writer(const Molecule &molecule) : m_molecule(molecule) {
        const QVector<Atom> &atoms = molecule.atomData();
        for (int index = 0; index < atoms.size(); ++index) m_atomIndex.insert(atoms[index].id, index);
        const QVector<Bond> &bonds = molecule.bondData();
        for (int index = 0; index < bonds.size(); ++index) {
            const Bond &bond = bonds[index];
            if (!m_atomIndex.contains(bond.atomA) || !m_atomIndex.contains(bond.atomB) || bond.atomA == bond.atomB) continue;
            // Partial (forming/breaking) bonds are not bonds yet/any more: not part of the SMILES.
            if (bond.style == QStringLiteral("partial")) continue;
            m_edges[bond.atomA].append({bond.atomB, index});
            m_edges[bond.atomB].append({bond.atomA, index});
            if (bond.style == QStringLiteral("aromatic")) {
                m_aromaticAtoms.insert(bond.atomA);
                m_aromaticAtoms.insert(bond.atomB);
            }
        }
    }

    QString write() {
        // Pass 1: spanning trees and ring closures of every component.
        QVector<int> starts;
        for (const Atom &atom : m_molecule.atomData()) {
            if (m_visited.contains(atom.id)) continue;
            // A group fragment's trailing characters would attach to its last atom,
            // so start from a real atom when the component has one.
            int start = atom.id;
            if (isAbbreviation(start)) {
                for (const Edge &edge : m_edges.value(start))
                    if (!isAbbreviation(edge.neighbor)) { start = edge.neighbor; break; }
            }
            // Phase 1 finds the spanning tree and ring-closure bonds of this
            // component; phase 2 emits it with ring digits and branches.
            findTree(start, -1);
            starts.append(start);
        }
        // Stereo marks need the written order of every neighbour, so they come
        // after all the trees are known.
        computeStereo();
        QString result;
        for (const int start : starts) {
            if (!result.isEmpty()) result += '.';
            writeAtom(start, result);
        }
        return result;
    }

private:
    void findTree(int atomId, int parentBond) {
        m_visited.insert(atomId);
        for (const Edge &edge : m_edges.value(atomId)) {
            if (edge.bondIndex == parentBond || m_seenBonds.contains(edge.bondIndex)) continue;
            m_seenBonds.insert(edge.bondIndex);
            if (m_visited.contains(edge.neighbor)) {
                // Undirected DFS: every non-tree edge closes a ring back to an ancestor.
                m_closures.append({edge.neighbor, atomId, edge.bondIndex});
            } else {
                m_children[atomId].append(edge);
                m_parent.insert(edge.neighbor, atomId);
                findTree(edge.neighbor, edge.bondIndex);
            }
        }
    }

    // Neighbours of an atom in the order the SMILES lists them: the atom written
    // before it, the implicit hydrogen (0), ring-closure digits (closing ones
    // first, as writeAtom emits them), then the branches and the chain.
    QVector<int> neighbourOrder(int atomId) const {
        QVector<int> order;
        if (m_parent.contains(atomId)) order.append(m_parent.value(atomId));
        const Atom &atom = m_molecule.atomData()[m_atomIndex.value(atomId)];
        if (m_molecule.implicitHydrogens(atom) == 1) order.append(0);
        for (const Closure &closure : m_closures) if (closure.closer == atomId) order.append(closure.opener);
        for (const Closure &closure : m_closures) if (closure.opener == atomId) order.append(closure.closer);
        for (const Edge &child : m_children.value(atomId)) order.append(child.neighbor);
        return order;
    }

    bool bondInRing(int bondIndex) const {
        const Bond &bond = m_molecule.bondData()[bondIndex];
        QSet<int> seen{bond.atomA};
        QVector<int> queue{bond.atomA};
        for (int head = 0; head < queue.size(); ++head) {
            for (const Edge &edge : m_edges.value(queue[head])) {
                if (edge.bondIndex == bondIndex) continue;
                if (edge.neighbor == bond.atomB) return true;
                if (!seen.contains(edge.neighbor)) { seen.insert(edge.neighbor); queue.append(edge.neighbor); }
            }
        }
        return false;
    }

    QPointF position(int atomId) const {
        const Atom &atom = m_molecule.atomData()[m_atomIndex.value(atomId)];
        return QPointF(atom.x, atom.y);
    }

    // Tetrahedral parity of every centre drawn with a wedge or dash, and the
    // slash marks that make acyclic double bonds cis or trans as drawn.
    void computeStereo() {
        const QVector<Bond> &bonds = m_molecule.bondData();
        for (const Atom &atom : m_molecule.atomData()) {
            if (isAbbreviation(atom.id)) continue;
            bool anchored = false;
            for (const Edge &edge : m_edges.value(atom.id)) {
                const Bond &bond = bonds[edge.bondIndex];
                if (bond.atomA == atom.id && (bond.style == QStringLiteral("wedge") || bond.style == QStringLiteral("dash"))) anchored = true;
            }
            if (!anchored) continue;
            const int parity = Stereo::chiralParity(m_molecule, atom.id, neighbourOrder(atom.id));
            if (parity != 0) m_parity.insert(atom.id, parity);
        }

        // A tree bond is written parent -> child; `up` is the direction of the
        // neighbour as seen from `end`.
        const auto treeBond = [&](int bondIndex, int &parent) {
            const Bond &bond = bonds[bondIndex];
            if (m_parent.value(bond.atomB, -1) == bond.atomA) { parent = bond.atomA; return true; }
            if (m_parent.value(bond.atomA, -1) == bond.atomB) { parent = bond.atomB; return true; }
            return false;
        };
        const auto upFromMark = [&](int bondIndex, int end) {
            int parent = 0;
            treeBond(bondIndex, parent);
            const bool slash = m_marks.value(bondIndex) == QStringLiteral("/");
            return end == parent ? slash : !slash;
        };
        const auto markFor = [&](int bondIndex, int end, bool up) {
            int parent = 0;
            treeBond(bondIndex, parent);
            const bool slash = end == parent ? up : !up;
            return slash ? QStringLiteral("/") : QStringLiteral("\\");
        };
        struct Sub { int bondIndex = -1; int neighbour = -1; int side = 0; };
        for (int index = 0; index < bonds.size(); ++index) {
            const Bond &bond = bonds[index];
            if (bond.order != 2 || bond.style == QStringLiteral("wavy") || bond.style == QStringLiteral("aromatic")) continue;
            int unused = 0;
            if (!treeBond(index, unused) || bondInRing(index)) continue;
            if (!m_atomIndex.contains(bond.atomA) || !m_atomIndex.contains(bond.atomB)) continue;
            const QPointF pa = position(bond.atomA), pb = position(bond.atomB);
            const auto sideOf = [&](const QPointF &p) {
                const double length = std::hypot(pb.x() - pa.x(), pb.y() - pa.y());
                if (length < 1e-6) return 0;
                const double cross = ((pb.x() - pa.x()) * (p.y() - pa.y()) - (pb.y() - pa.y()) * (p.x() - pa.x())) / length;
                return cross > 4.0 ? 1 : cross < -4.0 ? -1 : 0;
            };
            // The marked or markable substituent of one end; ok = false for cumulenes etc.
            const auto analyse = [&](int end, bool &ok) {
                ok = false;
                QVector<Sub> subs;
                const QString &element = m_molecule.atomData()[m_atomIndex.value(end)].element;
                if (element != QStringLiteral("C") && element != QStringLiteral("N") && element != QStringLiteral("Si")) return subs;
                for (const Edge &edge : m_edges.value(end)) {
                    if (edge.bondIndex == index) continue;
                    const Bond &other = bonds[edge.bondIndex];
                    if (other.order > 1) return subs;
                    if (other.style == QStringLiteral("aromatic")) return subs;
                    int parent = 0;
                    if (!treeBond(edge.bondIndex, parent)) continue; // ring-closure bonds carry no marks
                    if (other.style == QStringLiteral("wavy") || isAbbreviation(edge.neighbor)) continue;
                    subs.append({edge.bondIndex, edge.neighbor, sideOf(position(edge.neighbor))});
                }
                ok = true;
                return subs;
            };
            bool okA = false, okB = false;
            const QVector<Sub> subsA = analyse(bond.atomA, okA), subsB = analyse(bond.atomB, okB);
            if (!okA || !okB) continue;
            // Prefer a substituent that already carries a mark from a neighbouring double bond.
            const auto pick = [&](const QVector<Sub> &subs) {
                Sub chosen;
                for (const Sub &sub : subs) if (sub.side != 0 && m_marks.contains(sub.bondIndex)) return sub;
                for (const Sub &sub : subs) if (sub.side != 0) { chosen = sub; break; }
                return chosen;
            };
            const Sub x = pick(subsA), y = pick(subsB);
            if (x.bondIndex < 0 || y.bondIndex < 0) continue;
            const bool markedX = m_marks.contains(x.bondIndex), markedY = m_marks.contains(y.bondIndex);
            if (markedX && markedY) continue;
            bool upX, upY;
            if (markedX) { upX = upFromMark(x.bondIndex, bond.atomA); upY = x.side == y.side ? upX : !upX; }
            else if (markedY) { upY = upFromMark(y.bondIndex, bond.atomB); upX = x.side == y.side ? upY : !upY; }
            else { upX = true; upY = x.side == y.side ? upX : !upX; }
            if (!markedX) m_marks.insert(x.bondIndex, markFor(x.bondIndex, bond.atomA, upX));
            if (!markedY) m_marks.insert(y.bondIndex, markFor(y.bondIndex, bond.atomB, upY));
        }
    }

    bool isAbbreviation(int atomId) const {
        return Abbreviations::isAbbreviation(m_molecule.atomData()[m_atomIndex.value(atomId)].element);
    }

    // An abbreviation that is a component's root (only two linked groups) has its
    // child attached right after the fragment's first atom (and its ring digits).
    void writeGroupRoot(int atomId, QString &out) {
        const QString fragment = atomSymbol(atomId);
        int split = fragment.startsWith('[') ? fragment.indexOf(']') + 1 : 1;
        for (;;) {
            if (split < fragment.size() && fragment[split].isDigit()) ++split;
            else if (split + 2 < fragment.size() && fragment[split] == '%' && fragment[split + 1].isDigit() && fragment[split + 2].isDigit()) split += 3;
            else break;
        }
        out += fragment.left(split);
        for (const Edge &child : m_children.value(atomId)) {
            out += '(' + bondSymbol(child.bondIndex);
            writeAtom(child.neighbor, out);
            out += ')';
        }
        out += fragment.mid(split);
    }

    void writeAtom(int atomId, QString &out) {
        if (isAbbreviation(atomId) && !m_children.value(atomId).isEmpty()) { writeGroupRoot(atomId, out); return; }
        out += atomSymbol(atomId);
        for (const Closure &closure : m_closures) {
            if (closure.closer != atomId) continue;
            const int digit = m_openDigits.take(closure.bondIndex);
            out += ringDigit(digit);
            m_freeDigits.append(digit);
        }
        for (const Closure &closure : m_closures) {
            if (closure.opener != atomId) continue;
            const int digit = takeDigit();
            m_openDigits.insert(closure.bondIndex, digit);
            out += bondSymbol(closure.bondIndex) + ringDigit(digit);
        }
        const QVector<Edge> children = m_children.value(atomId);
        for (int index = 0; index < children.size(); ++index) {
            const bool branch = index + 1 < children.size();
            if (branch) out += '(';
            out += bondSymbol(children[index].bondIndex);
            writeAtom(children[index].neighbor, out);
            if (branch) out += ')';
        }
    }

    int takeDigit() {
        if (!m_freeDigits.isEmpty()) {
            std::sort(m_freeDigits.begin(), m_freeDigits.end());
            return m_freeDigits.takeFirst();
        }
        return ++m_highestDigit;
    }

    static QString ringDigit(int digit) {
        return digit < 10 ? QString::number(digit) : QStringLiteral("%") + QString::number(digit);
    }

    bool isAromatic(int atomId) const {
        const Atom &atom = m_molecule.atomData()[m_atomIndex.value(atomId)];
        return m_aromaticAtoms.contains(atomId) && Elements::canBeAromatic(atom.element);
    }

    QString bondSymbol(int bondIndex) const {
        const Bond &bond = m_molecule.bondData()[bondIndex];
        const bool bothAromatic = isAromatic(bond.atomA) && isAromatic(bond.atomB);
        if (bond.order == 1 && m_marks.contains(bondIndex)) return m_marks.value(bondIndex);
        if (bond.style == QStringLiteral("aromatic") && bothAromatic) return QString();
        if (bond.order == 2) return QStringLiteral("=");
        if (bond.order == 3) return QStringLiteral("#");
        // A single bond between two aromatic atoms (e.g. biphenyl) must be explicit.
        return bothAromatic ? QStringLiteral("-") : QString();
    }

    QString atomSymbol(int atomId) const {
        const Atom &atom = m_molecule.atomData()[m_atomIndex.value(atomId)];
        if (const Abbreviations::Info *group = Abbreviations::find(atom.element)) return group->smiles;
        const bool aromatic = isAromatic(atomId);
        const QString symbol = aromatic ? atom.element.toLower() : atom.element;
        // Neutral, unlabelled organic-subset atoms imply exactly the hydrogens the
        // editor computes, so they can be written bare.
        const int parity = m_parity.value(atomId, 0);
        if (parity == 0 && Elements::isOrganicSubset(atom.element) && atom.formalCharge == 0 && atom.isotope == 0 && atom.radical == 0) return symbol;
        QString bracket = QStringLiteral("[");
        if (atom.isotope) bracket += QString::number(atom.isotope);
        bracket += symbol;
        if (parity > 0) bracket += '@';
        else if (parity < 0) bracket += QStringLiteral("@@");
        const int hydrogens = m_molecule.implicitHydrogens(atom);
        if (hydrogens == 1) bracket += 'H';
        else if (hydrogens > 1) bracket += QStringLiteral("H%1").arg(hydrogens);
        if (atom.formalCharge > 0) bracket += '+';
        if (atom.formalCharge < 0) bracket += '-';
        if (qAbs(atom.formalCharge) > 1) bracket += QString::number(qAbs(atom.formalCharge));
        return bracket + ']';
    }

    const Molecule &m_molecule;
    QHash<int, int> m_atomIndex;
    QHash<int, QVector<Edge>> m_edges;
    QHash<int, QVector<Edge>> m_children;
    QHash<int, int> m_parent;      // tree parent of every non-root atom
    QHash<int, int> m_parity;      // atom id -> +1 (@) / -1 (@@)
    QHash<int, QString> m_marks;   // bond index -> "/" or "\\", written parent -> child
    QSet<int> m_aromaticAtoms;
    QSet<int> m_visited;
    QSet<int> m_seenBonds;
    QVector<Closure> m_closures;
    QHash<int, int> m_openDigits;
    QVector<int> m_freeDigits;
    int m_highestDigit = 0;
};
}

namespace SmilesWriter {
QString write(const Molecule &molecule) { return Writer(molecule).write(); }
}
