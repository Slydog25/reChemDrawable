#include "ReactionLayer.h"

#include "Molecule.h"
#include "MolfileReader.h"
#include "MolfileWriter.h"
#include "SchemeAnalysis.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace {
double segmentDistance(const QPointF &point, const QPointF &start, const QPointF &end) {
    const QLineF line(start, end);
    const double lengthSquared = line.length() * line.length();
    if (lengthSquared < 0.01) return QLineF(point, start).length();
    const double fraction = qBound(0.0, QPointF::dotProduct(point - start, end - start) / lengthSquared, 1.0);
    return QLineF(point, start + (end - start) * fraction).length();
}

QPointF quadPoint(const QPointF &a, const QPointF &c, const QPointF &b, double t) {
    const double u = 1.0 - t;
    return a * (u * u) + c * (2 * u * t) + b * (t * t);
}

// Distance from a point to the quadratic curve a -> c -> b (polyline approximation).
double curveDistance(const QPointF &point, const QPointF &a, const QPointF &c, const QPointF &b) {
    constexpr int segments = 32;
    double best = 1e18;
    QPointF previous = a;
    for (int i = 1; i <= segments; ++i) {
        const QPointF next = quadPoint(a, c, b, double(i) / segments);
        best = qMin(best, segmentDistance(point, previous, next));
        previous = next;
    }
    return best;
}

QPointF pathPoint(const QVariant &value) {
    if (value.typeId() == QMetaType::QPointF) return value.toPointF();
    const QVariantMap map = value.toMap();
    return QPointF(map.value(QStringLiteral("x")).toDouble(), map.value(QStringLiteral("y")).toDouble());
}
}

bool ReactionLayer::isCurvedKind(const QString &kind) {
    return kind == QStringLiteral("curved") || kind == QStringLiteral("fishhook");
}

QString ReactionLayer::normalizedKind(const QString &kind) {
    static const QStringList known{QStringLiteral("forward"), QStringLiteral("equilibrium"), QStringLiteral("retro"),
                                   QStringLiteral("resonance"), QStringLiteral("dashed"), QStringLiteral("noreaction"),
                                   QStringLiteral("curved"), QStringLiteral("fishhook")};
    return known.contains(kind) ? kind : QStringLiteral("forward");
}

bool ReactionLayer::isCompoundNumber(const QString &text) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9]+[a-z]?$"));
    return pattern.match(text.trimmed()).hasMatch();
}

QVariantMap ReactionLayer::controlFromPath(const QVariantList &path) {
    if (path.size() < 2) return {};
    const QPointF start = pathPoint(path.first()), end = pathPoint(path.last());
    const QPointF mid = (start + end) / 2.0;
    const QLineF chord(start, end);
    if (chord.length() < 0.01) return QVariantMap{{QStringLiteral("x"), mid.x()}, {QStringLiteral("y"), mid.y()}};
    const QPointF normal(-chord.dy() / chord.length(), chord.dx() / chord.length());
    double bestOffset = 0.0;
    for (const QVariant &value : path) {
        const double offset = QPointF::dotProduct(pathPoint(value) - start, normal);
        if (qAbs(offset) > qAbs(bestOffset)) bestOffset = offset;
    }
    // A quadratic curve peaks at half its control point's offset from the chord.
    const QPointF control = mid + normal * (2.0 * bestOffset);
    return QVariantMap{{QStringLiteral("x"), control.x()}, {QStringLiteral("y"), control.y()}};
}

ReactionLayer::ReactionLayer(Molecule *molecule, QObject *parent) : QObject(parent), m_molecule(molecule) {}

