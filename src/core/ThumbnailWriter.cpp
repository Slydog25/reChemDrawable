#include "ThumbnailWriter.h"

#include "Molecule.h"

#include <QHash>
#include <QPainter>
#include <QPolygonF>
#include <QtMath>

QImage ThumbnailWriter::render(const Molecule &molecule, const QVariantList &reactionItems, const QSize &size) {
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::white);

    const QVector<Atom> &atoms = molecule.atomData();
    QHash<int, QPointF> position;
    bool any = false;
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    auto include = [&](double x, double y) {
        if (!any) { minX = maxX = x; minY = maxY = y; any = true; return; }
        minX = qMin(minX, x); maxX = qMax(maxX, x);
        minY = qMin(minY, y); maxY = qMax(maxY, y);
    };
    for (const Atom &atom : atoms) {
        position.insert(atom.id, QPointF(atom.x, atom.y));
        include(atom.x, atom.y);
    }
    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        include(item.value(QStringLiteral("x1")).toDouble(), item.value(QStringLiteral("y1")).toDouble());
        include(item.value(QStringLiteral("x2")).toDouble(), item.value(QStringLiteral("y2")).toDouble());
    }
    if (!any) return image;

    const double margin = 14.0;
    const double spanX = qMax(maxX - minX, 1.0), spanY = qMax(maxY - minY, 1.0);
    double scale = qMin((size.width() - 2 * margin) / spanX, (size.height() - 2 * margin) / spanY);
    scale = qMin(scale, 0.8); // do not blow a lone atom up
    const QPointF centre((minX + maxX) / 2, (minY + maxY) / 2);
    auto map = [&](QPointF p) {
        return QPointF(size.width() / 2.0 + (p.x() - centre.x()) * scale, size.height() / 2.0 + (p.y() - centre.y()) * scale);
    };

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor ink(17, 17, 17);
    painter.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(ink);

    for (const Bond &bond : molecule.bondData()) {
        if (!position.contains(bond.atomA) || !position.contains(bond.atomB)) continue;
        const QPointF a = map(position.value(bond.atomA)), b = map(position.value(bond.atomB));
        const QLineF line(a, b);
        const double length = qMax(line.length(), 0.001);
        const QPointF unit(-line.dy() / length, line.dx() / length);
        if (bond.style == QStringLiteral("wedge")) {
            painter.drawPolygon(QPolygonF({a, b + unit * 3.5, b - unit * 3.5}));
        } else if (bond.style == QStringLiteral("dash")) {
            for (int i = 1; i < 6; ++i) {
                const double t = i / 6.0, w = 0.5 + t * 3.5;
                const QPointF p = a + (b - a) * t;
                painter.drawLine(p + unit * w, p - unit * w);
            }
        } else if (bond.style == QStringLiteral("newman")) {
            const double radius = molecule.newmanCircleRadius() * scale;
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse((a + b) / 2.0, radius, radius);
            painter.setBrush(ink);
        } else if (bond.style == QStringLiteral("newmanback")) {
            const double inset = qMin(molecule.newmanCircleRadius() * scale, length) / length;
            painter.drawLine(a + (b - a) * inset, b);
        } else if (bond.style == QStringLiteral("bold")) {
            painter.setPen(QPen(ink, 3.2, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(a, b);
            painter.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap));
        } else if (bond.order == 2) {
            painter.drawLine(a + unit * 2, b + unit * 2);
            painter.drawLine(a - unit * 2, b - unit * 2);
        } else if (bond.order >= 3) {
            painter.drawLine(a, b);
            painter.drawLine(a + unit * 3.2, b + unit * 3.2);
            painter.drawLine(a - unit * 3.2, b - unit * 3.2);
        } else {
            painter.drawLine(a, b);
            if (bond.style == QStringLiteral("aromatic") || bond.style == QStringLiteral("benzene"))
                painter.drawLine(a + unit * 3, b + unit * 3);
        }
    }

    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);
    const QFontMetricsF metrics(font);
    auto drawText = [&](const QString &text, QPointF centrePoint, bool whiteBackground) {
        const QRectF box = metrics.boundingRect(text);
        const QRectF target(centrePoint.x() - box.width() / 2, centrePoint.y() - metrics.height() / 2, box.width(), metrics.height());
        if (whiteBackground) painter.fillRect(target.adjusted(-2, 0, 2, 0), Qt::white);
        painter.setPen(ink);
        painter.drawText(target, Qt::AlignCenter, text);
    };
    // Same labels as the canvas (OH, H₂N, OMe, charges), so previews match the drawing.
    for (const Atom &atom : atoms) {
        const Molecule::AtomDisplay display = molecule.atomDisplay(atom);
        if (!display.showLabel && !display.charge) continue;
        drawText((display.showLabel ? display.text : QString()) + display.chargeLabel, map(QPointF(atom.x, atom.y)), true);
    }

    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        const QPointF p1(item.value(QStringLiteral("x1")).toDouble(), item.value(QStringLiteral("y1")).toDouble());
        const QPointF p2(item.value(QStringLiteral("x2")).toDouble(), item.value(QStringLiteral("y2")).toDouble());
        if (item.value(QStringLiteral("type")).toString() == QStringLiteral("label")) {
            drawText(item.value(QStringLiteral("text")).toString(), map(p1), false);
            continue;
        }
        // Mechanism marks (dagger, bracket, orbital) are too small to matter in a thumbnail.
        if (item.value(QStringLiteral("type")).toString() != QStringLiteral("arrow")) continue;
        painter.setPen(QPen(ink, 1.8, Qt::SolidLine, Qt::RoundCap));
        const QPointF a = map(p1), b = map(p2);
        painter.drawLine(a, b);
        const QLineF back(b, a);
        if (back.length() > 0.5) {
            const QLineF left = QLineF::fromPolar(7, back.angle() + 28), right = QLineF::fromPolar(7, back.angle() - 28);
            painter.drawLine(b, b + QPointF(left.dx(), left.dy()));
            painter.drawLine(b, b + QPointF(right.dx(), right.dy()));
        }
    }
    return image;
}
