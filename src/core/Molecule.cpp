#include "Molecule.h"

#include "Elements.h"
#include "GeometryEngine.h"
#include "MolfileReader.h"
#include "MolfileWriter.h"
#include "SmilesReader.h"
#include "SmilesWriter.h"
#include "SvgWriter.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLineF>
#include <QMap>
#include <QSet>
#include <QtMath>

namespace {
QVariantMap atomMap(const Atom &atom) {
    return {{QStringLiteral("id"), atom.id}, {QStringLiteral("element"), atom.element},
            {QStringLiteral("x"), atom.x}, {QStringLiteral("y"), atom.y},
            {QStringLiteral("charge"), atom.formalCharge}, {QStringLiteral("isotope"), atom.isotope},
            {QStringLiteral("radical"), atom.radical}};
}

QVariantMap bondMap(const Bond &bond) {
    return {{QStringLiteral("id"), bond.id}, {QStringLiteral("atomA"), bond.atomA},
            {QStringLiteral("atomB"), bond.atomB}, {QStringLiteral("order"), bond.order},
            {QStringLiteral("stereo"), bond.stereo}, {QStringLiteral("style"), bond.style}};
}

bool isBondStyle(const QString &style) {
    return style == QStringLiteral("plain") || style == QStringLiteral("wedge") ||
           style == QStringLiteral("dash") || style == QStringLiteral("wavy") ||
           style == QStringLiteral("aromatic") || style == QStringLiteral("benzene");
}

int ringSides(const QString &templateName) {
    static const QHash<QString, int> sides = {{QStringLiteral("cyclopropane"), 3}, {QStringLiteral("cyclobutane"), 4},
                                               {QStringLiteral("cyclopentane"), 5}, {QStringLiteral("cyclohexane"), 6},
                                               {QStringLiteral("benzene"), 6}};
    return sides.value(templateName, 0);
}

int ringBondOrder(const QString &templateName, int bondIndex) {
    return templateName == QStringLiteral("benzene") && bondIndex % 2 == 0 ? 2 : 1;
}
}

Molecule::Molecule(QObject *parent) : QObject(parent) {}

const QVector<Atom> &Molecule::atomData() const { return m_atoms; }
const QVector<Bond> &Molecule::bondData() const { return m_bonds; }

void Molecule::replaceContent(const QVector<Atom> &atoms, const QVector<Bond> &bonds) {
    m_atoms = atoms;
    m_bonds = bonds;
    m_nextAtomId = 1;
    m_nextBondId = 1;
    for (const Atom &atom : m_atoms) m_nextAtomId = qMax(m_nextAtomId, atom.id + 1);
    for (const Bond &bond : m_bonds) m_nextBondId = qMax(m_nextBondId, bond.id + 1);
    m_undo.clear();
    m_redo.clear();
    emit historyReset();
    emit changed();
}

QVariantList Molecule::atoms() const {
    QVariantList result;
    for (const Atom &atom : m_atoms) result.append(atomMap(atom));
    return result;
}

QVariantList Molecule::bonds() const {
    QVariantList result;
    for (const Bond &bond : m_bonds) result.append(bondMap(bond));
    return result;
}

int Molecule::addAtom(double x, double y, const QString &element) {
    const Snapshot before = snapshot();
    const int id = m_nextAtomId++;
    m_atoms.append({id, element.trimmed().isEmpty() ? QStringLiteral("C") : element.trimmed(), x, y});
    commit(before);
    return id;
}

int Molecule::addBond(int atomA, int atomB, int order) {
    order = qBound(1, order, 3);
    if (!canAddBond(atomA, atomB, order)) {
        emit message(QStringLiteral("That bond would exceed a basic valence limit or already exists."));
        return -1;
    }
    const Snapshot before = snapshot();
    const int id = m_nextBondId++;
    m_bonds.append({id, atomA, atomB, order, 0});
    commit(before);
    return id;
}

int Molecule::extendFromAtom(int atomId, double x, double y, const QString &element) {
    const Atom *anchor = findAtom(atomId);
    if (!anchor) return -1;
    if (valenceUsed(atomId) + 1 > maxValence(*anchor)) {
        emit message(QStringLiteral("That atom has no free valence for another bond."));
        return -1;
    }
    const Snapshot before = snapshot();
    const int id = m_nextAtomId++;
    m_atoms.append({id, element.trimmed().isEmpty() ? QStringLiteral("C") : element.trimmed(), x, y});
    m_bonds.append({m_nextBondId++, atomId, id, 1, 0});
    commit(before);
    return id;
}

bool Molecule::cycleBondOrder(int bondId) {
    Bond *bond = findBond(bondId);
    if (!bond) return false;
    const int nextOrder = bond->order == 3 ? 1 : bond->order + 1;
    const int adjustment = nextOrder - bond->order;
    if (valenceUsed(bond->atomA) + adjustment > maxValence(*findAtom(bond->atomA)) ||
        valenceUsed(bond->atomB) + adjustment > maxValence(*findAtom(bond->atomB))) {
        emit message(QStringLiteral("That bond order would exceed a basic valence limit."));
        return false;
    }
    const Snapshot before = snapshot();
    bond->order = nextOrder;
    commit(before);
    return true;
}

bool Molecule::removeAtom(int atomId) {
    const auto iterator = std::find_if(m_atoms.begin(), m_atoms.end(), [atomId](const Atom &atom) { return atom.id == atomId; });
    if (iterator == m_atoms.end()) return false;
    const Snapshot before = snapshot();
    m_atoms.erase(iterator);
    m_bonds.erase(std::remove_if(m_bonds.begin(), m_bonds.end(), [atomId](const Bond &bond) {
        return bond.atomA == atomId || bond.atomB == atomId;
    }), m_bonds.end());
    commit(before);
    return true;
}