QVariantList ReactionLayer::items() const {
    QVariantList result;
    for (const Item &item : m_items) {
        result.append(QVariantMap{{QStringLiteral("id"), item.id}, {QStringLiteral("type"), item.type},
                                  {QStringLiteral("x1"), item.x1}, {QStringLiteral("y1"), item.y1},
                                  {QStringLiteral("x2"), item.x2}, {QStringLiteral("y2"), item.y2},
                                  {QStringLiteral("text"), item.text},
                                  {QStringLiteral("kind"), item.kind}, {QStringLiteral("cx"), item.cx}, {QStringLiteral("cy"), item.cy},
                                  {QStringLiteral("bold"), item.type == QStringLiteral("label") && isCompoundNumber(item.text)}});
    }
    return result;
}

int ReactionLayer::addArrow(double startX, double startY, double endX, double endY) {
    if (QLineF(QPointF(startX, startY), QPointF(endX, endY)).length() < 12.0) return -1;
    const Snapshot before = snapshot();
    const int id = m_nextId++;
    Item item{id, QStringLiteral("arrow"), startX, startY, endX, endY, {}};
    item.cx = (startX + endX) / 2.0; item.cy = (startY + endY) / 2.0;
    m_items.append(item);
    commit(before);
    return id;
}

int ReactionLayer::addArrowKind(const QString &kindName, double startX, double startY, double endX, double endY,
                                double controlX, double controlY) {
    const QLineF chord(QPointF(startX, startY), QPointF(endX, endY));
    if (chord.length() < 12.0) return -1;
    const QString kind = normalizedKind(kindName);
    const Snapshot before = snapshot();
    const int id = m_nextId++;
    Item item{id, QStringLiteral("arrow"), startX, startY, endX, endY, {}};
    item.kind = kind;
    item.cx = (startX + endX) / 2.0; item.cy = (startY + endY) / 2.0;
    if (isCurvedKind(kind)) {
        if (std::isnan(controlX) || std::isnan(controlY)) {
            // Default bulge to the left of the direction of travel.
            item.cx += chord.dy() * 0.35; item.cy -= chord.dx() * 0.35;
        } else {
            item.cx = controlX; item.cy = controlY;
        }
    }
    m_items.append(item);
    commit(before);
    return id;
}

int ReactionLayer::addArrowFromPath(const QString &kind, const QVariantList &path) {
    if (path.size() < 2) return -1;
    const QPointF start = pathPoint(path.first()), end = pathPoint(path.last());
    if (!isCurvedKind(normalizedKind(kind))) return addArrowKind(kind, start.x(), start.y(), end.x(), end.y());
    const QVariantMap control = controlFromPath(path);
    // Nearly straight drag: keep the default bulge so the kind is still recognisable.
    const QPointF mid = (start + end) / 2.0;
    const QPointF c(control.value(QStringLiteral("x")).toDouble(), control.value(QStringLiteral("y")).toDouble());
    if (QLineF(mid, c).length() < 6.0)
        return addArrowKind(kind, start.x(), start.y(), end.x(), end.y());
    return addArrowKind(kind, start.x(), start.y(), end.x(), end.y(), c.x(), c.y());
}

QVariantMap ReactionLayer::snapLabelPosition(double x, double y, double tolerance) const {
    const QPointF tap(x, y);
    const Item *best = nullptr;
    double bestDistance = tolerance;
    for (const Item &item : m_items) {
        if (item.type != QStringLiteral("arrow") || isCurvedKind(item.kind)) continue;
        const double distance = segmentDistance(tap, QPointF(item.x1, item.y1), QPointF(item.x2, item.y2));
        if (distance <= bestDistance) { best = &item; bestDistance = distance; }
    }
    if (!best) return {};
    const QPointF start(best->x1, best->y1), end(best->x2, best->y2);
    const QLineF chord(start, end);
    const QPointF mid = (start + end) / 2.0;
    QPointF normal(chord.dy() / chord.length(), -chord.dx() / chord.length());
    // Choose the side the tap is on; on the line itself, prefer above (smaller y).
    const double side = QPointF::dotProduct(tap - mid, normal);
    if (qAbs(side) > 0.5 ? side < 0 : normal.y() > 0) normal = -normal;
    constexpr double offset = 22.0;
    const QPointF where = mid + normal * offset;
    return QVariantMap{{QStringLiteral("x"), where.x()}, {QStringLiteral("y"), where.y()}, {QStringLiteral("arrow"), best->id}};
}

