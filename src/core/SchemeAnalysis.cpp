#include "SchemeAnalysis.h"

#include "Abbreviations.h"
#include "GeometryEngine.h"
#include "Molecule.h"
#include "ReactionLayer.h"

#include <QHash>
#include <QLineF>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace SchemeAnalysis {
namespace {
struct Arrow {
    int id = -1;
    QString kind;
    QPointF start, end;
    QPointF mid() const { return (start + end) / 2.0; }
};

double segmentDistance(const QPointF &point, const QPointF &a, const QPointF &b) {
    const double lengthSquared = QPointF::dotProduct(b - a, b - a);
    if (lengthSquared < 0.01) return QLineF(point, a).length();
    const double fraction = qBound(0.0, QPointF::dotProduct(point - a, b - a) / lengthSquared, 1.0);
    return QLineF(point, a + (b - a) * fraction).length();
}

QVector<Arrow> straightArrows(const QVariantList &items) {
    QVector<Arrow> arrows;
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("type")).toString() != QStringLiteral("arrow")) continue;
        const QString kind = item.value(QStringLiteral("kind")).toString();
        if (ReactionLayer::isCurvedKind(kind)) continue;
        Arrow arrow;
        arrow.id = item.value(QStringLiteral("id")).toInt();
        arrow.kind = kind;
        arrow.start = QPointF(item.value(QStringLiteral("x1")).toDouble(), item.value(QStringLiteral("y1")).toDouble());
        arrow.end = QPointF(item.value(QStringLiteral("x2")).toDouble(), item.value(QStringLiteral("y2")).toDouble());
        arrows.append(arrow);
    }
    return arrows;
}

QString signedCount(int value) { return value > 0 ? QStringLiteral("+%1").arg(value) : QString::number(value); }
}

QVector<Component> components(const Molecule &molecule) {
    const QVector<Atom> &atoms = molecule.atomData();
    QHash<int, int> indexOf;
    for (int i = 0; i < atoms.size(); ++i) indexOf.insert(atoms[i].id, i);
    QVector<int> parent(atoms.size());
    for (int i = 0; i < parent.size(); ++i) parent[i] = i;
    const auto find = [&parent](int i) {
        while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; }
        return i;
    };
    for (const Bond &bond : molecule.bondData()) {
        if (!indexOf.contains(bond.atomA) || !indexOf.contains(bond.atomB)) continue;
        parent[find(indexOf.value(bond.atomA))] = find(indexOf.value(bond.atomB));
    }
    QMap<int, Component> byRoot; // ordered by root index
    for (int i = 0; i < atoms.size(); ++i) {
        Component &component = byRoot[find(i)];
        component.atomIndices.append(i);
        component.atomIds.append(atoms[i].id);
    }
    QVector<Component> result;
    for (Component component : byRoot) {
        double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
        QPointF sum;
        for (int i : component.atomIndices) {
            const Atom &atom = atoms[i];
            sum += QPointF(atom.x, atom.y);
            minX = qMin(minX, atom.x); maxX = qMax(maxX, atom.x);
            minY = qMin(minY, atom.y); maxY = qMax(maxY, atom.y);
        }
        component.centroid = sum / component.atomIndices.size();
        component.bounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
        result.append(component);
    }
    return result;
}

Composition composition(const Molecule &molecule, const QVector<int> &atomIndices) {
    Composition result;
    const QVector<Atom> &atoms = molecule.atomData();
    for (int index : atomIndices) {
        const Atom &atom = atoms[index];
        result.charge += atom.formalCharge;
        if (const Abbreviations::Info *group = Abbreviations::find(atom.element)) {
            for (auto it = group->composition.cbegin(); it != group->composition.cend(); ++it) result.elements[it.key()] += it.value();
            continue;
        }
        result.elements[atom.element] += 1;
        if (atom.element != QStringLiteral("H")) {
            const int hydrogens = molecule.implicitHydrogens(atom);
            if (hydrogens) result.elements[QStringLiteral("H")] += hydrogens;
        }
    }
    return result;
}