bool Molecule::removeBond(int bondId) {
    const auto iterator = std::find_if(m_bonds.begin(), m_bonds.end(), [bondId](const Bond &bond) { return bond.id == bondId; });
    if (iterator == m_bonds.end()) return false;
    const Snapshot before = snapshot();
    m_bonds.erase(iterator);
    commit(before);
    return true;
}

bool Molecule::setAtomElement(int atomId, const QString &element) {
    Atom *atom = findAtom(atomId);
    const QString normalized = element.trimmed();
    if (!atom || normalized.isEmpty() || valenceUsed(atomId) > maxValence(normalized, atom->formalCharge)) {
        emit message(QStringLiteral("That element cannot support the atom's existing bonds."));
        return false;
    }
    const Snapshot before = snapshot();
    atom->element = normalized;
    commit(before);
    return true;
}

bool Molecule::setAtomCharge(int atomId, int charge) {
    Atom *atom = findAtom(atomId);
    if (!atom || charge < -3 || charge > 3) return false;
    if (valenceUsed(atomId) > maxValence(atom->element, charge)) {
        emit message(QStringLiteral("That charge cannot support the atom's existing bonds."));
        return false;
    }
    const Snapshot before = snapshot();
    atom->formalCharge = charge;
    commit(before);
    return true;
}

bool Molecule::setAtomIsotope(int atomId, int isotope) {
    Atom *atom = findAtom(atomId);
    if (!atom || isotope < 0 || isotope > 300) return false;
    const Snapshot before = snapshot();
    atom->isotope = isotope;
    commit(before);
    return true;
}

bool Molecule::moveAtom(int atomId, double x, double y) {
    Atom *atom = findAtom(atomId);
    if (!atom) return false;
    const Snapshot before = snapshot();
    atom->x = x;
    atom->y = y;
    commit(before);
    return true;
}

bool Molecule::translateAtoms(const QVariantList &atomIds, double dx, double dy) {
    // A tap with the Move tool translates by zero; don't record an empty undo step.
    if (qFuzzyIsNull(dx) && qFuzzyIsNull(dy)) return false;
    bool found = false;
    const Snapshot before = snapshot();
    for (const QVariant &value : atomIds) {
        if (Atom *atom = findAtom(value.toInt())) {
            atom->x += dx; atom->y += dy; found = true;
        }
    }
    if (found) commit(before);
    return found;
}

bool Molecule::rotateAtoms(const QVariantList &atomIds, double degrees) {
    QVector<Atom *> selected;
    QPointF center;
    for (const QVariant &value : atomIds) {
        if (Atom *atom = findAtom(value.toInt())) { selected.append(atom); center += QPointF(atom->x, atom->y); }
    }
    if (selected.isEmpty()) return false;
    center /= selected.size();
    const double angle = qDegreesToRadians(degrees);
    const double cosine = qCos(angle), sine = qSin(angle);
    const Snapshot before = snapshot();
    for (Atom *atom : selected) {
        const QPointF delta(atom->x - center.x(), atom->y - center.y());
        atom->x = center.x() + delta.x() * cosine - delta.y() * sine;
        atom->y = center.y() + delta.x() * sine + delta.y() * cosine;
    }
    commit(before);
    return true;
}

bool Molecule::mirrorAtoms(const QVariantList &atomIds, bool horizontal) {
    QVector<Atom *> selected;
    QPointF center;
    for (const QVariant &value : atomIds) {
        if (Atom *atom = findAtom(value.toInt())) { selected.append(atom); center += QPointF(atom->x, atom->y); }
    }
    if (selected.isEmpty()) return false;
    center /= selected.size();
    const Snapshot before = snapshot();
    for (Atom *atom : selected) {
        if (horizontal) atom->x = 2.0 * center.x() - atom->x;
        else atom->y = 2.0 * center.y() - atom->y;
    }
    commit(before);
    return true;
}

bool Molecule::alignAtoms(const QVariantList &atomIds, const QString &alignment) {
    QVector<Atom *> selected;
    double coordinateTotal = 0.0;
    for (const QVariant &value : atomIds) {
        if (Atom *atom = findAtom(value.toInt())) {
            selected.append(atom);
            coordinateTotal += alignment == QStringLiteral("horizontal") ? atom->y : atom->x;
        }
    }
    if (selected.size() < 2 || (alignment != QStringLiteral("horizontal") && alignment != QStringLiteral("vertical"))) return false;
    const double coordinate = coordinateTotal / selected.size();
    const Snapshot before = snapshot();
    for (Atom *atom : selected) {
        if (alignment == QStringLiteral("horizontal")) atom->y = coordinate;
        else atom->x = coordinate;
    }
    commit(before);
    return true;
}

bool Molecule::removeAtoms(const QVariantList &atomIds) {
    QSet<int> ids;
    for (const QVariant &value : atomIds) ids.insert(value.toInt());
    if (ids.isEmpty()) return false;
    const int previousSize = m_atoms.size();
    const Snapshot before = snapshot();
    m_atoms.erase(std::remove_if(m_atoms.begin(), m_atoms.end(), [&ids](const Atom &atom) { return ids.contains(atom.id); }), m_atoms.end());
    m_bonds.erase(std::remove_if(m_bonds.begin(), m_bonds.end(), [&ids](const Bond &bond) {
        return ids.contains(bond.atomA) || ids.contains(bond.atomB);
    }), m_bonds.end());
    if (m_atoms.size() == previousSize) return false;
    commit(before);
    return true;
}

