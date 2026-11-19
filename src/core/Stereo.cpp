#include "Stereo.h"

#include "Elements.h"
#include "Molecule.h"

#include <QHash>
#include <QSet>
#include <QtMath>

#include <algorithm>

namespace Stereo {
namespace {

constexpr int MaxDepth = 8;

// One node of the hierarchical digraph. Duplicate atoms (double bonds, ring
// closures, aromatic bonds), hydrogens and phantoms have no children.
struct Node {
    int atom = -1;
    int z = 0;
    int mass = 0;
    QVector<Node> children;
};

int massOf(const Atom &atom) {
    if (atom.isotope > 0) return atom.isotope;
    if (const Elements::Info *info = Elements::find(atom.element)) return qRound(info->monoisotopicMass);
    return 2 * rankingAtomicNumber(atom.element);
}

Node leafFor(const Atom &atom) {
    Node node;
    node.atom = atom.id;
    node.z = rankingAtomicNumber(atom.element);
    node.mass = massOf(atom);
    return node;
}

Node hydrogenLeaf() {
    Node node;
    node.z = 1;
    node.mass = 1;
    return node;
}

// Sphere-by-sphere comparison: all atomic numbers first (rule 1a), then, only
// if those are identical everywhere, mass numbers (rule 1b). Returns >0 when a
// outranks b, <0 when b outranks a, 0 for a tie.
int compareNodes(const Node &a, const Node &b) {
    if (a.z != b.z) return a.z > b.z ? 1 : -1;
    QVector<int> massA{a.mass}, massB{b.mass};
    QVector<const Node *> levelA{&a}, levelB{&b};
    static const Node phantom;
    for (int depth = 0; depth < MaxDepth; ++depth) {
        QVector<const Node *> nextA, nextB;
        bool any = false;
        for (int i = 0; i < levelA.size(); ++i) {
            const QVector<Node> &ca = levelA[i]->children;
            const QVector<Node> &cb = levelB[i]->children;
            const int size = qMax(3, qMax(ca.size(), cb.size()));
            for (int k = 0; k < size; ++k) {
                const Node *na = k < ca.size() ? &ca[k] : &phantom;
                const Node *nb = k < cb.size() ? &cb[k] : &phantom;
                if (na->z != nb->z) return na->z > nb->z ? 1 : -1;
                if (na->z) any = true;
                nextA.append(na);
                nextB.append(nb);
                massA.append(na->mass);
                massB.append(nb->mass);
            }
        }
        if (!any) break;
        levelA = nextA;
        levelB = nextB;
    }
    for (int i = 0; i < massA.size(); ++i)
        if (massA[i] != massB[i]) return massA[i] > massB[i] ? 1 : -1;
    return 0;
}

class Context {
public:
    explicit Context(const Molecule &molecule) : m_molecule(molecule) {
        for (const Atom &atom : molecule.atomData()) m_atoms.insert(atom.id, atom);
        for (const Bond &bond : molecule.bondData()) {
            m_incident[bond.atomA].append(bond);
            m_incident[bond.atomB].append(bond);
        }
    }

    const Atom *atom(int id) const {
        const auto it = m_atoms.constFind(id);
        return it == m_atoms.cend() ? nullptr : &it.value();
    }
    QVector<Bond> bondsOf(int id) const { return m_incident.value(id); }
    static int other(const Bond &bond, int id) { return bond.atomA == id ? bond.atomB : bond.atomA; }
    int hydrogens(const Atom &atom) const { return m_molecule.implicitHydrogens(atom); }

    // Subtree below atomId, reached from parentId. ancestors holds every atom on
    // the path from the root, including parentId.
    Node build(int atomId, int parentId, QSet<int> ancestors, int depth) const {
        const Atom &self = m_atoms[atomId];
        Node node = leafFor(self);
        if (depth <= 0) return node;
        ancestors.insert(atomId);
        QVector<int> aromaticPartners;
        for (const Bond &bond : bondsOf(atomId)) {
            const int partnerId = other(bond, atomId);
            const Atom *partner = atom(partnerId);
            if (!partner) continue;
            const bool aromatic = bond.style == QStringLiteral("aromatic");
            if (aromatic) aromaticPartners.append(partnerId);
            const int duplicates = aromatic ? 0 : qMax(0, bond.order - 1);
            if (partnerId == parentId) {
                for (int d = 0; d < duplicates; ++d) node.children.append(leafFor(*partner));
                continue;
            }
            if (ancestors.contains(partnerId)) node.children.append(leafFor(*partner));
            else node.children.append(build(partnerId, atomId, ancestors, depth - 1));
            for (int d = 0; d < duplicates; ++d) node.children.append(leafFor(*partner));
        }
        if (aromaticPartners.size() >= 2) {
            // Stand-in for the Kekule duplicate: mean atomic number of the ring neighbours.
            int sum = 0;
            for (int id : aromaticPartners) sum += rankingAtomicNumber(m_atoms[id].element);
            Node duplicate;
            duplicate.z = qRound(double(sum) / aromaticPartners.size());
            duplicate.mass = 2 * duplicate.z;
            node.children.append(duplicate);
        }
        for (int h = hydrogens(self); h > 0; --h) node.children.append(hydrogenLeaf());
        std::stable_sort(node.children.begin(), node.children.end(),
                         [](const Node &x, const Node &y) { return compareNodes(x, y) > 0; });
        return node;
    }

