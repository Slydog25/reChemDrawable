#include "SceneRenderer.h"

#include "Molecule.h"
#include "ReactionLayer.h"

#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QtMath>

namespace {
const QColor kInk(0x11, 0x11, 0x11);
constexpr double kStroke = 3.0;

void line(QPainter &p, double x1, double y1, double x2, double y2) { p.drawLine(QPointF(x1, y1), QPointF(x2, y2)); }

// Chevron with its tip at (tx, ty) travelling along unit (ux, uy); side 0 = both
// barbs, +1 / -1 only that barb (same as SvgWriter::writeHead).
void head(QPainter &p, double tx, double ty, double ux, double uy, int side, double size = 13.0, double spread = 7.0) {
    const double nx = -uy, ny = ux;
    QPolygonF poly;
    if (side >= 0) poly << QPointF(tx - ux * size + nx * spread, ty - uy * size + ny * spread);
    poly << QPointF(tx, ty);
    if (side <= 0) poly << QPointF(tx - ux * size - nx * spread, ty - uy * size - ny * spread);
    p.save();
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(poly);
    p.restore();
}

void arrow(QPainter &p, const QVariantMap &item) {
    const double x1 = item.value(QStringLiteral("x1")).toDouble(), y1 = item.value(QStringLiteral("y1")).toDouble();
    const double x2 = item.value(QStringLiteral("x2")).toDouble(), y2 = item.value(QStringLiteral("y2")).toDouble();
    const double dx = x2 - x1, dy = y2 - y1, length = qSqrt(dx * dx + dy * dy);
    if (length < 0.01) return;
    const QString kind = ReactionLayer::normalizedKind(item.value(QStringLiteral("kind")).toString());
    const double ux = dx / length, uy = dy / length, nx = -uy, ny = ux;
    if (kind == QStringLiteral("curved") || kind == QStringLiteral("fishhook")) {
        const double cx = item.value(QStringLiteral("cx"), (x1 + x2) / 2).toDouble();
        const double cy = item.value(QStringLiteral("cy"), (y1 + y2) / 2).toDouble();
        double tx = x2 - cx, ty = y2 - cy;
        const double tl = qSqrt(tx * tx + ty * ty);
        if (tl < 0.01) { tx = ux; ty = uy; } else { tx /= tl; ty /= tl; }
        QPainterPath path(QPointF(x1, y1));
        path.quadTo(QPointF(cx, cy), QPointF(x2, y2));
        p.save(); p.setBrush(Qt::NoBrush); p.drawPath(path); p.restore();
        head(p, x2, y2, tx, ty, kind == QStringLiteral("fishhook") ? 1 : 0);
    } else if (kind == QStringLiteral("equilibrium")) {
        line(p, x1 - nx * 4, y1 - ny * 4, x2 - nx * 4, y2 - ny * 4);
        head(p, x2 - nx * 4, y2 - ny * 4, ux, uy, -1);
        line(p, x2 + nx * 4, y2 + ny * 4, x1 + nx * 4, y1 + ny * 4);
        head(p, x1 + nx * 4, y1 + ny * 4, -ux, -uy, -1);
    } else if (kind == QStringLiteral("retro")) {
        const double back = qMin(length * 0.5, 14.0);
        line(p, x1 + nx * 4, y1 + ny * 4, x2 - ux * back + nx * 4, y2 - uy * back + ny * 4);
        line(p, x1 - nx * 4, y1 - ny * 4, x2 - ux * back - nx * 4, y2 - uy * back - ny * 4);
        head(p, x2, y2, ux, uy, 0, 16, 11);
    } else if (kind == QStringLiteral("resonance")) {
        line(p, x1, y1, x2, y2);
        head(p, x2, y2, ux, uy, 0);
        head(p, x1, y1, -ux, -uy, 0);
    } else {
        p.save();
        QPen pen = p.pen();
        if (kind == QStringLiteral("dashed")) { pen.setDashPattern({9.0 / kStroke, 6.0 / kStroke}); pen.setCapStyle(Qt::FlatCap); }
        p.setPen(pen);
        line(p, x1, y1, x2, y2);
        p.restore();
        head(p, x2, y2, ux, uy, 0);
        if (kind == QStringLiteral("noreaction")) {
            const double mx = (x1 + x2) / 2, my = (y1 + y2) / 2;
            line(p, mx - (ux + nx) * 9, my - (uy + ny) * 9, mx + (ux + nx) * 9, my + (uy + ny) * 9);
            line(p, mx - (ux - nx) * 9, my - (uy - ny) * 9, mx + (ux - nx) * 9, my + (uy - ny) * 9);
        }
    }
}

QFont sceneFont(bool bold = false, double pixels = 20.0) {
    QFont font(QStringLiteral("sans-serif"));
    font.setPixelSize(qRound(pixels));
    font.setBold(bold);
    return font;
}

// Text whose vertical centre is cy; anchor 0 centred on x, -1 ends at x, +1 starts at x.
// Drawn as a path so PDF output is vector and needs no embedded font handling;
// `halo` strokes a white outline under the glyphs like the SVG's paint-order stroke.
void text(QPainter &p, const QString &s, double x, double cy, int anchor, const QFont &font, double halo) {
    if (s.isEmpty()) return;
    const QFontMetricsF fm(font);
    const double w = fm.horizontalAdvance(s);
    const double left = anchor == 0 ? x - w / 2 : anchor < 0 ? x - w : x;
    const double baseline = cy + (fm.ascent() - fm.descent()) / 2;
    QPainterPath path;
    path.addText(QPointF(left, baseline), font, s);
    p.save();
    p.setBrush(Qt::NoBrush);
    if (halo > 0) {
        p.setPen(QPen(Qt::white, halo, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(kInk);
    p.drawPath(path);
    p.restore();
}

void dot(QPainter &p, double cx, double cy, double r) {
    p.save(); p.setPen(Qt::NoPen); p.setBrush(kInk); p.drawEllipse(QPointF(cx, cy), r, r); p.restore();
}
}

QRectF SceneRenderer::contentBounds(const Molecule &molecule, const QVariantList &reactionItems) {
    bool any = false;
    QRectF box;
    auto include = [&](const QRectF &r) { box = any ? box.united(r) : r; any = true; };
    for (const Atom &atom : molecule.atomData()) {
        const Molecule::AtomDisplay display = molecule.atomDisplay(atom);
        double left = atom.x, right = atom.x;
        if (display.showLabel) {
            left = atom.x - 13 - display.centerIndex * 10.0;
            right = atom.x + 13 + (display.text.size() - display.centerIndex - display.centerLength) * 10.0;
        }
        if (display.charge) right += 22;
        include(QRectF(QPointF(left, atom.y - (display.showLabel || display.charge ? 14 : 0)),
                       QPointF(right, atom.y + (display.showLabel ? 14 : 0))));
    }
    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        const double x1 = item.value(QStringLiteral("x1")).toDouble(), y1 = item.value(QStringLiteral("y1")).toDouble();
        if (item.value(QStringLiteral("type")).toString() == QStringLiteral("label")) {
            const QString t = item.value(QStringLiteral("text")).toString();
            const double w = t.size() * 12.0 + 4;
            include(QRectF(x1 - w / 2, y1 - 13, w, 26));
            continue;
        }
        const double x2 = item.value(QStringLiteral("x2")).toDouble(), y2 = item.value(QStringLiteral("y2")).toDouble();
        include(QRectF(QPointF(qMin(x1, x2), qMin(y1, y2)), QPointF(qMax(x1, x2), qMax(y1, y2))).adjusted(-8, -8, 8, 8));
        if (item.contains(QStringLiteral("cx")) && ReactionLayer::isCurvedKind(item.value(QStringLiteral("kind")).toString())) {
            const double cx = item.value(QStringLiteral("cx")).toDouble(), cy = item.value(QStringLiteral("cy")).toDouble();
            const double ax = (x1 + 2 * cx + x2) / 4, ay = (y1 + 2 * cy + y2) / 4;
            include(QRectF(ax - 8, ay - 8, 16, 16));
        }
    }
    return any ? box : QRectF();
}

void SceneRenderer::paint(QPainter &p, const Molecule &molecule, const QVariantList &reactionItems) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(QPen(kInk, kStroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(kInk);
    QHash<int, Atom> byId;
    for (const Atom &atom : molecule.atomData()) byId.insert(atom.id, atom);

    for (const Bond &bond : molecule.bondData()) {
        if (!byId.contains(bond.atomA) || !byId.contains(bond.atomB)) continue;
        const Atom a = byId.value(bond.atomA), b = byId.value(bond.atomB);
        const double dx = b.x - a.x, dy = b.y - a.y, len = qSqrt(dx * dx + dy * dy);
        const double nx = len ? -dy / len : 0, ny = len ? dx / len : 0;
        if (bond.style == QStringLiteral("wedge")) {
            p.drawPolygon(QPolygonF({QPointF(a.x, a.y), QPointF(b.x + nx * 8, b.y + ny * 8), QPointF(b.x - nx * 8, b.y - ny * 8)}));
            continue;
        }
        if (bond.style == QStringLiteral("dash")) {
            for (int d = 1; d < 7; ++d) {
                const double t = d / 7.0, w = 1 + t * 8, x = a.x + dx * t, y = a.y + dy * t;
                line(p, x + nx * w, y + ny * w, x - nx * w, y - ny * w);
            }
            continue;
        }
        if (bond.style == QStringLiteral("wavy")) {
            QPainterPath path(QPointF(a.x, a.y));
            for (int w = 1; w <= 6; ++w) {
                const double t = w / 6.0, sign = w % 2 == 0 ? -1.0 : 1.0, px = a.x + dx * t, py = a.y + dy * t;
                path.quadTo(QPointF(px + nx * sign * 7, py + ny * sign * 7), QPointF(px, py));
            }
            p.save(); p.setBrush(Qt::NoBrush); p.drawPath(path); p.restore();
            continue;
        }
        if (bond.style == QStringLiteral("benzene")) {
            line(p, a.x, a.y, b.x, b.y);
            if (bond.order == 2 && len > 0.0)
                line(p, a.x + dx * 0.18 + nx * 7, a.y + dy * 0.18 + ny * 7, b.x - dx * 0.18 + nx * 7, b.y - dy * 0.18 + ny * 7);
            continue;
        }
        if (bond.order == 2 && bond.doubleSide != 0 && bond.style == QStringLiteral("plain") && len > 0.0) {
            const double inset = bond.doubleSide > 0 ? 7.0 : -7.0;
            line(p, a.x, a.y, b.x, b.y);
            line(p, a.x + dx * 0.12 + nx * inset, a.y + dy * 0.12 + ny * inset, b.x - dx * 0.12 + nx * inset, b.y - dy * 0.12 + ny * inset);
            continue;
        }
        p.save();
        if (bond.style == QStringLiteral("aromatic")) {
            QPen pen = p.pen();
            pen.setDashPattern({7.0 / kStroke, 5.0 / kStroke});
            pen.setCapStyle(Qt::FlatCap);
            p.setPen(pen);
        }
        const QVector<double> offsets = bond.order == 1 ? QVector<double>{0} : bond.order == 2 ? QVector<double>{-4, 4} : QVector<double>{-6, 0, 6};
        for (double o : offsets) line(p, a.x + nx * o, a.y + ny * o, b.x + nx * o, b.y + ny * o);
        p.restore();
    }
    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("type")).toString() == QStringLiteral("arrow")) arrow(p, item);
    }