bool Molecule::copyAtoms(const QVariantList &atomIds) {
    QSet<int> ids;
    for (const QVariant &value : atomIds) ids.insert(value.toInt());
    QVector<Atom> copiedAtoms;
    QPointF center;
    for (const Atom &atom : m_atoms) {
        if (!ids.contains(atom.id)) continue;
        copiedAtoms.append(atom);
        center += QPointF(atom.x, atom.y);
    }
    if (copiedAtoms.isEmpty()) return false;
    center /= copiedAtoms.size();
    for (Atom &atom : copiedAtoms) { atom.x -= center.x(); atom.y -= center.y(); }
    QVector<Bond> copiedBonds;
    for (const Bond &bond : m_bonds) {
        if (ids.contains(bond.atomA) && ids.contains(bond.atomB)) copiedBonds.append(bond);
    }
    m_clipboardAtoms = copiedAtoms;
    m_clipboardBonds = copiedBonds;
    emit message(QStringLiteral("Copied %1 atom(s).").arg(copiedAtoms.size()));
    return true;
}

QVariantList Molecule::pasteAtoms(double x, double y) {
    QVariantList created;
    if (m_clipboardAtoms.isEmpty()) {
        emit message(QStringLiteral("Select and copy a fragment before pasting."));
        return created;
    }
    const Snapshot before = snapshot();
    QHash<int, int> ids;
    for (const Atom &source : m_clipboardAtoms) {
        const int id = m_nextAtomId++;
        Atom atom = source;
        atom.id = id; atom.x += x; atom.y += y;
        m_atoms.append(atom);
        ids.insert(source.id, id);
        created.append(id);
    }
    for (const Bond &source : m_clipboardBonds) {
        Bond bond = source;
        bond.id = m_nextBondId++;
        bond.atomA = ids.value(source.atomA); bond.atomB = ids.value(source.atomB);
        m_bonds.append(bond);
    }
    commit(before);
    return created;
}

bool Molecule::setBondStyle(int bondId, const QString &style) {
    Bond *bond = findBond(bondId);
    if (!bond || !isBondStyle(style)) return false;
    const Snapshot before = snapshot();
    bond->style = style;
    // Keep the stored stereo flag in V2000 terms, matching MolfileReader/Writer.
    bond->stereo = style == QStringLiteral("wedge") ? 1 : style == QStringLiteral("dash") ? 6 :
                   style == QStringLiteral("wavy") ? (bond->order == 2 ? 3 : 4) : 0;
    commit(before);
    return true;
}

bool Molecule::cycleBondStyle(int bondId) {
    const Bond *bond = findBond(bondId);
    if (!bond) return false;
    const QStringList styles = {QStringLiteral("plain"), QStringLiteral("wedge"), QStringLiteral("dash"),
                                QStringLiteral("wavy"), QStringLiteral("aromatic")};
    const int index = styles.indexOf(bond->style);
    return setBondStyle(bondId, styles[(index + 1) % styles.size()]);
}

int Molecule::atomAt(double x, double y, double radius) const {
    const QPointF point(x, y);
    int closestId = -1;
    double closestDistance = radius;
    for (const Atom &atom : m_atoms) {
        const double distance = QLineF(point, QPointF(atom.x, atom.y)).length();
        if (distance <= closestDistance) { closestDistance = distance; closestId = atom.id; }
    }
    return closestId;
}

int Molecule::bondAt(double x, double y, double tolerance) const {
    const QPointF point(x, y);
    for (const Bond &bond : m_bonds) {
        const Atom *a = findAtom(bond.atomA);
        const Atom *b = findAtom(bond.atomB);
        if (!a || !b) continue;
        const QLineF line(QPointF(a->x, a->y), QPointF(b->x, b->y));
        if (line.length() < 0.1) continue;
        const double projection = ((point.x() - a->x) * (b->x - a->x) + (point.y() - a->y) * (b->y - a->y)) /
                                  (line.length() * line.length());
        if (projection < 0.0 || projection > 1.0) continue;
        const QPointF nearest = line.pointAt(projection);
        if (QLineF(point, nearest).length() <= tolerance) return bond.id;
    }
    return -1;
}

QPointF Molecule::snappedBondEnd(int atomId, double x, double y) const {
    const Atom *atom = findAtom(atomId);
    return atom ? GeometryEngine::snapBondEnd(QPointF(atom->x, atom->y), QPointF(x, y)) : QPointF(x, y);
}