int ReactionLayer::addLabelSnapped(const QString &text, double x, double y, double tolerance) {
    const QVariantMap snapped = snapLabelPosition(x, y, tolerance);
    if (snapped.isEmpty()) return addLabel(text, x, y);
    return addLabel(text, snapped.value(QStringLiteral("x")).toDouble(), snapped.value(QStringLiteral("y")).toDouble());
}

int ReactionLayer::addLabel(const QString &text, double x, double y) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return -1;
    const Snapshot before = snapshot();
    const int id = m_nextId++;
    m_items.append({id, QStringLiteral("label"), x, y, x, y, trimmed});
    commit(before);
    return id;
}

int ReactionLayer::addDagger(double x, double y) {
    const Snapshot before = snapshot();
    const int id = m_nextId++;
    Item item{id, QStringLiteral("dagger"), x, y, x, y, {}};
    m_items.append(item);
    commit(before);
    return id;
}

int ReactionLayer::addBracket(double x1, double y1, double x2, double y2, bool withDagger) {
    const double left = qMin(x1, x2), right = qMax(x1, x2), top = qMin(y1, y2), bottom = qMax(y1, y2);
    if (right - left < 12.0 || bottom - top < 12.0) return -1;
    const Snapshot before = snapshot();
    const int id = m_nextId++;
    Item item{id, QStringLiteral("bracket"), left, top, right, bottom, {}};
    item.kind = withDagger ? QStringLiteral("dagger") : QStringLiteral("plain");
    m_items.append(item);
    commit(before);
    return id;
}

int ReactionLayer::addOrbital(double x, double y, double dirX, double dirY, bool shaded) {
    double length = std::sqrt(dirX * dirX + dirY * dirY);
    if (length < 1e-6) { dirX = 0.0; dirY = -1.0; length = 1.0; }
    const Snapshot before = snapshot();
    const int id = m_nextId++;
    // (x2, y2) is the tip of the first lobe; the other lobe is its mirror through (x1, y1).
    Item item{id, QStringLiteral("orbital"), x, y, x + dirX / length * OrbitalLobeLength, y + dirY / length * OrbitalLobeLength, {}};
    item.kind = shaded ? QStringLiteral("shaded") : QStringLiteral("plain");
    m_items.append(item);
    commit(before);
    return id;
}

bool ReactionLayer::setArrowEnds(int itemId, double startX, double startY, double endX, double endY) {
    for (Item &item : m_items) {
        if (item.id != itemId || item.type != QStringLiteral("arrow") || isCurvedKind(item.kind)) continue;
        if (item.x1 == startX && item.y1 == startY && item.x2 == endX && item.y2 == endY) return false;
        const Snapshot before = snapshot();
        for (Item &target : m_items) {
            if (target.id != itemId) continue;
            target.x1 = startX; target.y1 = startY; target.x2 = endX; target.y2 = endY;
            target.cx = (startX + endX) / 2.0; target.cy = (startY + endY) / 2.0;
        }
        commit(before);
        return true;
    }
    return false;
}

QVariantList ReactionLayer::checkBalance() const {
    return m_molecule ? SchemeAnalysis::checkReactions(*m_molecule, items()) : QVariantList();
}