QString formulaText(const Composition &composition) {
    QMap<QString, int> counts = composition.elements;
    QString text;
    const auto append = [&text, &counts](const QString &element) {
        const int count = counts.take(element);
        if (count <= 0) return;
        text += element;
        if (count > 1) text += QString::number(count);
    };
    append(QStringLiteral("C"));
    append(QStringLiteral("H"));
    while (!counts.isEmpty()) append(counts.firstKey());
    if (composition.charge) {
        const int magnitude = qAbs(composition.charge);
        text += (magnitude > 1 ? QString::number(magnitude) : QString()) + (composition.charge > 0 ? QStringLiteral("+") : QStringLiteral("-"));
    }
    return text;
}

QVariantList checkReactions(const Molecule &molecule, const QVariantList &items) {
    const QVector<Arrow> arrows = straightArrows(items);
    const QVector<Component> parts = components(molecule);
    const double band = 5.0 * GeometryEngine::StandardBondLength;
    QVariantList results;
    int number = 0;
    for (const Arrow &arrow : arrows) {
        const bool reaction = arrow.kind == QStringLiteral("forward") || arrow.kind == QStringLiteral("equilibrium") ||
                              arrow.kind == QStringLiteral("noreaction") || arrow.kind == QStringLiteral("dashed");
        if (!reaction) continue;
        ++number;
        const double length = QLineF(arrow.start, arrow.end).length();
        if (length < 1e-6) continue;
        const QPointF u = (arrow.end - arrow.start) / length, n(-u.y(), u.x());
        const auto along = [&](const QPointF &p) { return QPointF::dotProduct(p - arrow.start, u); };
        const auto across = [&](const QPointF &p) { return QPointF::dotProduct(p - arrow.start, n); };
        // Other straight arrows lying in the band divide the scheme into steps.
        QVector<double> separators;
        for (const Arrow &other : arrows)
            if (other.id != arrow.id && qAbs(across(other.mid())) <= band) separators.append(along(other.mid()));

        Composition reactants, products;
        int reactantCount = 0, productCount = 0;
        QStringList reactantFormulas, productFormulas;
        for (const Component &part : parts) {
            const double t = along(part.centroid);
            if (qAbs(across(part.centroid)) > band) continue;
            const bool before = t < 0.0, after = t > length;
            if (!before && !after) continue;
            bool separated = false;
            for (double s : separators)
                if (before ? (s > t && s < 0.0) : (s > length && s < t)) { separated = true; break; }
            if (separated) continue;
            const Composition c = composition(molecule, part.atomIndices);
            Composition &side = before ? reactants : products;
            for (auto it = c.elements.cbegin(); it != c.elements.cend(); ++it) side.elements[it.key()] += it.value();
            side.charge += c.charge;
            (before ? reactantCount : productCount) += 1;
            (before ? reactantFormulas : productFormulas).append(formulaText(c));
        }

        QVariantMap result{{QStringLiteral("arrow"), arrow.id}, {QStringLiteral("kind"), arrow.kind},
                           {QStringLiteral("reactants"), reactantCount}, {QStringLiteral("products"), productCount},
                           {QStringLiteral("chargeReactants"), reactants.charge}, {QStringLiteral("chargeProducts"), products.charge}};
        const QString label = QStringLiteral("Arrow %1").arg(number);
        if (reactantCount == 0 || productCount == 0) {
            result.insert(QStringLiteral("balanced"), false);
            result.insert(QStringLiteral("incomplete"), true);
            result.insert(QStringLiteral("differences"), QVariantList());
            result.insert(QStringLiteral("text"), QStringLiteral("%1: no %2 found to check.").arg(label, reactantCount == 0 ? QStringLiteral("reactants") : QStringLiteral("products")));
            results.append(result);
            continue;
        }
        QVariantList differences;
        QStringList notes;
        QSet<QString> names;
        for (auto it = reactants.elements.cbegin(); it != reactants.elements.cend(); ++it) names.insert(it.key());
        for (auto it = products.elements.cbegin(); it != products.elements.cend(); ++it) names.insert(it.key());
        QStringList ordered(names.begin(), names.end());
        std::sort(ordered.begin(), ordered.end(), [](const QString &a, const QString &b) {
            const auto rank = [](const QString &e) { return e == QStringLiteral("C") ? 0 : e == QStringLiteral("H") ? 1 : 2; };
            return rank(a) != rank(b) ? rank(a) < rank(b) : a < b;
        });
        for (const QString &element : ordered) {
            const int left = reactants.elements.value(element), right = products.elements.value(element);
            if (left == right) continue;
            differences.append(QVariantMap{{QStringLiteral("element"), element}, {QStringLiteral("reactants"), left}, {QStringLiteral("products"), right}});
            notes.append(QStringLiteral("%1 %2 -> %3 (%4)").arg(element).arg(left).arg(right).arg(signedCount(right - left)));
        }
        const bool chargeOk = reactants.charge == products.charge;
        if (!chargeOk) notes.append(QStringLiteral("charge %1 -> %2").arg(signedCount(reactants.charge)).arg(signedCount(products.charge)));
        const bool balanced = differences.isEmpty() && chargeOk;
        const QString equation = reactantFormulas.join(QStringLiteral(" + ")) + QStringLiteral(" -> ") + productFormulas.join(QStringLiteral(" + "));
        result.insert(QStringLiteral("balanced"), balanced);
        result.insert(QStringLiteral("incomplete"), false);
        result.insert(QStringLiteral("differences"), differences);
        result.insert(QStringLiteral("text"), balanced ? QStringLiteral("%1: Balanced (%2)").arg(label, equation)
                                                       : QStringLiteral("%1: Not balanced (%2): %3").arg(label, equation, notes.join(QStringLiteral("; "))));
        results.append(result);
    }
    return results;
}