QPointF Molecule::suggestedBondEnd(int atomId) const {
    const Atom *atom = findAtom(atomId);
    if (!atom) return QPointF();
    constexpr double Pi = 3.14159265358979323846;
    const double length = GeometryEngine::StandardBondLength;
    const QPointF origin(atom->x, atom->y);
    const auto pointAt = [&origin, length](double angle) {
        return origin + QPointF(qCos(angle) * length, qSin(angle) * length);
    };
    const auto angleBetween = [](const Atom &from, const Atom &to) { return qAtan2(to.y - from.y, to.x - from.x); };
    const auto difference = [](double a, double b) { return qAbs(qAtan2(qSin(a - b), qCos(a - b))); };
    const auto occupied = [this, length](const QPointF &point) {
        for (const Atom &other : m_atoms)
            if (QLineF(point, QPointF(other.x, other.y)).length() < 0.5 * length) return true;
        return false;
    };

    QVector<const Atom *> neighbors;
    bool linear = false;
    int doubleBonds = 0;
    for (const Bond &bond : m_bonds) {
        const int neighborId = bond.atomA == atomId ? bond.atomB : bond.atomB == atomId ? bond.atomA : -1;
        if (const Atom *neighbor = findAtom(neighborId)) {
            neighbors.append(neighbor);
            if (bond.order == 3) linear = true;
            if (bond.order == 2) ++doubleBonds;
        }
    }
    if (doubleBonds >= 2) linear = true; // allene / cumulene centre

    // A lone atom starts a chain up and to the right, ChemDraw style.
    if (neighbors.isEmpty()) return pointAt(-Pi / 6.0);

    if (neighbors.size() == 1) {
        const Atom &neighbor = *neighbors.first();
        const double forward = angleBetween(neighbor, *atom);
        if (linear) return pointAt(forward);
        // sp2/sp3 chain atom: the new bond leaves at 120 degrees to the existing one.
        const double candidates[2] = {forward - Pi / 3.0, forward + Pi / 3.0};
        int choice = 0;
        // Zig-zag: run parallel to the bond that leads into the neighbour, so the
        // chain keeps alternating instead of curling back on itself.
        const Atom *previous = nullptr;
        for (const Bond &bond : m_bonds) {
            const int otherId = bond.atomA == neighbor.id ? bond.atomB : bond.atomB == neighbor.id ? bond.atomA : -1;
            if (otherId >= 0 && otherId != atomId) { previous = findAtom(otherId); if (previous) break; }
        }
        if (previous) {
            const double reference = angleBetween(*previous, neighbor);
            choice = difference(candidates[0], reference) <= difference(candidates[1], reference) ? 0 : 1;
        } else {
            // Two-atom chain: bend the opposite way vertically from the first bond
            // (upward when that bond is horizontal).
            const double rise = qSin(forward);
            if (qAbs(rise) < 1e-6) choice = qSin(candidates[0]) < 0 ? 0 : 1;
            else choice = (qSin(candidates[0]) > 0) == (rise < 0) ? 0 : 1;
        }
        if (occupied(pointAt(candidates[choice])) && !occupied(pointAt(candidates[1 - choice]))) choice = 1 - choice;
        return pointAt(candidates[choice]);
    }

    // Two or more bonds: the middle of the widest gap between them. That is 120
    // degrees from both bonds of a chain atom, and straight out of a ring.
    QVector<double> angles;
    for (const Atom *neighbor : neighbors) angles.append(angleBetween(*atom, *neighbor));
    std::sort(angles.begin(), angles.end());
    double bestGap = -1.0, bestAngle = 0.0;
    for (int index = 0; index < angles.size(); ++index) {
        const double start = angles[index];
        const double next = index + 1 < angles.size() ? angles[index + 1] : angles.first() + 2.0 * Pi;
        if (next - start > bestGap) { bestGap = next - start; bestAngle = start + (next - start) / 2.0; }
    }
    return pointAt(bestAngle);
}

bool Molecule::stampRing(const QString &templateName, double x, double y) {
    const int sides = ringSides(templateName);
    if (sides == 0) return false;
    const Snapshot before = snapshot();
    const QVector<QPointF> points = GeometryEngine::regularPolygon(QPointF(x, y), sides);
    QVector<int> ids;
    for (const QPointF &point : points) {
        const int id = m_nextAtomId++;
        m_atoms.append({id, QStringLiteral("C"), point.x(), point.y()});
        ids.append(id);
    }
    for (int index = 0; index < ids.size(); ++index) {
        const int order = ringBondOrder(templateName, index);
        m_bonds.append({m_nextBondId++, ids[index], ids[(index + 1) % ids.size()], order, 0,
                        templateName == QStringLiteral("benzene") ? QStringLiteral("benzene") : QStringLiteral("plain")});
    }
    commit(before);
    return true;
}

bool Molecule::stampRingAtAtom(const QString &templateName, int atomId, double x, double y) {
    const Atom *anchor = findAtom(atomId);
    const int sides = ringSides(templateName);
    if (!anchor || sides == 0) return false;

    const int anchorValence = ringBondOrder(templateName, 0) + ringBondOrder(templateName, sides - 1);
    if (valenceUsed(atomId) + anchorValence > maxValence(*anchor)) {
        emit message(QStringLiteral("That ring would exceed the selected atom's basic valence limit."));
        return false;
    }

    const QPointF target(x, y);
    const QPointF anchorPoint(anchor->x, anchor->y);
    const QPointF delta = target - anchorPoint;
    const double angle = qAtan2(delta.y(), delta.x());
    const double radius = GeometryEngine::StandardBondLength / (2.0 * qSin(qDegreesToRadians(180.0 / sides)));
    const QPointF center = anchorPoint - QPointF(qCos(angle) * radius, qSin(angle) * radius);
    const QVector<QPointF> points = GeometryEngine::regularPolygon(center, sides, radius, qRadiansToDegrees(angle));

    const Snapshot before = snapshot();
    QVector<int> ids = {atomId};
    for (int index = 1; index < points.size(); ++index) {
        const QPointF &point = points[index];
        const int id = m_nextAtomId++;
        m_atoms.append({id, QStringLiteral("C"), point.x(), point.y()});
        ids.append(id);
    }
    for (int index = 0; index < ids.size(); ++index) {
        m_bonds.append({m_nextBondId++, ids[index], ids[(index + 1) % ids.size()], ringBondOrder(templateName, index), 0,
                        templateName == QStringLiteral("benzene") ? QStringLiteral("benzene") : QStringLiteral("plain")});
    }
    commit(before);
    return true;
}