    const QFont font = sceneFont();
    for (const Atom &atom : molecule.atomData()) {
        const Molecule::AtomDisplay display = molecule.atomDisplay(atom);
        constexpr double halfChar = 6.5;
        double labelRight = atom.x;
        if (display.showLabel) {
            const double centerWidth = display.centerLength * 2 * halfChar;
            const QString before = display.text.left(display.centerIndex);
            const QString center = display.text.mid(display.centerIndex, display.centerLength);
            const QString after = display.text.mid(display.centerIndex + display.centerLength);
            text(p, before, atom.x - centerWidth / 2, atom.y, -1, font, 7);
            text(p, center, atom.x, atom.y, 0, font, 7);
            text(p, after, atom.x + centerWidth / 2, atom.y, 1, font, 7);
            labelRight = atom.x + centerWidth / 2 + after.size() * 10.0;
        }
        if (display.charge) {
            const double baseX = display.showLabel ? labelRight + 2 : atom.x + 7;
            if (molecule.circledCharges()) {
                const double cx = baseX + 5, cy = atom.y - 11;
                p.save();
                p.setPen(QPen(kInk, 1.2));
                p.setBrush(Qt::white);
                p.drawEllipse(QPointF(cx, cy), 5, 5);
                line(p, cx - 3, cy, cx + 3, cy);
                if (display.charge > 0) line(p, cx, cy - 3, cx, cy + 3);
                p.restore();
            } else {
                text(p, display.chargeLabel, baseX, atom.y - 8, 1, sceneFont(false, 14), 4);
            }
        }
        const double ring = display.showLabel ? 16.0 : 10.0;
        for (double angle : display.lonePairAngles) {
            const double r = qDegreesToRadians(angle), cx = atom.x + ring * qCos(r), cy = atom.y + ring * qSin(r);
            const double px = -qSin(r) * 3.0, py = qCos(r) * 3.0;
            dot(p, cx + px, cy + py, 1.8); dot(p, cx - px, cy - py, 1.8);
        }
        for (double angle : display.radicalAngles) {
            const double r = qDegreesToRadians(angle);
            dot(p, atom.x + ring * qCos(r), atom.y + ring * qSin(r), 2.2);
        }
    }
    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("type")).toString() != QStringLiteral("label")) continue;
        const QString t = item.value(QStringLiteral("text")).toString();
        const bool bold = item.contains(QStringLiteral("bold")) ? item.value(QStringLiteral("bold")).toBool() : ReactionLayer::isCompoundNumber(t);
        text(p, t, item.value(QStringLiteral("x1")).toDouble(), item.value(QStringLiteral("y1")).toDouble(), 0, sceneFont(bold), 0);
    }
    p.restore();
}