    // Smallest ring through this bond, or 0 when it is not in a ring.
    int ringSize(const Bond &bond) const {
        QHash<int, int> distance{{bond.atomA, 0}};
        QVector<int> queue{bond.atomA};
        for (int head = 0; head < queue.size(); ++head) {
            const int current = queue[head];
            for (const Bond &next : bondsOf(current)) {
                if (next.id == bond.id) continue;
                const int id = other(next, current);
                if (distance.contains(id)) continue;
                distance.insert(id, distance[current] + 1);
                if (id == bond.atomB) return distance[id] + 1;
                queue.append(id);
            }
        }
        return 0;
    }

private:
    const Molecule &m_molecule;
    QHash<int, Atom> m_atoms;
    QHash<int, QVector<Bond>> m_incident;
};

struct Substituent {
    Node node;
    double dx = 0, dy = 0, dz = 0; // unit-ish vector, y up
    bool explicitAtom = false;
};

bool hasMultipleBond(const Context &ctx, int atomId) {
    for (const Bond &bond : ctx.bondsOf(atomId))
        if (bond.order > 1 || bond.style == QStringLiteral("aromatic")) return true;
    return false;
}

bool tetrahedralCandidate(const Atom &atom, int substituents, int explicitCount) {
    const QString &e = atom.element;
    if (e == QStringLiteral("C") || e == QStringLiteral("Si") || e == QStringLiteral("Ge"))
        return atom.formalCharge == 0 && substituents == 4;
    if (e == QStringLiteral("N") || e == QStringLiteral("P"))
        return (atom.formalCharge == 1 && substituents == 4) || (e == QStringLiteral("P") && atom.formalCharge == 0 && explicitCount == 3 && substituents == 3);
    if (e == QStringLiteral("S")) return atom.formalCharge == 1 && substituents == 3;
    return false;
}

void describeAtom(const Context &ctx, const Atom &center, QVector<Descriptor> &out) {
    if (hasMultipleBond(ctx, center.id)) return;
    QVector<Substituent> subs;
    bool anchored = false;
    for (const Bond &bond : ctx.bondsOf(center.id)) {
        const Atom *n = ctx.atom(Context::other(bond, center.id));
        if (!n) continue;
        Substituent s;
        s.explicitAtom = true;
        s.node = ctx.build(n->id, center.id, QSet<int>{center.id}, MaxDepth);
        const double dx = n->x - center.x, dy = -(n->y - center.y), len = qSqrt(dx * dx + dy * dy);
        if (len < 1e-6) return;
        s.dx = dx / len; s.dy = dy / len;
        if (bond.atomA == center.id && bond.style == QStringLiteral("wedge")) { s.dz = 1; anchored = true; }
        else if (bond.atomA == center.id && bond.style == QStringLiteral("dash")) { s.dz = -1; anchored = true; }
        subs.append(s);
    }
    if (!anchored) return;
    const int explicitCount = subs.size();
    const int hydrogens = ctx.hydrogens(center);
    if (!tetrahedralCandidate(center, explicitCount + hydrogens, explicitCount)) return;
    if (explicitCount < 3 || explicitCount > 4) return;
    if (explicitCount == 3) {
        // Fourth position: implicit hydrogen if there is one, otherwise a lone pair
        // (phantom, lowest rank). It points away from the three drawn bonds.
        Substituent fourth;
        double vx = 0, vy = 0, vz = 0;
        for (const Substituent &s : subs) { vx -= s.dx; vy -= s.dy; vz -= s.dz; }
        const double len = qSqrt(vx * vx + vy * vy + vz * vz);
        if (len < 1e-6) return;
        fourth.dx = vx / len; fourth.dy = vy / len; fourth.dz = vz / len;
        if (hydrogens > 0) fourth.node = hydrogenLeaf();
        subs.append(fourth);
    }
    QVector<int> order{0, 1, 2, 3};
    std::sort(order.begin(), order.end(), [&subs](int a, int b) { return compareNodes(subs[a].node, subs[b].node) > 0; });
    for (int i = 0; i < 3; ++i)
        if (compareNodes(subs[order[i]].node, subs[order[i + 1]].node) == 0) return;
    const Substituent &a = subs[order[0]], &b = subs[order[1]], &c = subs[order[2]], &d = subs[order[3]];
    const double ax = a.dx - d.dx, ay = a.dy - d.dy, az = a.dz - d.dz;
    const double bx = b.dx - d.dx, by = b.dy - d.dy, bz = b.dz - d.dz;
    const double cx = c.dx - d.dx, cy = c.dy - d.dy, cz = c.dz - d.dz;
    const double volume = ax * (by * cz - bz * cy) + ay * (bz * cx - bx * cz) + az * (bx * cy - by * cx);
    if (qAbs(volume) < 0.05) return;

    Descriptor result;
    result.atomId = center.id;
    result.label = volume < 0 ? QStringLiteral("R") : QStringLiteral("S");
    // Put the label in the widest angular gap between the drawn bonds.
    QVector<double> angles;
    for (int i = 0; i < explicitCount; ++i) angles.append(qAtan2(subs[i].dy, subs[i].dx));
    std::sort(angles.begin(), angles.end());
    double bestGap = -1, bestAngle = M_PI / 2;
    for (int i = 0; i < angles.size(); ++i) {
        const double next = i + 1 < angles.size() ? angles[i + 1] : angles[0] + 2 * M_PI;
        if (next - angles[i] > bestGap) { bestGap = next - angles[i]; bestAngle = angles[i] + bestGap / 2; }
    }
    result.x = center.x + qCos(bestAngle) * 28;
    result.y = center.y - qSin(bestAngle) * 28;
    out.append(result);
}

void describeBond(const Context &ctx, const Bond &bond, QVector<Descriptor> &out) {
    if (bond.order != 2 || bond.style == QStringLiteral("wavy") || bond.style == QStringLiteral("aromatic")) return;
    const int ring = ctx.ringSize(bond);
    if (ring > 0 && ring < 8) return;
    const Atom *a = ctx.atom(bond.atomA), *b = ctx.atom(bond.atomB);
    if (!a || !b) return;
    const double bx = b->x - a->x, by = -(b->y - a->y), blen = qSqrt(bx * bx + by * by);
    if (blen < 1e-6) return;

    struct End { int rankedFirst = -1; double side = 0; bool ok = false; double cx = 0, cy = 0; };
    const auto analyse = [&](const Atom &end, const Atom &partner, End &result) {
        const QString &e = end.element;
        if (e != QStringLiteral("C") && e != QStringLiteral("N") && e != QStringLiteral("Si")) return;
        QVector<Node> nodes;
        QVector<double> sides; // 0 for implicit positions
        double centroidX = 0, centroidY = 0;
        for (const Bond &other : ctx.bondsOf(end.id)) {
            if (other.id == bond.id) continue;
            if (other.order > 1 || other.style == QStringLiteral("aromatic")) return; // cumulene or conjugated-aromatic end
            const Atom *n = ctx.atom(Context::other(other, end.id));
            if (!n) return;
            nodes.append(ctx.build(n->id, end.id, QSet<int>{end.id, partner.id}, MaxDepth));
            const double dx = n->x - end.x, dy = -(n->y - end.y);
            sides.append((bx * dy - by * dx) / blen);
            centroidX += n->x - end.x; centroidY += n->y - end.y;
        }
        for (int h = ctx.hydrogens(end); h > 0; --h) { nodes.append(hydrogenLeaf()); sides.append(0); }
        if (nodes.size() == 1 && e == QStringLiteral("N") && end.formalCharge == 0) { nodes.append(Node()); sides.append(0); }
        if (nodes.size() != 2) return;
        const int cmp = compareNodes(nodes[0], nodes[1]);
        if (cmp == 0) return;
        const int high = cmp > 0 ? 0 : 1;
        // Side of the double-bond axis the higher-ranked substituent lies on.
        double side;
        if (sides[high] != 0) {
            side = sides[high];
            if (sides[1 - high] != 0 && (side > 0) == (sides[1 - high] > 0)) return; // degenerate drawing
        } else {
            side = -sides[1 - high];
        }
        if (qAbs(side) < 0.5) return;
        result.side = side > 0 ? 1 : -1;
        result.ok = true;
        result.cx = centroidX; result.cy = centroidY;
    };
    End ea, eb;
    analyse(*a, *b, ea);
    analyse(*b, *a, eb);
    if (!ea.ok || !eb.ok) return;

    Descriptor result;
    result.bondId = bond.id;
    result.label = ea.side == eb.side ? QStringLiteral("Z") : QStringLiteral("E");
    // Offset from the bond midpoint, away from the substituents (screen coordinates).
    const double mx = (a->x + b->x) / 2, my = (a->y + b->y) / 2;
    const double nx = -(b->y - a->y) / blen, ny = (b->x - a->x) / blen;
    const double toward = (ea.cx + eb.cx) * nx + (ea.cy + eb.cy) * ny;
    const double sign = toward > 0 ? -1.0 : 1.0;
    result.x = mx + nx * sign * 18;
    result.y = my + ny * sign * 18;
    out.append(result);
}

} // namespace

int rankingAtomicNumber(const QString &element) {
    if (int z = Elements::atomicNumber(element)) return z;
    // Abbreviation labels (e.g. "OMe", "NHBoc") rank by their first atom.
    if (element.size() >= 2 && element[1].isLower()) {
        if (int z = Elements::atomicNumber(element.left(2))) return z;
    }
    return element.isEmpty() ? 0 : Elements::atomicNumber(element.left(1));
}

int chiralParity(const QVector<Arm> &arms) {
    if (arms.size() != 4) return 0;
    double v[4][3];
    double sum[3] = {0, 0, 0};
    int implicitIndex = -1;
    for (int i = 0; i < 4; ++i) {
        if (arms[i].implicit) {
            if (implicitIndex >= 0) return 0;
            implicitIndex = i;
            continue;
        }
        double dx = arms[i].dx, dy = -arms[i].dy;
        const double len = qSqrt(dx * dx + dy * dy);
        if (len < 1e-6) return 0;
        v[i][0] = dx / len; v[i][1] = dy / len; v[i][2] = arms[i].z;
        for (int k = 0; k < 3; ++k) sum[k] += v[i][k];
    }
    if (implicitIndex >= 0) {
        const double len = qSqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
        if (len < 1e-6) return 0;
        for (int k = 0; k < 3; ++k) v[implicitIndex][k] = -sum[k] / len;
    }
    const double a[3] = {v[1][0] - v[0][0], v[1][1] - v[0][1], v[1][2] - v[0][2]};
    const double b[3] = {v[2][0] - v[0][0], v[2][1] - v[0][1], v[2][2] - v[0][2]};
    const double c[3] = {v[3][0] - v[0][0], v[3][1] - v[0][1], v[3][2] - v[0][2]};
    const double volume = a[0] * (b[1] * c[2] - b[2] * c[1]) + a[1] * (b[2] * c[0] - b[0] * c[2]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
    if (qAbs(volume) < 0.05) return 0;
    return volume < 0 ? 1 : -1;
}

int chiralParity(const Molecule &molecule, int centerId, const QVector<int> &order) {
    const Context ctx(molecule);
    const Atom *center = ctx.atom(centerId);
    if (!center || hasMultipleBond(ctx, centerId)) return 0;
    const QVector<Bond> bonds = ctx.bondsOf(centerId);
    bool anchored = false;
    for (const Bond &bond : bonds)
        if (bond.atomA == centerId && (bond.style == QStringLiteral("wedge") || bond.style == QStringLiteral("dash"))) anchored = true;
    if (!anchored) return 0;
    const int hydrogens = ctx.hydrogens(*center);
    if (!tetrahedralCandidate(*center, bonds.size() + hydrogens, bonds.size())) return 0;
    if (order.size() != 4 || bonds.size() + (hydrogens > 0 ? 1 : 0) != 4 || hydrogens > 1) return 0;
    QVector<Arm> arms;
    for (const int id : order) {
        Arm arm;
        if (id == 0) {
            arm.implicit = true;
        } else {
            const Bond *found = nullptr;
            for (const Bond &bond : bonds) if (Context::other(bond, centerId) == id) found = &bond;
            const Atom *n = ctx.atom(id);
            if (!found || !n) return 0;
            arm.dx = n->x - center->x; arm.dy = n->y - center->y;
            if (found->atomA == centerId && found->style == QStringLiteral("wedge")) arm.z = 1;
            else if (found->atomA == centerId && found->style == QStringLiteral("dash")) arm.z = -1;
        }
        arms.append(arm);
    }
    if (hydrogens == 0) for (const Arm &arm : arms) if (arm.implicit) return 0;
    return chiralParity(arms);
}

QVector<Descriptor> compute(const Molecule &molecule) {
    QVector<Descriptor> result;
    const Context ctx(molecule);
    for (const Atom &atom : molecule.atomData()) describeAtom(ctx, atom, result);
    for (const Bond &bond : molecule.bondData()) describeBond(ctx, bond, result);
    return result;
}

} // namespace Stereo
