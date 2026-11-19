#include "SmilesReader.h"

#include "Elements.h"
#include "Layout2D.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QtMath>
#include <algorithm>
#include <functional>

namespace SmilesReader {
namespace {

enum class Sym { None, Single, Double, Triple, Aromatic };

struct PAtom {
    QString element;
    bool aromatic = false;
    bool bracket = false;
    int isotope = 0;
    int charge = 0;
    int hCount = 0;
};

struct PBond {
    int a = 0;
    int b = 0;
    Sym sym = Sym::None;
    int order = 1;
    bool aromatic = false;
};

struct ParseError {
    int position;
    QString reason;
};

const QSet<QString> &validSymbols() {
    static const QSet<QString> symbols = [] {
        QSet<QString> s;
        const QStringList list = QString(
            "H He Li Be B C N O F Ne Na Mg Al Si P S Cl Ar K Ca Sc Ti V Cr Mn Fe Co Ni Cu Zn Ga Ge As Se Br Kr "
            "Rb Sr Y Zr Nb Mo Tc Ru Rh Pd Ag Cd In Sn Sb Te I Xe Cs Ba La Ce Pr Nd Pm Sm Eu Gd Tb Dy Ho Er Tm Yb Lu "
            "Hf Ta W Re Os Ir Pt Au Hg Tl Pb Bi Po At Rn Fr Ra Ac Th Pa U Np Pu Am Cm Bk Cf Es Fm Md No Lr")
                                    .split(' ');
        for (const QString &e : list) s.insert(e);
        return s;
    }();
    return symbols;
}

class Parser {
public:
    explicit Parser(const QString &text) : m_text(text) {}

    // Throws ParseError.
    void run() {
        if (m_text.isEmpty()) throw ParseError{0, QStringLiteral("empty SMILES")};
        const int n = m_text.size();
        int previous = -1;
        QVector<int> branchStack;
        Sym pending = Sym::None;
        int pendingPos = 0;
        struct Open { int atom; Sym sym; int pos; };
        QHash<int, Open> open;
        bool justOpened = false;
        bool componentHasAtom = false;
        int lastDot = -1;

        for (int i = 0; i < n;) {
            const QChar ch = m_text[i];
            const bool wasOpened = justOpened;
            justOpened = false;
            if (ch == '-' || ch == '=' || ch == '#' || ch == ':' || ch == '/' || ch == '\\' || ch == '$') {
                if (ch == '$') throw ParseError{i, QStringLiteral("quadruple bonds ('$') are not supported")};
                if (pending != Sym::None) throw ParseError{i, QStringLiteral("two bond symbols in a row")};
                if (previous < 0) throw ParseError{i, QStringLiteral("bond symbol with no atom before it")};
                pending = ch == '=' ? Sym::Double : ch == '#' ? Sym::Triple : ch == ':' ? Sym::Aromatic : Sym::Single;
                pendingPos = i;
                ++i;
            } else if (ch == '(') {
                if (previous < 0) throw ParseError{i, QStringLiteral("branch with no atom before it")};
                if (pending != Sym::None) throw ParseError{i, QStringLiteral("bond symbol before '(' - put it inside the branch")};
                branchStack.append(previous);
                justOpened = true;
                ++i;
            } else if (ch == ')') {
                if (branchStack.isEmpty()) throw ParseError{i, QStringLiteral("')' without a matching '('")};
                if (wasOpened) throw ParseError{i, QStringLiteral("empty branch")};
                if (pending != Sym::None) throw ParseError{pendingPos, QStringLiteral("bond symbol with no atom after it")};
                previous = branchStack.takeLast();
                ++i;
            } else if (ch == '.') {
                if (!branchStack.isEmpty()) throw ParseError{i, QStringLiteral("'.' inside a branch")};
                if (pending != Sym::None) throw ParseError{pendingPos, QStringLiteral("bond symbol with no atom after it")};
                if (!componentHasAtom) throw ParseError{i, QStringLiteral("empty component before '.'")};
                previous = -1;
                componentHasAtom = false;
                lastDot = i;
                ++i;
            } else if (ch.isDigit() || ch == '%') {
                if (previous < 0) throw ParseError{i, QStringLiteral("ring closure with no atom before it")};
                const int start = i;
                int number;
                if (ch == '%') {
                    if (i + 2 >= n || !m_text[i + 1].isDigit() || !m_text[i + 2].isDigit())
                        throw ParseError{i, QStringLiteral("'%' needs two digits")};
                    number = m_text.mid(i + 1, 2).toInt();
                    i += 3;
                } else {
                    number = ch.digitValue();
                    ++i;
                }
                if (open.contains(number)) {
                    const Open o = open.take(number);
                    Sym sym = pending;
                    if (o.sym != Sym::None && pending != Sym::None && o.sym != pending)
                        throw ParseError{start, QStringLiteral("ring closure %1 has conflicting bond symbols").arg(number)};
                    if (sym == Sym::None) sym = o.sym;
                    if (o.atom == previous) throw ParseError{start, QStringLiteral("ring closure %1 joins an atom to itself").arg(number)};
                    addBond(o.atom, previous, sym, start);
                } else {
                    open.insert(number, {previous, pending, start});
                }
                pending = Sym::None;
            } else {
                const int start = i;
                const int atom = readAtom(i);
                componentHasAtom = true;
                if (previous >= 0) addBond(previous, atom, pending, pending != Sym::None ? pendingPos : start);
                pending = Sym::None;
                previous = atom;
            }
        }
        if (pending != Sym::None) throw ParseError{pendingPos, QStringLiteral("bond symbol with no atom after it")};
        if (!branchStack.isEmpty()) throw ParseError{n, QStringLiteral("'(' is never closed")};
        if (!open.isEmpty()) {
            int number = open.keys().first();
            for (const int key : open.keys()) if (open[key].pos < open[number].pos) number = key;
            throw ParseError{open[number].pos, QStringLiteral("ring bond %1 is never closed").arg(number)};
        }
        if (m_atoms.isEmpty()) throw ParseError{0, QStringLiteral("no atoms")};
        if (lastDot >= 0 && !componentHasAtom) throw ParseError{lastDot, QStringLiteral("empty component after '.'")};
    }

