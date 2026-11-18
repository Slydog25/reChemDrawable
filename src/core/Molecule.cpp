#include "Molecule.h"

#include "GeometryEngine.h"
#include "MolfileReader.h"
#include "MolfileWriter.h"
#include "SvgWriter.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLineF>
#include <QMap>
#include <QSet>
#include <QStandardPaths>
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
    bond->stereo = style == QStringLiteral("wedge") ? 1 : style == QStringLiteral("dash") ? 2 : 0;
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

    QVector<double> occupiedAngles;
    for (const Bond &bond : m_bonds) {
        const int neighborId = bond.atomA == atomId ? bond.atomB : bond.atomB == atomId ? bond.atomA : -1;
        const Atom *neighbor = findAtom(neighborId);
        if (!neighbor) continue;
        occupiedAngles.append(qAtan2(neighbor->y - atom->y, neighbor->x - atom->x));
    }
    if (occupiedAngles.isEmpty()) {
        return QPointF(atom->x + GeometryEngine::StandardBondLength, atom->y);
    }

    constexpr int CandidateCount = 12;
    constexpr double Pi = 3.14159265358979323846;
    double bestAngle = 0.0;
    double bestClearance = -1.0;
    for (int candidate = 0; candidate < CandidateCount; ++candidate) {
        const double angle = candidate * 2.0 * Pi / CandidateCount;
        double clearance = 2.0 * Pi;
        for (const double occupied : occupiedAngles) {
            const double difference = qAbs(qAtan2(qSin(angle - occupied), qCos(angle - occupied)));
            clearance = qMin(clearance, difference);
        }
        if (clearance > bestClearance) {
            bestClearance = clearance;
            bestAngle = angle;
        }
    }
    return QPointF(atom->x + qCos(bestAngle) * GeometryEngine::StandardBondLength,
                   atom->y + qSin(bestAngle) * GeometryEngine::StandardBondLength);
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

    const int desiredSharedOrder = ringBondOrder(templateName, 0);
    const int aAddedOrder = ringBondOrder(templateName, sides - 1);
    const int bAddedOrder = ringBondOrder(templateName, 1);
    if (valenceUsed(a->id) - sharedBond->order + desiredSharedOrder + aAddedOrder > maxValence(*a) ||
        valenceUsed(b->id) - sharedBond->order + desiredSharedOrder + bAddedOrder > maxValence(*b)) {
        emit message(QStringLiteral("That ring would exceed the selected bond's basic valence limit."));
        return false;
    }

    const QPointF aPoint(a->x, a->y), bPoint(b->x, b->y);
    const QPointF edge = bPoint - aPoint;
    const double length = qSqrt(edge.x() * edge.x() + edge.y() * edge.y());
    if (length < 1.0) return false;
    const QPointF midpoint = (aPoint + bPoint) / 2.0;
    QPointF normal(-edge.y() / length, edge.x() / length);
    const QPointF target(x, y);
    if (QPointF::dotProduct(target - midpoint, normal) < 0.0) normal = -normal;
    const double radius = length / (2.0 * qSin(qDegreesToRadians(180.0 / sides)));
    const double apothem = radius * qCos(qDegreesToRadians(180.0 / sides));
    const QPointF center = midpoint + normal * apothem;
    const QPointF fromCenterA = aPoint - center, fromCenterB = bPoint - center;
    const double cross = fromCenterA.x() * fromCenterB.y() - fromCenterA.y() * fromCenterB.x();
    const double direction = cross >= 0.0 ? 1.0 : -1.0;
    const double firstAngle = qAtan2(fromCenterB.y(), fromCenterB.x());
    const double step = direction * (2.0 * 3.14159265358979323846 / sides);

    const Snapshot before = snapshot();
    sharedBond->order = desiredSharedOrder;
    sharedBond->style = templateName == QStringLiteral("benzene") ? QStringLiteral("benzene") : QStringLiteral("plain");
    QVector<int> ids = {a->id, b->id};
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
    Atom *anchor = findAtom(atomId);
    if (!anchor || valenceUsed(atomId) + 1 > maxValence(*anchor)) return false;
    const Snapshot before = snapshot();
    const QPointF firstPoint(anchor->x + GeometryEngine::StandardBondLength, anchor->y);
    const auto appendAtom = [this](const QString &element, const QPointF &point) {
        const int id = m_nextAtomId++;
        m_atoms.append({id, element, point.x(), point.y()});
        return id;
    };
    const auto appendBond = [this](int a, int b, int order) { m_bonds.append({m_nextBondId++, a, b, order, 0}); };
    if (group == QStringLiteral("OH")) {
        appendBond(atomId, appendAtom(QStringLiteral("O"), firstPoint), 1);
    } else if (group == QStringLiteral("NH2")) {
        appendBond(atomId, appendAtom(QStringLiteral("N"), firstPoint), 1);
    } else if (group == QStringLiteral("Me")) {
        appendBond(atomId, appendAtom(QStringLiteral("C"), firstPoint), 1);
    } else if (group == QStringLiteral("COOH")) {
        const int carbon = appendAtom(QStringLiteral("C"), firstPoint);
        const int oxygen1 = appendAtom(QStringLiteral("O"), firstPoint + QPointF(0, -GeometryEngine::StandardBondLength));
        const int oxygen2 = appendAtom(QStringLiteral("O"), firstPoint + QPointF(0, GeometryEngine::StandardBondLength));
        appendBond(atomId, carbon, 1);
        appendBond(carbon, oxygen1, 2);
        appendBond(carbon, oxygen2, 1);
    } else if (group == QStringLiteral("CN")) {
        const int carbon = appendAtom(QStringLiteral("C"), firstPoint);
        const int nitrogen = appendAtom(QStringLiteral("N"), firstPoint + QPointF(GeometryEngine::StandardBondLength, 0));
        appendBond(atomId, carbon, 1);
        appendBond(carbon, nitrogen, 3);
    } else if (group == QStringLiteral("NO2")) {
        const int nitrogen = appendAtom(QStringLiteral("N"), firstPoint);
        const int oxygen1 = appendAtom(QStringLiteral("O"), firstPoint + QPointF(GeometryEngine::StandardBondLength, -GeometryEngine::StandardBondLength));
        const int oxygen2 = appendAtom(QStringLiteral("O"), firstPoint + QPointF(GeometryEngine::StandardBondLength, GeometryEngine::StandardBondLength));
        appendBond(atomId, nitrogen, 1);
        appendBond(nitrogen, oxygen1, 2);
        appendBond(nitrogen, oxygen2, 1);
    } else if (group == QStringLiteral("tBu")) {
        const int center = appendAtom(QStringLiteral("C"), firstPoint);
        appendBond(atomId, center, 1);
        const QVector<QPointF> branches = {firstPoint + QPointF(GeometryEngine::StandardBondLength, 0),
                                           firstPoint + QPointF(-24, -42), firstPoint + QPointF(-24, 42)};
        for (const QPointF &point : branches) appendBond(center, appendAtom(QStringLiteral("C"), point), 1);
    } else if (group == QStringLiteral("Ph")) {
        const QVector<QPointF> points = GeometryEngine::regularPolygon(firstPoint + QPointF(GeometryEngine::StandardBondLength, 0), 6,
                                                                         GeometryEngine::StandardBondLength, 180.0);
        QVector<int> phenyl;
        for (const QPointF &point : points) phenyl.append(appendAtom(QStringLiteral("C"), point));
        appendBond(atomId, phenyl.first(), 1);
        for (int index = 0; index < phenyl.size(); ++index)
            appendBond(phenyl[index], phenyl[(index + 1) % phenyl.size()], index % 2 == 0 ? 2 : 1);
    } else {
        return false;
    }
    commit(before);
    return true;
}