int ReactionLayer::itemAt(double x, double y, double tolerance) const {
    const QPointF point(x, y);
    for (auto iterator = m_items.crbegin(); iterator != m_items.crend(); ++iterator) {
        if (iterator->type == QStringLiteral("arrow")) {
            const QPointF a(iterator->x1, iterator->y1), b(iterator->x2, iterator->y2);
            const double distance = isCurvedKind(iterator->kind) ? curveDistance(point, a, QPointF(iterator->cx, iterator->cy), b)
                                                                 : segmentDistance(point, a, b);
            if (distance <= tolerance) return iterator->id;
        }
        if (iterator->type == QStringLiteral("dagger")) {
            if (qAbs(x - iterator->x1) <= tolerance && qAbs(y - iterator->y1) <= tolerance + 4.0) return iterator->id;
        }
        if (iterator->type == QStringLiteral("bracket")) {
            // Only the bracket strokes hit, so atoms inside the brackets stay reachable.
            const double edge = qMin(segmentDistance(point, QPointF(iterator->x1, iterator->y1), QPointF(iterator->x1, iterator->y2)),
                                     segmentDistance(point, QPointF(iterator->x2, iterator->y1), QPointF(iterator->x2, iterator->y2)));
            if (edge <= tolerance) return iterator->id;
        }
        if (iterator->type == QStringLiteral("orbital")) {
            const QPointF centre(iterator->x1, iterator->y1), tip(iterator->x2, iterator->y2);
            if (segmentDistance(point, centre * 2.0 - tip, tip) <= tolerance) return iterator->id;
        }
        if (iterator->type == QStringLiteral("label")) {
            const double halfWidth = qMax(24.0, iterator->text.size() * 5.5);
            if (qAbs(x - iterator->x1) <= halfWidth && qAbs(y - iterator->y1) <= tolerance) return iterator->id;
        }
    }
    return -1;
}

bool ReactionLayer::translateItem(int itemId, double dx, double dy) {
    if (qFuzzyIsNull(dx) && qFuzzyIsNull(dy)) return false;
    for (Item &item : m_items) {
        if (item.id != itemId) continue;
        const Snapshot before = snapshot();
        item.x1 += dx; item.y1 += dy; item.x2 += dx; item.y2 += dy; item.cx += dx; item.cy += dy;
        commit(before);
        return true;
    }
    return false;
}

bool ReactionLayer::removeItem(int itemId) {
    for (int index = 0; index < m_items.size(); ++index) {
        if (m_items[index].id != itemId) continue;
        const Snapshot before = snapshot();
        m_items.removeAt(index);
        commit(before);
        return true;
    }
    return false;
}