bool Molecule::stampRingAtBond(const QString &templateName, int bondId, double x, double y) {
    Bond *sharedBond = findBond(bondId);
    const int sides = ringSides(templateName);
    if (!sharedBond || sides < 3) return false;
    Atom *a = findAtom(sharedBond->atomA);
    Atom *b = findAtom(sharedBond->atomB);
    if (!a || !b) return false;

    const bool benzeneTemplate = templateName == QStringLiteral("benzene");
    const int aId = a->id, bId = b->id;
    // The new ring's own bonds run single, double, ..., single from b back to a,
    // so the shared bond keeps the order it has: fused onto either side of a
    // Kekule benzene this gives naphthalene. A plain single bond becomes double,
    // making the new ring a full benzene, when both atoms have room for it.
    int sharedOrder = sharedBond->order;
    if (benzeneTemplate && sharedOrder == 1 && valenceUsed(aId) + 2 <= maxValence(*a) && valenceUsed(bId) + 2 <= maxValence(*b))
        sharedOrder = 2;
    if (valenceUsed(aId) - sharedBond->order + sharedOrder + 1 > maxValence(*a) ||
        valenceUsed(bId) - sharedBond->order + sharedOrder + 1 > maxValence(*b)) {
        emit message(QStringLiteral("That ring would exceed the selected bond's basic valence limit."));
        return false;
    }

    const QPointF aPoint(a->x, a->y), bPoint(b->x, b->y);
    const QPointF edge = bPoint - aPoint;
    const double length = qSqrt(edge.x() * edge.x() + edge.y() * edge.y());
    if (length < 1.0) return false;
    const QPointF midpoint = (aPoint + bPoint) / 2.0;
    const double radius = length / (2.0 * qSin(qDegreesToRadians(180.0 / sides)));
    const double apothem = radius * qCos(qDegreesToRadians(180.0 / sides));
    // Build on the side with room: a bond in an existing ring already has atoms
    // where the new ring would go on the inside. Only for a bare bond does the
    // tap decide, since a tap on the bond line itself is ambiguous.
    QPointF normal(-edge.y() / length, edge.x() / length);
    if (QPointF::dotProduct(QPointF(x, y) - midpoint, normal) < 0.0) normal = -normal;
    const auto crowding = [this, aId, bId, radius](const QPointF &candidateCenter) {
        int count = 0;
        for (const Atom &atom : m_atoms)
            if (atom.id != aId && atom.id != bId && QLineF(candidateCenter, QPointF(atom.x, atom.y)).length() < radius * 1.1) ++count;
        return count;
    };
    if (crowding(midpoint - normal * apothem) < crowding(midpoint + normal * apothem)) normal = -normal;
    const QPointF center = midpoint + normal * apothem;
    const QPointF fromCenterA = aPoint - center, fromCenterB = bPoint - center;
    const double cross = fromCenterA.x() * fromCenterB.y() - fromCenterA.y() * fromCenterB.x();
    const double direction = cross >= 0.0 ? 1.0 : -1.0;
    const double firstAngle = qAtan2(fromCenterB.y(), fromCenterB.x());
    const double step = direction * (2.0 * 3.14159265358979323846 / sides);

    const Snapshot before = snapshot();
    sharedBond->order = sharedOrder;
    // A plain shared bond joins the new benzene's drawing style; a wedge, dash
    // or other style the user set is left alone.
    if (benzeneTemplate && sharedBond->style == QStringLiteral("plain")) sharedBond->style = QStringLiteral("benzene");
    QVector<int> ids = {aId, bId};
    for (int index = 2; index < sides; ++index) {
        const double angle = firstAngle + step * (index - 1);
        const QPointF point = center + QPointF(qCos(angle) * radius, qSin(angle) * radius);
        const int id = m_nextAtomId++;
        m_atoms.append({id, QStringLiteral("C"), point.x(), point.y()});
        ids.append(id);
    }
    const QString style = templateName == QStringLiteral("benzene") ? QStringLiteral("benzene") : QStringLiteral("plain");
    for (int index = 1; index < ids.size(); ++index) {
        m_bonds.append({m_nextBondId++, ids[index], ids[(index + 1) % ids.size()], ringBondOrder(templateName, index), 0, style});
    }
    commit(before);
    return true;
}