    QVector<PAtom> m_atoms;
    QVector<PBond> m_bonds;

private:
    void addBond(int a, int b, Sym sym, int position) {
        const QPair<int, int> key(qMin(a, b), qMax(a, b));
        if (m_pairs.contains(key)) throw ParseError{position, QStringLiteral("these two atoms are already bonded")};
        m_pairs.insert(key);
        PBond bond;
        bond.a = a;
        bond.b = b;
        bond.sym = sym;
        bond.order = sym == Sym::Double ? 2 : sym == Sym::Triple ? 3 : 1;
        m_bonds.append(bond);
    }

    int readAtom(int &i) {
        const int n = m_text.size();
        const QChar ch = m_text[i];
        PAtom atom;
        if (ch == '[') {
            atom.bracket = true;
            const int start = i;
            ++i;
            while (i < n && m_text[i].isDigit()) { atom.isotope = atom.isotope * 10 + m_text[i].digitValue(); ++i; }
            if (i >= n) throw ParseError{start, QStringLiteral("'[' is never closed")};
            if (m_text[i] == '*') throw ParseError{i, QStringLiteral("wildcard atoms ('*') are not supported")};
            const QChar first = m_text[i];
            if (!first.isLetter()) throw ParseError{i, QStringLiteral("expected an element symbol")};
            QString symbol(first);
            ++i;
            if (first.isUpper()) {
                if (i < n && m_text[i].isLower() && validSymbols().contains(symbol + m_text[i])) { symbol += m_text[i]; ++i; }
                if (!validSymbols().contains(symbol)) throw ParseError{int(i - symbol.size()), QStringLiteral("unknown element '%1'").arg(symbol)};
                atom.element = symbol;
            } else {
                if (i < n && m_text[i].isLower() && (symbol + m_text[i] == QStringLiteral("se") || symbol + m_text[i] == QStringLiteral("as"))) {
                    symbol += m_text[i];
                    ++i;
                }
                static const QSet<QString> aromatic = {"b", "c", "n", "o", "p", "s", "se", "as"};
                if (!aromatic.contains(symbol)) throw ParseError{int(i - symbol.size()), QStringLiteral("'%1' is not an aromatic element").arg(symbol)};
                atom.aromatic = true;
                atom.element = symbol.left(1).toUpper() + symbol.mid(1);
            }
            // Chirality (@, @@, @TH1 ...) is accepted and ignored.
            if (i < n && m_text[i] == '@') {
                while (i < n && m_text[i] == '@') ++i;
                if (i + 2 < n && m_text[i].isUpper() && m_text[i + 1].isUpper() && m_text[i + 2].isDigit()) {
                    i += 2;
                    while (i < n && m_text[i].isDigit()) ++i;
                }
            }
            if (i < n && m_text[i] == 'H') {
                ++i;
                atom.hCount = 1;
                if (i < n && m_text[i].isDigit()) {
                    atom.hCount = 0;
                    while (i < n && m_text[i].isDigit()) { atom.hCount = atom.hCount * 10 + m_text[i].digitValue(); ++i; }
                }
            }
            if (i < n && (m_text[i] == '+' || m_text[i] == '-')) {
                const QChar sign = m_text[i];
                int magnitude = 0;
                while (i < n && m_text[i] == sign) { ++magnitude; ++i; }
                if (magnitude == 1 && i < n && m_text[i].isDigit()) {
                    magnitude = 0;
                    while (i < n && m_text[i].isDigit()) { magnitude = magnitude * 10 + m_text[i].digitValue(); ++i; }
                }
                atom.charge = sign == '+' ? magnitude : -magnitude;
            }
            if (i < n && m_text[i] == ':') {
                ++i;
                if (i >= n || !m_text[i].isDigit()) throw ParseError{i, QStringLiteral("atom class needs digits after ':'")};
                while (i < n && m_text[i].isDigit()) ++i;
            }
            if (i >= n) throw ParseError{start, QStringLiteral("'[' is never closed")};
            if (m_text[i] != ']') throw ParseError{i, QStringLiteral("unexpected '%1' inside [...]").arg(m_text[i])};
            ++i;
        } else {
            QString symbol;
            if (ch == 'C' && i + 1 < n && m_text[i + 1] == 'l') { symbol = QStringLiteral("Cl"); i += 2; }
            else if (ch == 'B' && i + 1 < n && m_text[i + 1] == 'r') { symbol = QStringLiteral("Br"); i += 2; }
            else if (QStringLiteral("BCNOPSFI").contains(ch)) { symbol = QString(ch); ++i; }
            else if (QStringLiteral("bcnops").contains(ch)) { symbol = QString(ch.toUpper()); atom.aromatic = true; ++i; }
            else throw ParseError{i, QStringLiteral("unexpected '%1'").arg(ch)};
            atom.element = symbol;
        }
        m_atoms.append(atom);
        return m_atoms.size() - 1;
    }