Tidy tidyPlan(const Molecule &molecule, const QVariantList &items) {
    struct Element {
        enum Kind { MoleculeKind, ArrowKind, PlusKind } kind = MoleculeKind;
        int ref = -1;          // component index, or item id
        double width = 0.0;
        double centerX = 0.0;  // current position along x, for ordering
        double left = 0.0;
        QVector<int> attached; // label item ids (arrows)
    };
    struct Label { int id = -1; QString text; QPointF pos; };
    constexpr double moleculePad = 14.0, gap = 10.0, plusWidth = 14.0, epsilon = 1e-6;
    const double bondLength = GeometryEngine::StandardBondLength;

    const QVector<Component> parts = components(molecule);
    const QVector<Arrow> arrows = straightArrows(items);
    QVector<Label> labels;
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("type")).toString() != QStringLiteral("label")) continue;
        Label label;
        label.id = item.value(QStringLiteral("id")).toInt();
        label.text = item.value(QStringLiteral("text")).toString().trimmed();
        label.pos = QPointF(item.value(QStringLiteral("x1")).toDouble(), item.value(QStringLiteral("y1")).toDouble());
        labels.append(label);
    }

    QVector<Element> elements;
    for (int i = 0; i < parts.size(); ++i) {
        Element e;
        e.kind = Element::MoleculeKind; e.ref = i;
        e.width = parts[i].bounds.width() + 2 * moleculePad;
        e.centerX = parts[i].bounds.center().x();
        e.left = parts[i].bounds.left() - moleculePad;
        elements.append(e);
    }
    // Labels within 40 px of a straight arrow follow it; a lone "+" is a sequence element of its own.
    QHash<int, int> arrowElement;
    for (const Arrow &arrow : arrows) {
        Element e;
        e.kind = Element::ArrowKind; e.ref = arrow.id;
        e.centerX = arrow.mid().x();
        e.left = qMin(arrow.start.x(), arrow.end.x());
        arrowElement.insert(arrow.id, elements.size());
        elements.append(e);
    }
    for (const Label &label : labels) {
        const Arrow *best = nullptr;
        double bestDistance = 40.0;
        for (const Arrow &arrow : arrows) {
            const double d = segmentDistance(label.pos, arrow.start, arrow.end);
            if (d <= bestDistance) { best = &arrow; bestDistance = d; }
        }
        if (best) {
            elements[arrowElement.value(best->id)].attached.append(label.id);
        } else if (label.text == QStringLiteral("+")) {
            Element e;
            e.kind = Element::PlusKind; e.ref = label.id;
            e.width = plusWidth; e.centerX = label.pos.x(); e.left = label.pos.x() - plusWidth / 2;
            elements.append(e);
        }
    }
    for (Element &e : elements) {
        if (e.kind != Element::ArrowKind) continue;
        double widest = 0.0;
        for (int labelId : e.attached)
            for (const Label &label : labels) if (label.id == labelId) widest = qMax(widest, label.text.size() * 10.0);
        e.width = qMax(3.0 * bondLength, widest + 16.0);
    }
    Tidy plan;
    if (elements.isEmpty()) return plan;

    // Baseline: mean vertical centre of the molecules, or of the arrows if there are none.
    double baseline = 0.0;
    if (!parts.isEmpty()) {
        for (const Component &part : parts) baseline += part.bounds.center().y();
        baseline /= parts.size();
    } else {
        for (const Arrow &arrow : arrows) baseline += arrow.mid().y();
        baseline /= qMax(1, int(arrows.size()));
    }
    double cursor = 1e18;
    for (const Element &e : elements) cursor = qMin(cursor, e.left);
    std::stable_sort(elements.begin(), elements.end(), [](const Element &a, const Element &b) { return a.centerX < b.centerX; });

    const auto labelById = [&labels](int id) { for (const Label &l : labels) if (l.id == id) return l; return Label(); };
    const auto arrowById = [&arrows](int id) { for (const Arrow &a : arrows) if (a.id == id) return a; return Arrow(); };
    for (const Element &e : elements) {
        const double newCenter = cursor + e.width / 2.0;
        if (e.kind == Element::MoleculeKind) {
            const Component &part = parts[e.ref];
            const double dx = newCenter - part.bounds.center().x(), dy = baseline - part.bounds.center().y();
            if (qAbs(dx) > epsilon || qAbs(dy) > epsilon) plan.atoms.append({part.atomIds, dx, dy});
        } else if (e.kind == Element::PlusKind) {
            const Label label = labelById(e.ref);
            const double dx = newCenter - label.pos.x(), dy = baseline - label.pos.y();
            if (qAbs(dx) > epsilon || qAbs(dy) > epsilon) plan.items.append({label.id, dx, dy});
        } else {
            const Arrow arrow = arrowById(e.ref);
            const bool leftToRight = arrow.end.x() >= arrow.start.x();
            const double tail = leftToRight ? cursor : cursor + e.width, head = leftToRight ? cursor + e.width : cursor;
            if (qAbs(arrow.start.x() - tail) > epsilon || qAbs(arrow.start.y() - baseline) > epsilon ||
                qAbs(arrow.end.x() - head) > epsilon || qAbs(arrow.end.y() - baseline) > epsilon)
                plan.arrows.append({arrow.id, tail, baseline, head, baseline});
            const QPointF delta = QPointF(newCenter, baseline) - arrow.mid();
            if (qAbs(delta.x()) > epsilon || qAbs(delta.y()) > epsilon)
                for (int labelId : e.attached) plan.items.append({labelId, delta.x(), delta.y()});
        }
        cursor += e.width + gap;
    }
    return plan;
}

} // namespace SchemeAnalysis