bool Molecule::stampFunctionalGroup(const QString &group, int atomId) {
    const Atom *anchor = findAtom(atomId);
    if (!anchor) return false;
    if (valenceUsed(atomId) + 1 > maxValence(*anchor)) {
        emit message(QStringLiteral("That atom has no free valence for a substituent."));
        return false;
    }
    static const QStringList groups = {QStringLiteral("OH"), QStringLiteral("NH2"), QStringLiteral("Me"), QStringLiteral("COOH"),
                                       QStringLiteral("CN"), QStringLiteral("NO2"), QStringLiteral("tBu"), QStringLiteral("Ph")};
    if (!groups.contains(group)) return false;

    // Each group is laid out in a local frame where +x is the new bond's direction
    // (the same one a tap on the atom would use) and lengths are in bond lengths,
    // then rotated into place.
    const double length = GeometryEngine::StandardBondLength;
    const QPointF origin(anchor->x, anchor->y);
    const QPointF end = suggestedBondEnd(atomId);
    const QPointF along = (end - origin) / length;
    const QPointF across(-along.y(), along.x());
    const auto place = [&](double x, double y) { return origin + along * (x * length) + across * (y * length); };
    const double sin60 = 0.8660254037844386;

    const Snapshot before = snapshot();
    const auto appendAtom = [this](const QString &element, const QPointF &point) {
        const int id = m_nextAtomId++;
        m_atoms.append({id, element, point.x(), point.y()});
        return id;
    };
    const auto appendBond = [this](int a, int b, int order) { m_bonds.append({m_nextBondId++, a, b, order, 0}); };
    if (group == QStringLiteral("OH")) {
        appendBond(atomId, appendAtom(QStringLiteral("O"), place(1, 0)), 1);
    } else if (group == QStringLiteral("NH2")) {
        appendBond(atomId, appendAtom(QStringLiteral("N"), place(1, 0)), 1);
    } else if (group == QStringLiteral("Me")) {
        appendBond(atomId, appendAtom(QStringLiteral("C"), place(1, 0)), 1);
    } else if (group == QStringLiteral("COOH") || group == QStringLiteral("NO2")) {
        // Trigonal centre: both oxygens at 120 degrees to the bond back to the anchor.
        const bool nitro = group == QStringLiteral("NO2");
        const int center = appendAtom(nitro ? QStringLiteral("N") : QStringLiteral("C"), place(1, 0));
        const int doubleOxygen = appendAtom(QStringLiteral("O"), place(1.5, -sin60));
        const int singleOxygen = appendAtom(QStringLiteral("O"), place(1.5, sin60));
        appendBond(atomId, center, 1);
        appendBond(center, doubleOxygen, 2);
        appendBond(center, singleOxygen, 1);
        if (nitro) {
            // Nitro is N+(=O)O-; without the charges nitrogen would carry four bonds
            // past its neutral valence limit.
            findAtom(center)->formalCharge = 1;
            findAtom(singleOxygen)->formalCharge = -1;
        }
    } else if (group == QStringLiteral("CN")) {
        // Nitrile carbon is sp: C and N continue in a straight line.
        const int carbon = appendAtom(QStringLiteral("C"), place(1, 0));
        const int nitrogen = appendAtom(QStringLiteral("N"), place(2, 0));
        appendBond(atomId, carbon, 1);
        appendBond(carbon, nitrogen, 3);
    } else if (group == QStringLiteral("tBu")) {
        // Quaternary carbon drawn as a cross: methyls ahead and to either side.
        const int center = appendAtom(QStringLiteral("C"), place(1, 0));
        appendBond(atomId, center, 1);
        for (const QPointF &point : {place(2, 0), place(1, -1), place(1, 1)})
            appendBond(center, appendAtom(QStringLiteral("C"), point), 1);
    } else if (group == QStringLiteral("Ph")) {
        // Ring centre two bond lengths out, ipso carbon pointing back at the anchor.
        QVector<int> phenyl;
        for (int index = 0; index < 6; ++index) {
            const double angle = 3.14159265358979323846 * (1.0 + index / 3.0);
            phenyl.append(appendAtom(QStringLiteral("C"), place(2 + qCos(angle), qSin(angle))));
        }
        appendBond(atomId, phenyl.first(), 1);
        for (int index = 0; index < phenyl.size(); ++index)
            appendBond(phenyl[index], phenyl[(index + 1) % phenyl.size()], index % 2 == 0 ? 2 : 1);
    }
    commit(before);
    return true;
}

bool Molecule::cleanUpStructure() {
    if (m_bonds.isEmpty()) return false;
    const double bondLength = GeometryEngine::StandardBondLength;

    QHash<int, int> indexOf;
    for (int index = 0; index < m_atoms.size(); ++index) indexOf.insert(m_atoms[index].id, index);
    QHash<int, QVector<int>> neighbors;
    QHash<int, QVector<int>> ordersAt;
    QSet<QPair<int, int>> bonded;
    for (const Bond &bond : m_bonds) {
        if (!indexOf.contains(bond.atomA) || !indexOf.contains(bond.atomB)) continue;
        neighbors[bond.atomA].append(bond.atomB);
        neighbors[bond.atomB].append(bond.atomA);
        ordersAt[bond.atomA].append(bond.order);
        ordersAt[bond.atomB].append(bond.order);
        bonded.insert(qMakePair(qMin(bond.atomA, bond.atomB), qMax(bond.atomA, bond.atomB)));
    }

    // Shortest path from `from` to `to` that avoids `avoid`, in bonds, or -1 when
    // none exists within maxDepth. Used to tell which ring an angle sits in.
    const auto pathAvoiding = [&neighbors](int from, int to, int avoid, int maxDepth) {
        QHash<int, int> depth{{from, 0}};
        QVector<int> queue{from};
        for (int head = 0; head < queue.size(); ++head) {
            const int current = queue[head];
            const int currentDepth = depth.value(current);
            if (currentDepth >= maxDepth) continue;
            for (const int next : neighbors.value(current)) {
                if (next == avoid || depth.contains(next)) continue;
                if (next == to) return currentDepth + 1;
                depth.insert(next, currentDepth + 1);
                queue.append(next);
            }
        }
        return -1;
    };

    // Each bond angle becomes a target 1-3 distance: ring angles follow the regular
    // polygon of the smallest ring through the pair, chain angles are 120 degrees
    // (180 next to a triple bond or an allene carbon). Crowded acyclic centers with
    // four or more neighbors are left to the bond-length and overlap springs.
    struct AngleTarget { int center, first, second; double distance; };
    QVector<AngleTarget> angles;
    for (const Atom &center : m_atoms) {
        const QVector<int> around = neighbors.value(center.id);
        const QVector<int> orders = ordersAt.value(center.id);
        const bool linear = orders.contains(3) || orders.count(2) >= 2;
        for (int i = 0; i < around.size(); ++i) {
            for (int j = i + 1; j < around.size(); ++j) {
                const int path = pathAvoiding(around[i], around[j], center.id, 6);
                double degrees = 0.0;
                if (path > 0) {
                    const int ringSize = path + 2;
                    degrees = 180.0 * (ringSize - 2) / ringSize;
                } else if (around.size() == 2) {
                    degrees = linear ? 180.0 : 120.0;
                } else if (around.size() == 3) {
                    degrees = 120.0;
                } else {
                    continue;
                }
                angles.append({center.id, around[i], around[j], 2.0 * bondLength * qSin(qDegreesToRadians(degrees) / 2.0)});
            }
        }
    }

    const Snapshot before = snapshot();
    const auto pull = [this, &indexOf](int first, int second, double target, double weight, bool pushOnly) {
        Atom &a = m_atoms[indexOf.value(first)];
        Atom &b = m_atoms[indexOf.value(second)];
        QPointF delta(b.x - a.x, b.y - a.y);
        double length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
        if (length < 0.01) { delta = QPointF(1, 0); length = 1; }
        if (pushOnly && length >= target) return;
        const double scale = weight * (length - target) / (2.0 * length);
        a.x += delta.x() * scale; a.y += delta.y() * scale;
        b.x -= delta.x() * scale; b.y -= delta.y() * scale;
    };
    const double overlap = 0.75 * bondLength;
    for (int iteration = 0; iteration < 300; ++iteration) {
        for (const Bond &bond : m_bonds) {
            if (indexOf.contains(bond.atomA) && indexOf.contains(bond.atomB)) pull(bond.atomA, bond.atomB, bondLength, 1.0, false);
        }
        for (const AngleTarget &angle : angles) {
            // A distance spring cannot bend three exactly collinear atoms, so nudge
            // one end sideways first when the angle should not be straight.
            const Atom &center = m_atoms[indexOf.value(angle.center)];
            Atom &first = m_atoms[indexOf.value(angle.first)];
            Atom &second = m_atoms[indexOf.value(angle.second)];
            if (angle.distance >= 1.99 * bondLength) {
                // Near 180 degrees a 1-3 distance spring has almost no leverage, so
                // pull both ends onto the line through the center instead.
                QPointF direction(first.x - second.x, first.y - second.y);
                const double length = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
                direction = length < 0.01 ? QPointF(1, 0) : direction / length;
                first.x += 0.5 * (center.x + direction.x() * bondLength - first.x);
                first.y += 0.5 * (center.y + direction.y() * bondLength - first.y);
                second.x += 0.5 * (center.x - direction.x() * bondLength - second.x);
                second.y += 0.5 * (center.y - direction.y() * bondLength - second.y);
                continue;
            }
            const QPointF u(first.x - center.x, first.y - center.y), w(second.x - center.x, second.y - center.y);
            const double cross = u.x() * w.y() - u.y() * w.x();
            if (qAbs(cross) < 1e-3 && angle.distance < 1.99 * bondLength) {
                const double length = qMax(0.01, qSqrt(u.x() * u.x() + u.y() * u.y()));
                first.x += -u.y() / length * 0.1 * bondLength;
                first.y += u.x() / length * 0.1 * bondLength;
            }
            pull(angle.first, angle.second, angle.distance, 0.5, false);
        }
        for (int i = 0; i < m_atoms.size(); ++i) {
            for (int j = i + 1; j < m_atoms.size(); ++j) {
                const int a = m_atoms[i].id, b = m_atoms[j].id;
                if (bonded.contains(qMakePair(qMin(a, b), qMax(a, b)))) continue;
                pull(a, b, overlap, 0.5, true);
            }
        }
    }
    commit(before);
    return true;
}

