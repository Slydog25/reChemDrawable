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
        writeLine(stream, x1 + nx * 4, y1 + ny * 4, x2 + nx * 4, y2 + ny * 4);
        writeHead(stream, x2 + nx * 4, y2 + ny * 4, ux, uy, 1);
        writeLine(stream, x2 - nx * 4, y2 - ny * 4, x1 - nx * 4, y1 - ny * 4);
        writeHead(stream, x1 - nx * 4, y1 - ny * 4, -ux, -uy, 1);
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
    for (const Atom &atom : atoms) {
        if (atom.element == QStringLiteral("C") && !atom.formalCharge && !atom.isotope) continue;
        const QString charge = atom.formalCharge == 1 ? QStringLiteral("+") : atom.formalCharge == -1 ? QStringLiteral("−") :
            atom.formalCharge ? QStringLiteral("%1%2").arg(qAbs(atom.formalCharge)).arg(atom.formalCharge > 0 ? QStringLiteral("+") : QStringLiteral("−")) : QString();
        stream << "<text x=\"" << atom.x << "\" y=\"" << atom.y << "\" stroke=\"white\" stroke-width=\"7\" paint-order=\"stroke\">"
               << escaped((atom.isotope ? QString::number(atom.isotope) : QString()) + atom.element + charge) << "</text>\n";
    }
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