QImage SceneRenderer::renderImage(const Molecule &molecule, const QVariantList &reactionItems, double scale, double margin) {
    const QRectF box = contentBounds(molecule, reactionItems);
    if (box.isNull()) return QImage();
    const QRectF area = box.adjusted(-margin, -margin, margin, margin);
    QImage image(qCeil(area.width() * scale), qCeil(area.height() * scale), QImage::Format_ARGB32);
    image.fill(Qt::white);
    // Model units are CSS pixels (96 per inch), so scale 3.125 is 300 dpi.
    image.setDotsPerMeterX(qRound(scale * 96.0 / 0.0254));
    image.setDotsPerMeterY(qRound(scale * 96.0 / 0.0254));
    QPainter painter(&image);
    painter.scale(scale, scale);
    painter.translate(-area.topLeft());
    paint(painter, molecule, reactionItems);
    return image;
}

bool SceneRenderer::writePdf(const QString &path, const Molecule &molecule, const QVariantList &reactionItems, const QString &title) {
    const QRectF box = contentBounds(molecule, reactionItems);
    if (box.isNull()) return false;
    const QRectF area = box.adjusted(-ExportMargin, -ExportMargin, ExportMargin, ExportMargin);
    QPdfWriter pdf(path);
    pdf.setResolution(72); // device units are points
    pdf.setTitle(title);
    pdf.setCreator(QStringLiteral("reChemDrawable"));
    const QSizeF pageSize(area.width() * PdfPointsPerUnit, area.height() * PdfPointsPerUnit);
    pdf.setPageSize(QPageSize(pageSize, QPageSize::Point));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&pdf);
    if (!painter.isActive()) return false;
    painter.scale(PdfPointsPerUnit, PdfPointsPerUnit);
    painter.translate(-area.topLeft());
    paint(painter, molecule, reactionItems);
    return true;
}