bool Molecule::canAddBond(int atomA, int atomB, int order) const {
    if (atomA == atomB || !findAtom(atomA) || !findAtom(atomB) || order < 1 || order > 3) return false;
    for (const Bond &bond : m_bonds) if ((bond.atomA == atomA && bond.atomB == atomB) || (bond.atomA == atomB && bond.atomB == atomA)) return false;
    return valenceUsed(atomA) + order <= maxValence(*findAtom(atomA)) &&
           valenceUsed(atomB) + order <= maxValence(*findAtom(atomB));
}

QString Molecule::formula() const {
    // C and H first, then the other elements alphabetically. Isotope-labelled atoms
    // are listed after their element as e.g. "[13C]" or "[2H]".
    QMap<QString, QMap<int, int>> counts;
    for (const Atom &atom : m_atoms) {
        counts[atom.element][atom.isotope] += 1;
        if (atom.element != QStringLiteral("H")) counts[QStringLiteral("H")][0] += implicitHydrogens(atom);
    }
    QString result;
    const auto append = [&result, &counts](const QString &element) {
        const QMap<int, int> isotopes = counts.take(element);
        for (auto it = isotopes.cbegin(); it != isotopes.cend(); ++it) {
            if (it.value() <= 0) continue;
            result += it.key() ? QStringLiteral("[%1%2]").arg(it.key()).arg(element) : element;
            if (it.value() > 1) result += QString::number(it.value());
        }
    };
    append(QStringLiteral("C"));
    append(QStringLiteral("H"));
    while (!counts.isEmpty()) append(counts.firstKey());
    int totalCharge = 0;
    for (const Atom &atom : m_atoms) totalCharge += atom.formalCharge;
    if (totalCharge) result += totalCharge == 1 ? QStringLiteral("+") : totalCharge == -1 ? QStringLiteral("-") :
                             totalCharge > 0 ? QStringLiteral("%1+").arg(totalCharge) : QStringLiteral("%1-").arg(-totalCharge);
    return result;
}

QVariantMap Molecule::properties() const {
    const Elements::Info *hydrogen = Elements::find(QStringLiteral("H"));
    double molecularWeight = 0.0;
    double exactMass = 0.0;
    QStringList unknown;
    for (const Atom &atom : m_atoms) {
        const Elements::Info *info = Elements::find(atom.element);
        if (!info && !unknown.contains(atom.element)) unknown.append(atom.element);
        // A labelled atom contributes its mass number; that is within ~0.01 Da of
        // the true isotopic mass for the light isotopes used in labelling.
        molecularWeight += atom.isotope ? atom.isotope : (info ? info->averageMass : 0.0);
        exactMass += atom.isotope ? atom.isotope : (info ? info->monoisotopicMass : 0.0);
        const int implicit = implicitHydrogens(atom);
        molecularWeight += implicit * hydrogen->averageMass;
        exactMass += implicit * hydrogen->monoisotopicMass;
    }
    return {{QStringLiteral("formula"), formula()}, {QStringLiteral("molecularWeight"), molecularWeight},
            {QStringLiteral("exactMass"), exactMass}, {QStringLiteral("atomCount"), m_atoms.size()},
            {QStringLiteral("bondCount"), m_bonds.size()}, {QStringLiteral("unknownElements"), unknown}};
}