bool ReactionLayer::saveToFile(const QString &path) {
    const QFileInfo info(path);
    if (!m_molecule || path.trimmed().isEmpty() || !QDir().mkpath(info.absolutePath())) return false;
    QJsonArray annotations;
    for (const Item &item : m_items) {
        QJsonObject object{{QStringLiteral("id"), item.id}, {QStringLiteral("type"), item.type},
                           {QStringLiteral("x1"), item.x1}, {QStringLiteral("y1"), item.y1},
                           {QStringLiteral("x2"), item.x2}, {QStringLiteral("y2"), item.y2}};
        if (item.type == QStringLiteral("label")) object.insert(QStringLiteral("text"), item.text);
        if (item.type == QStringLiteral("arrow")) {
            object.insert(QStringLiteral("kind"), item.kind);
            if (isCurvedKind(item.kind)) { object.insert(QStringLiteral("cx"), item.cx); object.insert(QStringLiteral("cy"), item.cy); }
        }
        // Mechanism marks: bracket kind plain/dagger, orbital kind plain/shaded.
        if (item.type == QStringLiteral("bracket") || item.type == QStringLiteral("orbital")) object.insert(QStringLiteral("kind"), item.kind);
        annotations.append(object);
    }
    // The Molfile cannot store where a double bond's second line goes, or lone
    // pairs; keep them by bond and atom index.
    QJsonArray bondSides;
    bool anySide = false;
    for (const Bond &bond : m_molecule->bondData()) {
        bondSides.append(bond.doubleSide);
        anySide = anySide || bond.doubleSide != 0;
    }
    QJsonArray lonePairs;
    for (const Atom &atom : m_molecule->atomData()) lonePairs.append(atom.lonePairs);
    QJsonObject document{{QStringLiteral("format"), QStringLiteral("reChemDrawable reaction scheme")},
                         {QStringLiteral("version"), 1},
                         {QStringLiteral("molfile"), MolfileWriter::writeV2000(*m_molecule, info.completeBaseName())},
                         {QStringLiteral("annotations"), annotations},
                         {QStringLiteral("lonePairs"), lonePairs},
                         {QStringLiteral("circledCharges"), m_molecule->circledCharges()}};
    if (anySide) document.insert(QStringLiteral("bondSides"), bondSides);
    // Partial (dashed forming/breaking) bonds have no Molfile form; the Molfile holds them
    // as plain single bonds and this list, by bond index, restores the style.
    QJsonArray partialBonds;
    const QVector<Bond> &savedBonds = m_molecule->bondData();
    for (int index = 0; index < savedBonds.size(); ++index)
        if (savedBonds[index].style == QStringLiteral("partial")) partialBonds.append(index);
    if (!partialBonds.isEmpty()) document.insert(QStringLiteral("partialBonds"), partialBonds);
    // Drawing styles a Molfile cannot hold (Haworth bold, Newman circle), by bond index.
    QJsonArray bondStyles;
    bool anyStyle = false;
    for (const Bond &bond : m_molecule->bondData()) {
        const bool special = bond.style == QStringLiteral("bold") || bond.style == QStringLiteral("newman") ||
                             bond.style == QStringLiteral("newmanback");
        bondStyles.append(special ? bond.style : QString());
        anyStyle = anyStyle || special;
    }
    if (anyStyle) document.insert(QStringLiteral("bondStyles"), bondStyles);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(document).toJson(QJsonDocument::Indented));
    emit message(QStringLiteral("Saved reaction scheme %1.").arg(path));
    return true;
}