    QString m_text;
    QSet<QPair<int, int>> m_pairs;
};

int lowestValence(const QString &element, int charge) {
    const Elements::Info *info = Elements::find(element);
    if (!info || info->normalValences.isEmpty()) return 0;
    const int v = info->normalValences.first();
    switch (info->rule) {
    case Elements::ChargeRule::LoseEither: return v - qAbs(charge);
    case Elements::ChargeRule::Group13: return v - charge;
    case Elements::ChargeRule::Group15To17: return v + charge;
    }
    return v;
}

// Finds a perfect matching on `needers` using only the given bond edges.
bool matchDoubles(const QVector<int> &needers, const QHash<int, QVector<int>> &edgesOf,
                  const QVector<PBond> &bonds, QSet<int> &chosen) {
    QSet<int> matched;
    int steps = 0;
    const std::function<bool()> solve = [&]() -> bool {
        if (++steps > 200000) return false;
        int best = -1, bestCount = 1 << 30;
        for (const int atom : needers) {
            if (matched.contains(atom)) continue;
            int count = 0;
            for (const int e : edgesOf.value(atom)) {
                const int other = bonds[e].a == atom ? bonds[e].b : bonds[e].a;
                if (!matched.contains(other)) ++count;
            }
            if (count < bestCount) { best = atom; bestCount = count; }
        }
        if (best < 0) return true;
        if (bestCount == 0) return false;
        for (const int e : edgesOf.value(best)) {
            const int other = bonds[e].a == best ? bonds[e].b : bonds[e].a;
            if (matched.contains(other)) continue;
            matched.insert(best); matched.insert(other); chosen.insert(e);
            if (solve()) return true;
            matched.remove(best); matched.remove(other); chosen.remove(e);
        }
        return false;
    };
    return solve();
}

} // namespace

Result parse(const QString &smiles) {
    Result result;
    Parser parser(smiles);
    try {
        parser.run();
    } catch (const ParseError &error) {
        result.errorPosition = error.position;
        result.error = QStringLiteral("SMILES error at position %1: %2").arg(error.position + 1).arg(error.reason);
        return result;
    }
    QVector<PAtom> &patoms = parser.m_atoms;
    QVector<PBond> &pbonds = parser.m_bonds;
    const int n = patoms.size();

    QVector<QVector<int>> adjacency(n);
    for (int e = 0; e < pbonds.size(); ++e) {
        adjacency[pbonds[e].a].append(pbonds[e].b);
        adjacency[pbonds[e].b].append(pbonds[e].a);
    }
    // Is bond e part of a cycle? (A path from a to b that avoids the bond itself.)
    const auto inRing = [&](int e) {
        const int a = pbonds[e].a, b = pbonds[e].b;
        QVector<char> seen(n, 0);
        QVector<int> queue{a};
        seen[a] = 1;
        for (int head = 0; head < queue.size(); ++head) {
            const int cur = queue[head];
            for (const int next : adjacency[cur]) {
                if ((cur == a && next == b) || (cur == b && next == a)) continue;
                if (next == b) return true;
                if (!seen[next]) { seen[next] = 1; queue.append(next); }
            }
        }
        return false;
    };

    // Aromatic bonds: explicit ':' or an unmarked bond joining two aromatic atoms in a ring.
    for (int e = 0; e < pbonds.size(); ++e) {
        PBond &bond = pbonds[e];
        if (bond.sym == Sym::Aromatic) bond.aromatic = true;
        else if (bond.sym == Sym::None && patoms[bond.a].aromatic && patoms[bond.b].aromatic && inRing(e)) bond.aromatic = true;
    }

    // Kekulize: an aromatic atom needs a ring double bond when it has exactly one
    // free valence left after its other bonds (and, in brackets, its H count).
    QVector<int> aromaticDegree(n, 0);
    QVector<double> sigma(n, 0);
    for (const PBond &bond : std::as_const(pbonds)) {
        for (const int atom : {bond.a, bond.b}) {
            if (bond.aromatic) { aromaticDegree[atom] += 1; sigma[atom] += 1; }
            else sigma[atom] += bond.order;
        }
    }
    QVector<char> needs(n, 0);
    for (int i = 0; i < n; ++i) {
        const PAtom &atom = patoms[i];
        if (aromaticDegree[i] == 0) continue;
        const int valence = lowestValence(atom.element, atom.charge);
        const double freeValence = valence - sigma[i] - (atom.bracket ? atom.hCount : 0);
        if (atom.bracket) needs[i] = freeValence == 1;
        else needs[i] = freeValence >= 1 && atom.element != QStringLiteral("O") && atom.element != QStringLiteral("S");
    }
    QHash<int, QVector<int>> edgesOf;
    QVector<int> aromaticBonds;
    QVector<QVector<int>> aromaticAdj(n);
    for (int e = 0; e < pbonds.size(); ++e) {
        if (!pbonds[e].aromatic) continue;
        aromaticBonds.append(e);
        aromaticAdj[pbonds[e].a].append(pbonds[e].b);
        aromaticAdj[pbonds[e].b].append(pbonds[e].a);
        if (needs[pbonds[e].a] && needs[pbonds[e].b]) {
            edgesOf[pbonds[e].a].append(e);
            edgesOf[pbonds[e].b].append(e);
        }
    }
    QVector<int> component(n, -1);
    int componentCount = 0;
    QSet<int> doubled;
    QSet<int> failedComponents;
    QVector<int> componentBondCount;
    QVector<int> componentAtomCount;
    for (int i = 0; i < n; ++i) {
        if (aromaticDegree[i] == 0 || component[i] >= 0) continue;
        QVector<int> members{i};
        component[i] = componentCount;
        for (int head = 0; head < members.size(); ++head)
            for (const int next : aromaticAdj[members[head]])
                if (component[next] < 0) { component[next] = componentCount; members.append(next); }
        QVector<int> needers;
        for (const int m : members) if (needs[m]) needers.append(m);
        QSet<int> chosen;
        if (!matchDoubles(needers, edgesOf, pbonds, chosen)) {
            failedComponents.insert(componentCount);
            result.warnings.append(QStringLiteral("Could not kekulize an aromatic ring system; its bonds are drawn as aromatic."));
        }
        doubled.unite(chosen);
        componentAtomCount.append(members.size());
        componentBondCount.append(0);
        ++componentCount;
    }
    for (const int e : aromaticBonds) componentBondCount[component[pbonds[e].a]] += 1;

    for (int i = 0; i < n; ++i) {
        Atom atom;
        atom.id = i + 1;
        atom.element = patoms[i].element;
        atom.formalCharge = patoms[i].charge;
        atom.isotope = patoms[i].isotope;
        result.atoms.append(atom);
    }
    for (int e = 0; e < pbonds.size(); ++e) {
        const PBond &bond = pbonds[e];
        Bond out;
        out.id = e + 1;
        out.atomA = bond.a + 1;
        out.atomB = bond.b + 1;
        out.order = bond.order;
        if (bond.aromatic) {
            const int comp = component[bond.a];
            if (failedComponents.contains(comp)) {
                out.order = 1;
                out.style = QStringLiteral("aromatic");
            } else {
                out.order = doubled.contains(e) ? 2 : 1;
                const bool loneRing = componentBondCount[comp] == componentAtomCount[comp];
                // A hexagon on its own: the benzene style draws a clean inner line.
                out.style = loneRing && componentAtomCount[comp] == 6 ? QStringLiteral("benzene") : QStringLiteral("plain");
            }
        }
        result.bonds.append(out);
    }

    // Hydrogens: the editor derives them from valence; add explicit H atoms where
    // a bracket atom asks for more.
    QVector<double> orderSum(n, 0);
    for (const Bond &bond : std::as_const(result.bonds)) {
        const double v = bond.style == QStringLiteral("aromatic") ? 1.5 : bond.order;
        orderSum[bond.atomA - 1] += v;
        orderSum[bond.atomB - 1] += v;
    }
    int nextAtom = n + 1, nextBond = result.bonds.size() + 1;
    for (int i = 0; i < n; ++i) {
        if (!patoms[i].bracket) continue;
        const int implicit = Elements::implicitHydrogens(patoms[i].element, patoms[i].charge, qCeil(orderSum[i] - 1e-9));
        const int wanted = patoms[i].hCount;
        if (wanted > implicit) {
            for (int h = 0; h < wanted - implicit; ++h) {
                Atom hydrogen;
                hydrogen.id = nextAtom++;
                hydrogen.element = QStringLiteral("H");
                result.atoms.append(hydrogen);
                Bond bond;
                bond.id = nextBond++;
                bond.atomA = i + 1;
                bond.atomB = hydrogen.id;
                result.bonds.append(bond);
            }
            result.warnings.append(QStringLiteral("Added %1 explicit hydrogen(s) on %2 to match its H count.").arg(wanted - implicit).arg(patoms[i].element));
        } else if (wanted < implicit) {
            result.warnings.append(QStringLiteral("%1 has fewer hydrogens in the SMILES than the editor's valence rules give; the editor's count is used.").arg(patoms[i].element));
        }
    }
    result.ok = true;
    return result;
}

bool build(const QString &smiles, double centerX, double centerY, QVector<Atom> &atoms,
           QVector<Bond> &bonds, QString *error, QStringList *warnings) {
    const QString token = smiles.trimmed().section(QRegularExpression(QStringLiteral("\\s+")), 0, 0);
    Result result = parse(token);
    if (!result.ok) {
        if (error) *error = token.isEmpty() ? QStringLiteral("SMILES error: nothing to import.") : result.error;
        return false;
    }
    QVector<Layout2D::Edge> edges;
    for (const Bond &bond : std::as_const(result.bonds)) edges.append({bond.atomA - 1, bond.atomB - 1, bond.order});
    const QVector<QPointF> points = Layout2D::place(result.atoms.size(), edges);
    double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
    for (const QPointF &p : points) {
        minX = qMin(minX, p.x()); maxX = qMax(maxX, p.x());
        minY = qMin(minY, p.y()); maxY = qMax(maxY, p.y());
    }
    const QPointF offset(centerX - (minX + maxX) / 2, centerY - (minY + maxY) / 2);
    for (int i = 0; i < result.atoms.size(); ++i) {
        result.atoms[i].x = points[i].x() + offset.x();
        result.atoms[i].y = points[i].y() + offset.y();
    }
    atoms = result.atoms;
    bonds = result.bonds;
    if (warnings) *warnings = result.warnings;
    return true;
}

} // namespace SmilesReader
