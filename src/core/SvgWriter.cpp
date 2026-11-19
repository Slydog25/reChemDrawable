#include "SvgWriter.h"

#include "Molecule.h"
#include "ReactionLayer.h"

#include <QHash>
#include <QTextStream>

namespace {
// Arrowhead chevron with its tip at (tx, ty) travelling along unit (ux, uy). side 0
// draws both barbs, +1 / -1 only the barb on that side of the shaft (half head).
void writeHead(QTextStream &stream, double tx, double ty, double ux, double uy, int side, double size = 13.0, double spread = 7.0) {
    const double nx = -uy, ny = ux;
    stream << "<polyline fill=\"none\" points=\"";
    if (side >= 0) stream << tx - ux * size + nx * spread << ',' << ty - uy * size + ny * spread << ' ';
    stream << tx << ',' << ty;
    if (side <= 0) stream << ' ' << tx - ux * size - nx * spread << ',' << ty - uy * size - ny * spread;
    stream << "\"/>\n";
}

void writeLine(QTextStream &stream, double x1, double y1, double x2, double y2, const char *extra = "") {
    stream << "<line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2 << "\" y2=\"" << y2 << "\"" << extra << "/>\n";
}

void writeArrow(QTextStream &stream, const QVariantMap &item) {
    const double x1 = item.value(QStringLiteral("x1")).toDouble(), y1 = item.value(QStringLiteral("y1")).toDouble();
    const double x2 = item.value(QStringLiteral("x2")).toDouble(), y2 = item.value(QStringLiteral("y2")).toDouble();
    const double dx = x2 - x1, dy = y2 - y1, length = qSqrt(dx * dx + dy * dy);
    if (length < 0.01) return;
    const QString kind = ReactionLayer::normalizedKind(item.value(QStringLiteral("kind")).toString());
    const double ux = dx / length, uy = dy / length, nx = -uy, ny = ux;
    stream << "<g class=\"arrow-" << kind << "\">\n";
    if (kind == QStringLiteral("curved") || kind == QStringLiteral("fishhook")) {
        const double cx = item.value(QStringLiteral("cx"), (x1 + x2) / 2).toDouble(), cy = item.value(QStringLiteral("cy"), (y1 + y2) / 2).toDouble();
        double tx = x2 - cx, ty = y2 - cy;
        const double tl = qSqrt(tx * tx + ty * ty);
        if (tl < 0.01) { tx = ux; ty = uy; } else { tx /= tl; ty /= tl; }
        stream << "<path fill=\"none\" d=\"M" << x1 << ',' << y1 << " Q" << cx << ',' << cy << ' ' << x2 << ',' << y2 << "\"/>\n";
        writeHead(stream, x2, y2, tx, ty, kind == QStringLiteral("fishhook") ? 1 : 0);
    } else if (kind == QStringLiteral("equilibrium")) {
        // Upper line forward, lower line back (y points down), barbs outside.
        writeLine(stream, x1 - nx * 4, y1 - ny * 4, x2 - nx * 4, y2 - ny * 4);
        writeHead(stream, x2 - nx * 4, y2 - ny * 4, ux, uy, -1);
        writeLine(stream, x2 + nx * 4, y2 + ny * 4, x1 + nx * 4, y1 + ny * 4);
        writeHead(stream, x1 + nx * 4, y1 + ny * 4, -ux, -uy, -1);
    } else if (kind == QStringLiteral("retro")) {
        const double back = qMin(length * 0.5, 14.0);
        writeLine(stream, x1 + nx * 4, y1 + ny * 4, x2 - ux * back + nx * 4, y2 - uy * back + ny * 4);
        writeLine(stream, x1 - nx * 4, y1 - ny * 4, x2 - ux * back - nx * 4, y2 - uy * back - ny * 4);
        writeHead(stream, x2, y2, ux, uy, 0, 16, 11);
    } else if (kind == QStringLiteral("resonance")) {
        writeLine(stream, x1, y1, x2, y2);
        writeHead(stream, x2, y2, ux, uy, 0);
        writeHead(stream, x1, y1, -ux, -uy, 0);
    } else {
        writeLine(stream, x1, y1, x2, y2, kind == QStringLiteral("dashed") ? " stroke-dasharray=\"9 6\"" : "");
        writeHead(stream, x2, y2, ux, uy, 0);
        if (kind == QStringLiteral("noreaction")) {
            const double mx = (x1 + x2) / 2, my = (y1 + y2) / 2;
            writeLine(stream, mx - (ux + nx) * 9, my - (uy + ny) * 9, mx + (ux + nx) * 9, my + (uy + ny) * 9);
            writeLine(stream, mx - (ux - nx) * 9, my - (uy - ny) * 9, mx + (ux - nx) * 9, my + (uy - ny) * 9);
        }
    }
    stream << "</g>\n";
}

QString escaped(const QString &text) {
    QString value = text;
    return value.replace('&', QStringLiteral("&amp;")).replace('<', QStringLiteral("&lt;")).replace('>', QStringLiteral("&gt;"));
}
}