bool ReactionLayer::loadFromFile(const QString &path) {
    QFile file(path);
    if (!m_molecule || !file.open(QIODevice::ReadOnly)) { emit message(QStringLiteral("Could not open %1.").arg(path)); return false; }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject root = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || root.value(QStringLiteral("format")).toString() != QStringLiteral("reChemDrawable reaction scheme")) {
        emit message(QStringLiteral("This is not a reChemDrawable reaction-scheme file.")); return false;
    }
    Molecule parsed;
    QString error;
    if (!MolfileReader::readV2000(root.value(QStringLiteral("molfile")).toString(), &parsed, &error)) {
        emit message(QStringLiteral("Could not load molecular structure: %1").arg(error)); return false;
    }
    QVector<Item> annotations;
    int nextId = 1;
    for (const QJsonValue &value : root.value(QStringLiteral("annotations")).toArray()) {
        const QJsonObject object = value.toObject();
        const QString type = object.value(QStringLiteral("type")).toString();
        const bool known = type == QStringLiteral("arrow") || type == QStringLiteral("label") || type == QStringLiteral("dagger") ||
                           type == QStringLiteral("bracket") || type == QStringLiteral("orbital");
        if (!known || !object.contains(QStringLiteral("x1")) || !object.contains(QStringLiteral("y1"))) continue;
        const int id = object.value(QStringLiteral("id")).toInt(nextId);
        Item item{id, type, object.value(QStringLiteral("x1")).toDouble(), object.value(QStringLiteral("y1")).toDouble(),
                  object.value(QStringLiteral("x2")).toDouble(), object.value(QStringLiteral("y2")).toDouble(), object.value(QStringLiteral("text")).toString()};
        item.kind = type == QStringLiteral("arrow") ? normalizedKind(object.value(QStringLiteral("kind")).toString()) : QStringLiteral("forward");
        if (type == QStringLiteral("bracket"))
            item.kind = object.value(QStringLiteral("kind")).toString() == QStringLiteral("dagger") ? QStringLiteral("dagger") : QStringLiteral("plain");
        if (type == QStringLiteral("orbital"))
            item.kind = object.value(QStringLiteral("kind")).toString() == QStringLiteral("shaded") ? QStringLiteral("shaded") : QStringLiteral("plain");
        item.cx = object.value(QStringLiteral("cx")).toDouble((item.x1 + item.x2) / 2.0);
        item.cy = object.value(QStringLiteral("cy")).toDouble((item.y1 + item.y2) / 2.0);
        annotations.append(item);
        nextId = qMax(nextId, id + 1);
    }
    QVector<Bond> parsedBonds = parsed.bondData();
    const QJsonArray bondSides = root.value(QStringLiteral("bondSides")).toArray();
    for (int index = 0; index < parsedBonds.size() && index < bondSides.size(); ++index)
        parsedBonds[index].doubleSide = qBound(-1, bondSides.at(index).toInt(), 1);
    for (const QJsonValue &value : root.value(QStringLiteral("partialBonds")).toArray()) {
        const int index = value.toInt(-1);
        if (index >= 0 && index < parsedBonds.size()) { parsedBonds[index].style = QStringLiteral("partial"); parsedBonds[index].order = 1; parsedBonds[index].stereo = 0; }
    }
    const QJsonArray bondStyles = root.value(QStringLiteral("bondStyles")).toArray();
    for (int index = 0; index < parsedBonds.size() && index < bondStyles.size(); ++index) {
        const QString style = bondStyles.at(index).toString();
        if (style == QStringLiteral("bold") || style == QStringLiteral("newman") || style == QStringLiteral("newmanback"))
            parsedBonds[index].style = style;
    }
    QVector<Atom> parsedAtoms = parsed.atomData();
    const QJsonArray lonePairs = root.value(QStringLiteral("lonePairs")).toArray();
    for (int index = 0; index < parsedAtoms.size() && index < lonePairs.size(); ++index)
        parsedAtoms[index].lonePairs = qBound(0, lonePairs[index].toInt(), 3);
    m_molecule->setCircledCharges(root.value(QStringLiteral("circledCharges")).toBool(false));
    m_molecule->replaceContent(parsedAtoms, parsedBonds);
    m_items = annotations;
    m_nextId = nextId;
    m_undo.clear(); m_redo.clear();
    emit historyReset();
    emit changed();
    emit message(QStringLiteral("Loaded reaction scheme %1.").arg(path));
    return true;
}

bool ReactionLayer::undo() {
    if (m_undo.isEmpty()) return false;
    m_redo.append(snapshot());
    const Snapshot previous = m_undo.takeLast();
    m_items = previous.items; m_nextId = previous.nextId;
    emit changed(); return true;
}

bool ReactionLayer::redo() {
    if (m_redo.isEmpty()) return false;
    m_undo.append(snapshot());
    const Snapshot next = m_redo.takeLast();
    m_items = next.items; m_nextId = next.nextId;
    emit changed(); return true;
}

void ReactionLayer::clear() {
    if (m_items.isEmpty()) return;
    const Snapshot before = snapshot();
    m_items.clear(); commit(before);
}

// A real copy, not a shared one: translateItem() writes through a reference
// taken before the snapshot (see Molecule::snapshot()).
ReactionLayer::Snapshot ReactionLayer::snapshot() const { return {QVector<Item>(m_items.cbegin(), m_items.cend()), m_nextId}; }
void ReactionLayer::commit(const Snapshot &before) {
    m_undo.append(before);
    if (m_undo.size() > MaxUndoSteps) m_undo.removeFirst();
    m_redo.clear();
    emit committed();
    emit changed();
}

void ReactionLayer::reset() {
    m_items.clear();
    m_nextId = 1;
    m_undo.clear();
    m_redo.clear();
    emit historyReset();
    emit changed();
}