bool Molecule::cleanUpStructure() {
    if (m_bonds.isEmpty()) return false;
    const Snapshot before = snapshot();
    for (int iteration = 0; iteration < 24; ++iteration) {
        for (const Bond &bond : m_bonds) {
            Atom *a = findAtom(bond.atomA);
            Atom *b = findAtom(bond.atomB);
            if (!a || !b) continue;
            QPointF delta(b->x - a->x, b->y - a->y);
            double length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
            if (length < 0.01) { delta = QPointF(1, 0); length = 1; }
            const double scale = (length - GeometryEngine::StandardBondLength) / (2.0 * length);
            const QPointF shift(delta.x() * scale, delta.y() * scale);
            a->x += shift.x(); a->y += shift.y();
            b->x -= shift.x(); b->y -= shift.y();
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
    QMap<QString, int> counts;
    for (const Atom &atom : m_atoms) {
        counts[atom.element] += 1;
        const int implicitHydrogen = qMax(0, maxValence(atom) - valenceUsed(atom.id));
        if (atom.element != QStringLiteral("H")) counts[QStringLiteral("H")] += implicitHydrogen;
    }
    QString result;
    const auto append = [&result, &counts](const QString &element) {
        const int count = counts.take(element);
        if (count) result += element + (count == 1 ? QString() : QString::number(count));
    };
    append(QStringLiteral("C"));
    append(QStringLiteral("H"));
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        result += it.key() + (it.value() == 1 ? QString() : QString::number(it.value()));
    }
    int totalCharge = 0;
    for (const Atom &atom : m_atoms) totalCharge += atom.formalCharge;
    if (totalCharge) result += totalCharge == 1 ? QStringLiteral("+") : totalCharge == -1 ? QStringLiteral("-") :
                             totalCharge > 0 ? QStringLiteral("%1+").arg(totalCharge) : QStringLiteral("%1-").arg(-totalCharge);
    return result;
}

QVariantMap Molecule::properties() const {
    static const QHash<QString, double> averageMass = {{QStringLiteral("H"), 1.008}, {QStringLiteral("C"), 12.011},
        {QStringLiteral("N"), 14.007}, {QStringLiteral("O"), 15.999}, {QStringLiteral("F"), 18.998}, {QStringLiteral("P"), 30.974},
        {QStringLiteral("S"), 32.06}, {QStringLiteral("Cl"), 35.45}, {QStringLiteral("Br"), 79.904}, {QStringLiteral("I"), 126.904}};
    static const QHash<QString, double> monoisotopicMass = {{QStringLiteral("H"), 1.007825}, {QStringLiteral("C"), 12.0},
        {QStringLiteral("N"), 14.003074}, {QStringLiteral("O"), 15.994915}, {QStringLiteral("F"), 18.998403}, {QStringLiteral("P"), 30.973762},
        {QStringLiteral("S"), 31.972071}, {QStringLiteral("Cl"), 34.968853}, {QStringLiteral("Br"), 78.918338}, {QStringLiteral("I"), 126.904468}};
    double molecularWeight = 0.0;
    double exactMass = 0.0;
    for (const Atom &atom : m_atoms) {
        molecularWeight += averageMass.value(atom.element);
        exactMass += atom.isotope ? atom.isotope : monoisotopicMass.value(atom.element);
        if (atom.element != QStringLiteral("H")) {
            const int hydrogen = qMax(0, maxValence(atom) - valenceUsed(atom.id));
            molecularWeight += hydrogen * averageMass.value(QStringLiteral("H"));
            exactMass += hydrogen * monoisotopicMass.value(QStringLiteral("H"));
        }
    }
    return {{QStringLiteral("formula"), formula()}, {QStringLiteral("molecularWeight"), molecularWeight},
            {QStringLiteral("exactMass"), exactMass}, {QStringLiteral("atomCount"), m_atoms.size()},
            {QStringLiteral("bondCount"), m_bonds.size()}};
}

QString Molecule::defaultSavePath() const {
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) {
        documents = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    return QDir(documents).filePath(QStringLiteral("reChemDrawable/structure.mol"));
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

bool Molecule::saveDefaultMolfile() { return saveToFile(defaultSavePath()); }
bool Molecule::loadDefaultMolfile() { return loadFromFile(defaultSavePath()); }
bool Molecule::saveDefaultSvg() { return saveSvgToFile(QFileInfo(defaultSavePath()).dir().filePath(QStringLiteral("structure.svg"))); }

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

int Molecule::maxValence(const QString &element, int charge) const {
    static const QHash<QString, int> values = {{QStringLiteral("C"), 4}, {QStringLiteral("N"), 3}, {QStringLiteral("O"), 2},
        {QStringLiteral("S"), 6}, {QStringLiteral("P"), 5}, {QStringLiteral("F"), 1}, {QStringLiteral("Cl"), 1},
        {QStringLiteral("Br"), 1}, {QStringLiteral("I"), 1}, {QStringLiteral("H"), 1}};
    const auto found = values.constFind(element);
    // Unknown elements stay permissive and ignore charge.
    if (found == values.cend()) return 4;
    // Carbon and hydrogen lose a bonding site for either sign of charge (carbocation,
    // carbanion, proton, hydride). Group 15-17 atoms gain one per positive charge
    // (ammonium, oxonium) and lose one per negative charge (amide, alkoxide, halide).
    if (element == QStringLiteral("C") || element == QStringLiteral("H")) return qMax(0, *found - qAbs(charge));
    return qMax(0, *found + charge);
}

int Molecule::maxValence(const Atom &atom) const { return maxValence(atom.element, atom.formalCharge); }

Molecule::Snapshot Molecule::snapshot() const { return {m_atoms, m_bonds, m_nextAtomId, m_nextBondId}; }
void Molecule::restore(const Snapshot &value) { m_atoms = value.atoms; m_bonds = value.bonds; m_nextAtomId = value.nextAtomId; m_nextBondId = value.nextBondId; }
void Molecule::commit(const Snapshot &before) { m_undo.append(before); m_redo.clear(); emit committed(); emit changed(); }
