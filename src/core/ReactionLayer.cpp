#include "ReactionLayer.h"

#include "Molecule.h"
#include "MolfileReader.h"
#include "MolfileWriter.h"
#include "SvgWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QStandardPaths>

namespace {
double segmentDistance(const QPointF &point, const QPointF &start, const QPointF &end) {
    const QLineF line(start, end);
    const double lengthSquared = line.length() * line.length();
    if (lengthSquared < 0.01) return QLineF(point, start).length();
    const double fraction = qBound(0.0, QPointF::dotProduct(point - start, end - start) / lengthSquared, 1.0);
    return QLineF(point, start + (end - start) * fraction).length();
}
}

ReactionLayer::ReactionLayer(Molecule *molecule, QObject *parent) : QObject(parent), m_molecule(molecule) {}

QVariantList ReactionLayer::items() const {
    QVariantList result;
    for (const Item &item : m_items) {
        result.append(QVariantMap{{QStringLiteral("id"), item.id}, {QStringLiteral("type"), item.type},
                                  {QStringLiteral("x1"), item.x1}, {QStringLiteral("y1"), item.y1},
                                  {QStringLiteral("x2"), item.x2}, {QStringLiteral("y2"), item.y2},
                                  {QStringLiteral("text"), item.text}});
    }
    return result;
}

int ReactionLayer::addArrow(double startX, double startY, double endX, double endY) {
    if (QLineF(QPointF(startX, startY), QPointF(endX, endY)).length() < 12.0) return -1;
    const Snapshot before = snapshot();
    const int id = m_nextId++;
    m_items.append({id, QStringLiteral("arrow"), startX, startY, endX, endY, {}});
    commit(before);
    return id;
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

int ReactionLayer::itemAt(double x, double y, double tolerance) const {
    const QPointF point(x, y);
    for (auto iterator = m_items.crbegin(); iterator != m_items.crend(); ++iterator) {
        if (iterator->type == QStringLiteral("arrow") && segmentDistance(point, QPointF(iterator->x1, iterator->y1), QPointF(iterator->x2, iterator->y2)) <= tolerance)
            return iterator->id;
        if (iterator->type == QStringLiteral("label")) {
            const double halfWidth = qMax(24.0, iterator->text.size() * 5.5);
            if (qAbs(x - iterator->x1) <= halfWidth && qAbs(y - iterator->y1) <= tolerance) return iterator->id;
        }
    }
    return -1;
}

bool ReactionLayer::translateItem(int itemId, double dx, double dy) {
    for (Item &item : m_items) {
        if (item.id != itemId) continue;
        const Snapshot before = snapshot();
        item.x1 += dx; item.y1 += dy; item.x2 += dx; item.y2 += dy;
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
        annotations.append(object);
    }
    const QJsonObject document{{QStringLiteral("format"), QStringLiteral("reChemDrawable reaction scheme")},
                               {QStringLiteral("version"), 1},
                               {QStringLiteral("molfile"), MolfileWriter::writeV2000(*m_molecule, info.completeBaseName())},
                               {QStringLiteral("annotations"), annotations}};
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
        if ((type != QStringLiteral("arrow") && type != QStringLiteral("label")) || !object.contains(QStringLiteral("x1")) || !object.contains(QStringLiteral("y1"))) continue;
        const int id = object.value(QStringLiteral("id")).toInt(nextId);
        annotations.append({id, type, object.value(QStringLiteral("x1")).toDouble(), object.value(QStringLiteral("y1")).toDouble(),
                            object.value(QStringLiteral("x2")).toDouble(), object.value(QStringLiteral("y2")).toDouble(), object.value(QStringLiteral("text")).toString()});
        nextId = qMax(nextId, id + 1);
    }
    m_molecule->replaceContent(parsed.atomData(), parsed.bondData());
    m_items = annotations;
    m_nextId = nextId;
    m_undo.clear(); m_redo.clear();
    emit historyReset();
    emit changed();
    emit message(QStringLiteral("Loaded reaction scheme %1.").arg(path));
    return true;
}

bool ReactionLayer::saveDefaultScheme() { return saveToFile(defaultSchemePath()); }
bool ReactionLayer::loadDefaultScheme() { return loadFromFile(defaultSchemePath()); }
bool ReactionLayer::saveDefaultSvg() {
    if (!m_molecule) return false;
    const QString path = QFileInfo(defaultSchemePath()).dir().filePath(QStringLiteral("reaction-scheme.svg"));
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    file.write(SvgWriter::write(*m_molecule, items()).toUtf8());
    emit message(QStringLiteral("Saved reaction-scheme SVG %1.").arg(path));
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

ReactionLayer::Snapshot ReactionLayer::snapshot() const { return {m_items, m_nextId}; }
void ReactionLayer::commit(const Snapshot &before) { m_undo.append(before); m_redo.clear(); emit committed(); emit changed(); }

QString ReactionLayer::defaultSchemePath() const {
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) documents = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(documents).filePath(QStringLiteral("reChemDrawable/reaction-scheme.rcd"));
}