QString SvgWriter::write(const Molecule &molecule, const QVariantList &reactionItems) {
    const QVector<Atom> &atoms = molecule.atomData();
    QHash<int, Atom> byId;
    bool hasContent = !atoms.isEmpty() || !reactionItems.isEmpty();
    if (!hasContent) return QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1\" height=\"1\"/>\n");
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    bool boundsInitialized = !atoms.isEmpty();
    if (boundsInitialized) minX = maxX = atoms.first().x, minY = maxY = atoms.first().y;
    for (const Atom &atom : atoms) {
        byId.insert(atom.id, atom);
        minX = qMin(minX, atom.x); maxX = qMax(maxX, atom.x);
        minY = qMin(minY, atom.y); maxY = qMax(maxY, atom.y);
    }
    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        const double x1 = item.value(QStringLiteral("x1")).toDouble(), y1 = item.value(QStringLiteral("y1")).toDouble();
        const double x2 = item.value(QStringLiteral("x2")).toDouble(), y2 = item.value(QStringLiteral("y2")).toDouble();
        if (!boundsInitialized) { minX = maxX = x1; minY = maxY = y1; boundsInitialized = true; }
        minX = qMin(minX, qMin(x1, x2)); maxX = qMax(maxX, qMax(x1, x2));
        minY = qMin(minY, qMin(y1, y2)); maxY = qMax(maxY, qMax(y1, y2));
        if (item.contains(QStringLiteral("cx")) && ReactionLayer::isCurvedKind(item.value(QStringLiteral("kind")).toString())) {
            // The curve stays inside the hull of its three points; its apex is half-way to the control point.
            const double cx = item.value(QStringLiteral("cx")).toDouble(), cy = item.value(QStringLiteral("cy")).toDouble();
            const double apexX = (x1 + 2 * cx + x2) / 4, apexY = (y1 + 2 * cy + y2) / 4;
            minX = qMin(minX, apexX); maxX = qMax(maxX, apexX);
            minY = qMin(minY, apexY); maxY = qMax(maxY, apexY);
        }
    }
    constexpr double padding = 32.0;
    QString output;
    QTextStream stream(&output);
    const double svgWidth = maxX - minX + 2 * padding, svgHeight = maxY - minY + 2 * padding;
    stream << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << svgWidth << "\" height=\"" << svgHeight
           << "\" viewBox=\"" << minX - padding << " " << minY - padding << " " << svgWidth << " " << svgHeight
           << "\" stroke=\"#111\" fill=\"#111\">\n";
    stream << "<g stroke-width=\"3\" stroke-linecap=\"round\">\n";
    for (const Bond &bond : molecule.bondData()) {
        const Atom a = byId.value(bond.atomA), b = byId.value(bond.atomB);
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double length = qSqrt(dx * dx + dy * dy);
        const double nx = length ? -dy / length : 0, ny = length ? dx / length : 0;
        if (bond.style == QStringLiteral("wedge")) {
            stream << "<polygon points=\"" << a.x << ',' << a.y << ' ' << b.x + nx * 8 << ',' << b.y + ny * 8 << ' '
                   << b.x - nx * 8 << ',' << b.y - ny * 8 << "\"/>\n";
            continue;
        }
        if (bond.style == QStringLiteral("dash")) {
            for (int dash = 1; dash < 7; ++dash) {
                const double progress = dash / 7.0, width = 1 + progress * 8;
                const double x = a.x + dx * progress, y = a.y + dy * progress;
                stream << "<line x1=\"" << x + nx * width << "\" y1=\"" << y + ny * width << "\" x2=\""
                       << x - nx * width << "\" y2=\"" << y - ny * width << "\"/>\n";
            }
            continue;
        }
        if (bond.style == QStringLiteral("wavy")) {
            // Same wave as MoleculeCanvas.qml draws on screen.
            stream << "<path fill=\"none\" d=\"M" << a.x << ',' << a.y;
            for (int wave = 1; wave <= 6; ++wave) {
                const double progress = wave / 6.0, sign = wave % 2 == 0 ? -1.0 : 1.0;
                const double px = a.x + dx * progress, py = a.y + dy * progress;
                stream << " Q" << px + nx * sign * 7 << ',' << py + ny * sign * 7 << ' ' << px << ',' << py;
            }
            stream << "\"/>\n";
            continue;
        }
        if (bond.style == QStringLiteral("bold")) {
            stream << "<line stroke-width=\"7\" x1=\"" << a.x << "\" y1=\"" << a.y << "\" x2=\"" << b.x << "\" y2=\"" << b.y << "\"/>\n";
            continue;
        }
        if (bond.style == QStringLiteral("newman")) {
            // Newman projection: the circle for the rear carbon, centred between the two carbons.
            stream << "<circle fill=\"none\" cx=\"" << (a.x + b.x) / 2 << "\" cy=\"" << (a.y + b.y) / 2
                   << "\" r=\"" << molecule.newmanCircleRadius() << "\"/>\n";
            continue;
        }
        if (bond.style == QStringLiteral("newmanback")) {
            // Rear bonds of a Newman projection start at the circle, not at the carbon.
            const double inset = length > 0.0 ? qMin(molecule.newmanCircleRadius(), length) / length : 0.0;
            stream << "<line x1=\"" << a.x + dx * inset << "\" y1=\"" << a.y + dy * inset << "\" x2=\"" << b.x << "\" y2=\"" << b.y << "\"/>\n";
            continue;
        }
        if (bond.style == QStringLiteral("benzene")) {
            stream << "<line x1=\"" << a.x << "\" y1=\"" << a.y << "\" x2=\"" << b.x << "\" y2=\"" << b.y << "\"/>\n";
            if (bond.order == 2 && length > 0.0) {
                const double inset = 7.0;
                const double startX = a.x + dx * 0.18 + nx * inset;
                const double startY = a.y + dy * 0.18 + ny * inset;
                const double endX = b.x - dx * 0.18 + nx * inset;
                const double endY = b.y - dy * 0.18 + ny * inset;
                stream << "<line x1=\"" << startX << "\" y1=\"" << startY << "\" x2=\"" << endX << "\" y2=\"" << endY << "\"/>\n";
            }
            continue;
        }
        if (bond.order == 2 && bond.doubleSide != 0 && bond.style == QStringLiteral("plain") && length > 0.0) {
            // Full line on the atom axis plus a shorter parallel line to one side.
            const double inset = bond.doubleSide > 0 ? 7.0 : -7.0;
            stream << "<line x1=\"" << a.x << "\" y1=\"" << a.y << "\" x2=\"" << b.x << "\" y2=\"" << b.y << "\"/>\n";
            stream << "<line x1=\"" << a.x + dx * 0.12 + nx * inset << "\" y1=\"" << a.y + dy * 0.12 + ny * inset
                   << "\" x2=\"" << b.x - dx * 0.12 + nx * inset << "\" y2=\"" << b.y - dy * 0.12 + ny * inset << "\"/>\n";
            continue;
        }
        const QString dashArray = bond.style == QStringLiteral("aromatic") ? QStringLiteral(" stroke-dasharray=\"7 5\"") : QString();
        const QVector<double> offsets = bond.order == 1 ? QVector<double>{0} : bond.order == 2 ? QVector<double>{-4, 4} : QVector<double>{-6, 0, 6};
        for (double offset : offsets) stream << "<line x1=\"" << a.x + nx * offset << "\" y1=\"" << a.y + ny * offset
            << "\" x2=\"" << b.x + nx * offset << "\" y2=\"" << b.y + ny * offset << "\"" << dashArray << "/>\n";
    }
    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("type")).toString() != QStringLiteral("arrow")) continue;
        writeArrow(stream, item);
    }
    stream << "</g>\n<g font-family=\"sans-serif\" font-size=\"20\" text-anchor=\"middle\" dominant-baseline=\"middle\">\n";
    QString dots;
    for (const Atom &atom : atoms) {
        const Molecule::AtomDisplay display = molecule.atomDisplay(atom);
        constexpr double halfChar = 6.5; // rough half width of one 20px capital letter
        double labelLeft = atom.x, labelRight = atom.x;
        if (display.showLabel) {
            // Same layout as MoleculeCanvas.qml: the chosen characters sit on the atom.
            const double centerWidth = display.centerLength * 2 * halfChar;
            const QString before = display.text.left(display.centerIndex);
            const QString center = display.text.mid(display.centerIndex, display.centerLength);
            const QString after = display.text.mid(display.centerIndex + display.centerLength);
            const QString stroke = QStringLiteral(" stroke=\"white\" stroke-width=\"7\" paint-order=\"stroke\"");
            if (!before.isEmpty())
                stream << "<text x=\"" << atom.x - centerWidth / 2 << "\" y=\"" << atom.y << "\" text-anchor=\"end\"" << stroke << ">" << escaped(before) << "</text>\n";
            stream << "<text x=\"" << atom.x << "\" y=\"" << atom.y << "\"" << stroke << ">" << escaped(center) << "</text>\n";
            if (!after.isEmpty())
                stream << "<text x=\"" << atom.x + centerWidth / 2 << "\" y=\"" << atom.y << "\" text-anchor=\"start\"" << stroke << ">" << escaped(after) << "</text>\n";
            labelLeft = atom.x - centerWidth / 2 - before.size() * 10.0;
            labelRight = atom.x + centerWidth / 2 + after.size() * 10.0;
        }
        if (display.charge) {
            const double baseX = display.showLabel ? labelRight + 2 : atom.x + 7;
            const double cy = atom.y - 11;
            if (molecule.circledCharges()) {
                const double cx = baseX + 5;
                dots += QStringLiteral("<circle cx=\"%1\" cy=\"%2\" r=\"5\" fill=\"white\" stroke-width=\"1.2\"/>\n").arg(cx).arg(cy);
                dots += QStringLiteral("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\" stroke-width=\"1.2\"/>\n").arg(cx - 3).arg(cy).arg(cx + 3);
                if (display.charge > 0)
                    dots += QStringLiteral("<line x1=\"%1\" y1=\"%2\" x2=\"%1\" y2=\"%3\" stroke-width=\"1.2\"/>\n").arg(cx).arg(cy - 3).arg(cy + 3);
            } else {
                stream << "<text x=\"" << baseX << "\" y=\"" << atom.y - 8 << "\" font-size=\"14\" text-anchor=\"start\" stroke=\"white\" stroke-width=\"4\" paint-order=\"stroke\">"
                       << escaped(display.chargeLabel) << "</text>\n";
            }
        }
        const double ring = display.showLabel ? 16.0 : 10.0;
        for (double angle : display.lonePairAngles) {
            const double radians = qDegreesToRadians(angle);
            const double cx = atom.x + ring * qCos(radians), cy = atom.y + ring * qSin(radians);
            const double px = -qSin(radians) * 3.0, py = qCos(radians) * 3.0;
            dots += QStringLiteral("<circle cx=\"%1\" cy=\"%2\" r=\"1.8\" stroke=\"none\"/>\n<circle cx=\"%3\" cy=\"%4\" r=\"1.8\" stroke=\"none\"/>\n")
                        .arg(cx + px).arg(cy + py).arg(cx - px).arg(cy - py);
        }
        for (double angle : display.radicalAngles) {
            const double radians = qDegreesToRadians(angle);
            dots += QStringLiteral("<circle cx=\"%1\" cy=\"%2\" r=\"2.2\" stroke=\"none\"/>\n")
                        .arg(atom.x + ring * qCos(radians)).arg(atom.y + ring * qSin(radians));
        }
        Q_UNUSED(labelLeft)
    }
    stream << dots;
    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("type")).toString() == QStringLiteral("label")) {
            const QString text = item.value(QStringLiteral("text")).toString();
            const bool bold = item.contains(QStringLiteral("bold")) ? item.value(QStringLiteral("bold")).toBool() : ReactionLayer::isCompoundNumber(text);
            stream << "<text x=\"" << item.value(QStringLiteral("x1")).toDouble() << "\" y=\"" << item.value(QStringLiteral("y1")).toDouble()
                   << "\" stroke=\"none\"" << (bold ? " font-weight=\"bold\"" : "") << ">" << escaped(text) << "</text>\n";
        }
    }
    stream << "</g>\n</svg>\n";
    return output;
}