bool Molecule::saveToFile(const QString &path) {
    const QFileInfo info(path);
    if (path.trimmed().isEmpty() || !QDir().mkpath(info.absolutePath())) {
        emit message(QStringLiteral("Could not create the export folder."));
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit message(QStringLiteral("Could not write %1.").arg(path));
        return false;
    }
    file.write(MolfileWriter::writeV2000(*this, info.completeBaseName()).toUtf8());
    file.close();
    emit message(QStringLiteral("Saved %1.").arg(path));
    return true;
}

bool Molecule::loadFromFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit message(QStringLiteral("Could not open %1.").arg(path));
        return false;
    }
    QString error;
    if (!MolfileReader::readV2000(QString::fromUtf8(file.readAll()), this, &error)) {
        emit message(QStringLiteral("Could not load %1: %2").arg(path, error));
        return false;
    }
    emit message(QStringLiteral("Loaded %1.").arg(path));
    return true;
}

bool Molecule::saveSvgToFile(const QString &path) {
    const QFileInfo info(path);
    if (path.trimmed().isEmpty() || !QDir().mkpath(info.absolutePath())) return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    file.write(SvgWriter::write(*this).toUtf8());
    emit message(QStringLiteral("Saved %1.").arg(path));
    return true;
}

bool Molecule::undo() {
    if (m_undo.isEmpty()) return false;
    m_redo.append(snapshot());
    restore(m_undo.takeLast());
    emit changed();
    return true;
}

bool Molecule::redo() {
    if (m_redo.isEmpty()) return false;
    m_undo.append(snapshot());
    restore(m_redo.takeLast());
    emit changed();
    return true;
}

void Molecule::clear() {
    if (m_atoms.isEmpty() && m_bonds.isEmpty()) return;
    const Snapshot before = snapshot();
    m_atoms.clear(); m_bonds.clear();
    commit(before);
}

Atom *Molecule::findAtom(int id) { for (Atom &atom : m_atoms) if (atom.id == id) return &atom; return nullptr; }
const Atom *Molecule::findAtom(int id) const { for (const Atom &atom : m_atoms) if (atom.id == id) return &atom; return nullptr; }
Bond *Molecule::findBond(int id) { for (Bond &bond : m_bonds) if (bond.id == id) return &bond; return nullptr; }
const Bond *Molecule::findBond(int id) const { for (const Bond &bond : m_bonds) if (bond.id == id) return &bond; return nullptr; }

int Molecule::valenceUsed(int atomId) const { int total = 0; for (const Bond &bond : m_bonds) if (bond.atomA == atomId || bond.atomB == atomId) total += bond.order; return total; }

int Molecule::maxValence(const QString &element, int charge) const { return Elements::maxValence(element, charge); }

int Molecule::maxValence(const Atom &atom) const { return maxValence(atom.element, atom.formalCharge); }

int Molecule::implicitHydrogens(const Atom &atom) const {
    // Aromatic-style bonds are stored with order 1 but count 1.5 toward hydrogen
    // filling, so a ring drawn with aromatic bonds gives C6H6 rather than C6H12.
    double sum = 0.0;
    for (const Bond &bond : m_bonds) {
        if (bond.atomA != atom.id && bond.atomB != atom.id) continue;
        sum += bond.style == QStringLiteral("aromatic") ? 1.5 : bond.order;
    }
    return Elements::implicitHydrogens(atom.element, atom.formalCharge, qCeil(sum - 1e-9));
}

QString Molecule::smiles() const { return SmilesWriter::write(*this); }

bool Molecule::importSmiles(const QString &smiles, double x, double y) {
    QVector<Atom> atoms;
    QVector<Bond> bonds;
    QString error;
    QStringList warnings;
    if (!SmilesReader::build(smiles, x, y, atoms, bonds, &error, &warnings)) {
        emit message(error);
        return false;
    }
    const Snapshot before = snapshot();
    const int atomOffset = m_nextAtomId - 1, bondOffset = m_nextBondId - 1;
    for (Atom atom : std::as_const(atoms)) {
        atom.id += atomOffset;
        m_atoms.append(atom);
    }
    for (Bond bond : std::as_const(bonds)) {
        bond.id += bondOffset;
        bond.atomA += atomOffset;
        bond.atomB += atomOffset;
        m_bonds.append(bond);
    }
    m_nextAtomId += atoms.size();
    m_nextBondId += bonds.size();
    commit(before);
    emit message(warnings.isEmpty() ? QStringLiteral("Imported SMILES.")
                                    : QStringLiteral("Imported SMILES. ") + warnings.join(QLatin1Char(' ')));
    return true;
}

double Molecule::standardBondLength() const { return GeometryEngine::StandardBondLength; }

Molecule::Snapshot Molecule::snapshot() const { return {m_atoms, m_bonds, m_nextAtomId, m_nextBondId}; }
void Molecule::restore(const Snapshot &value) { m_atoms = value.atoms; m_bonds = value.bonds; m_nextAtomId = value.nextAtomId; m_nextBondId = value.nextBondId; }
void Molecule::commit(const Snapshot &before) {
    m_undo.append(before);
    if (m_undo.size() > MaxUndoSteps) m_undo.removeFirst();
    m_redo.clear();
    emit committed();
    emit changed();
}
