#include "SmilesWriter.h"

#include "Elements.h"
#include "Molecule.h"

#include <algorithm>
#include <QHash>
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
            m_edges[bond.atomA].append({bond.atomB, index});
            m_edges[bond.atomB].append({bond.atomA, index});
            if (bond.style == QStringLiteral("aromatic")) {
                m_aromaticAtoms.insert(bond.atomA);
                m_aromaticAtoms.insert(bond.atomB);
            }
        }
    }

    QString write() {
        QString result;
        for (const Atom &atom : m_molecule.atomData()) {
            if (m_visited.contains(atom.id)) continue;
            // Phase 1 finds the spanning tree and ring-closure bonds of this
            // component; phase 2 emits it with ring digits and branches.
            findTree(atom.id, -1);
            if (!result.isEmpty()) result += '.';
            writeAtom(atom.id, result);
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
                findTree(edge.neighbor, edge.bondIndex);
            }
        }
    }

    void writeAtom(int atomId, QString &out) {
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
        if (bond.style == QStringLiteral("aromatic") && bothAromatic) return QString();
        if (bond.order == 2) return QStringLiteral("=");
        if (bond.order == 3) return QStringLiteral("#");
        // A single bond between two aromatic atoms (e.g. biphenyl) must be explicit.
        return bothAromatic ? QStringLiteral("-") : QString();
    }

    QString atomSymbol(int atomId) const {
        const Atom &atom = m_molecule.atomData()[m_atomIndex.value(atomId)];
        const bool aromatic = isAromatic(atomId);
        const QString symbol = aromatic ? atom.element.toLower() : atom.element;
        // Neutral, unlabelled organic-subset atoms imply exactly the hydrogens the
        // editor computes, so they can be written bare.
        if (Elements::isOrganicSubset(atom.element) && atom.formalCharge == 0 && atom.isotope == 0) return symbol;
        QString bracket = QStringLiteral("[");
        if (atom.isotope) bracket += QString::number(atom.isotope);
        bracket += symbol;
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
