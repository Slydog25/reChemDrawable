#include "CdxmlWriter.h"

#include "Abbreviations.h"
#include "Elements.h"
#include "GeometryEngine.h"
#include "Molecule.h"
#include "ReactionLayer.h"

#include <QHash>
#include <QXmlStreamWriter>

namespace {
constexpr double kScale = CdxmlWriter::BondLengthPoints / GeometryEngine::StandardBondLength;

QString num(double value) { return QString::number(value * kScale, 'f', 2); }
QString pos(double x, double y) { return num(x) + QLatin1Char(' ') + num(y); }

void writeText(QXmlStreamWriter &xml, int id, double x, double y, const QString &text, bool bold, const QString &justification = QStringLiteral("Center")) {
    xml.writeStartElement(QStringLiteral("t"));
    xml.writeAttribute(QStringLiteral("id"), QString::number(id));
    xml.writeAttribute(QStringLiteral("p"), pos(x, y));
    xml.writeAttribute(QStringLiteral("Justification"), justification);
    xml.writeStartElement(QStringLiteral("s"));
    xml.writeAttribute(QStringLiteral("font"), QStringLiteral("3"));
    xml.writeAttribute(QStringLiteral("size"), QStringLiteral("10"));
    xml.writeAttribute(QStringLiteral("color"), QStringLiteral("0"));
    if (bold) xml.writeAttribute(QStringLiteral("face"), QStringLiteral("1")); // 1 = bold
    xml.writeCharacters(text);
    xml.writeEndElement();
    xml.writeEndElement();
}

void writeArrow(QXmlStreamWriter &xml, int &nextId, const QVariantMap &item) {
    const double x1 = item.value(QStringLiteral("x1")).toDouble(), y1 = item.value(QStringLiteral("y1")).toDouble();
    const double x2 = item.value(QStringLiteral("x2")).toDouble(), y2 = item.value(QStringLiteral("y2")).toDouble();
    const QString kind = ReactionLayer::normalizedKind(item.value(QStringLiteral("kind")).toString());
    const QString bbox = num(qMin(x1, x2)) + QLatin1Char(' ') + num(qMin(y1, y2)) + QLatin1Char(' ') + num(qMax(x1, x2)) + QLatin1Char(' ') + num(qMax(y1, y2));
    if (kind == QStringLiteral("curved") || kind == QStringLiteral("fishhook")) {
        const double cx = item.value(QStringLiteral("cx"), (x1 + x2) / 2).toDouble(), cy = item.value(QStringLiteral("cy"), (y1 + y2) / 2).toDouble();
        xml.writeStartElement(QStringLiteral("curve"));
        xml.writeAttribute(QStringLiteral("id"), QString::number(nextId++));
        xml.writeAttribute(QStringLiteral("CurvePoints"), pos(x1, y1) + QLatin1Char(' ') + pos(cx, cy) + QLatin1Char(' ') + pos(x2, y2));
        // Fishhook = single-electron (half) arrowhead; electron-pair arrow = full head.
        xml.writeAttribute(QStringLiteral("ArrowheadHead"), kind == QStringLiteral("fishhook") ? QStringLiteral("HalfLeft") : QStringLiteral("Full"));
        xml.writeAttribute(QStringLiteral("ArrowheadType"), QStringLiteral("Solid"));
        xml.writeEndElement();
        return;
    }
    if (kind == QStringLiteral("equilibrium") || kind == QStringLiteral("resonance") || kind == QStringLiteral("retro")) {
        xml.writeStartElement(QStringLiteral("graphic"));
        xml.writeAttribute(QStringLiteral("id"), QString::number(nextId++));
        xml.writeAttribute(QStringLiteral("BoundingBox"), bbox);
        xml.writeAttribute(QStringLiteral("GraphicType"), QStringLiteral("Line"));
        xml.writeAttribute(QStringLiteral("ArrowType"),
                           kind == QStringLiteral("equilibrium") ? QStringLiteral("Equilibrium")
                           : kind == QStringLiteral("resonance") ? QStringLiteral("Resonance") : QStringLiteral("RetroSynthetic"));
        xml.writeEndElement();
        return;
    }
    xml.writeStartElement(QStringLiteral("arrow"));
    xml.writeAttribute(QStringLiteral("id"), QString::number(nextId++));
    xml.writeAttribute(QStringLiteral("BoundingBox"), bbox);
    xml.writeAttribute(QStringLiteral("Head3D"), num(x2) + QLatin1Char(' ') + num(y2) + QStringLiteral(" 0"));
    xml.writeAttribute(QStringLiteral("Tail3D"), num(x1) + QLatin1Char(' ') + num(y1) + QStringLiteral(" 0"));
    xml.writeAttribute(QStringLiteral("ArrowheadHead"), QStringLiteral("Full"));
    xml.writeAttribute(QStringLiteral("ArrowheadType"), QStringLiteral("Solid"));
    xml.writeAttribute(QStringLiteral("HeadSize"), QStringLiteral("1000"));
    xml.writeAttribute(QStringLiteral("ArrowheadCenterSize"), QStringLiteral("875"));
    xml.writeAttribute(QStringLiteral("ArrowheadWidth"), QStringLiteral("250"));
    if (kind == QStringLiteral("dashed")) xml.writeAttribute(QStringLiteral("LineType"), QStringLiteral("Dashed"));
    if (kind == QStringLiteral("noreaction")) xml.writeAttribute(QStringLiteral("NoGo"), QStringLiteral("Cross"));
    xml.writeEndElement();
}
}

QString CdxmlWriter::write(const Molecule &molecule, const QVariantList &reactionItems) {
    QString output;
    QXmlStreamWriter xml(&output);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(1);
    xml.writeStartDocument(QStringLiteral("1.0"));
    xml.writeDTD(QStringLiteral("<!DOCTYPE CDXML SYSTEM \"http://www.cambridgesoft.com/xml/cdxml.dtd\" >"));
    xml.writeStartElement(QStringLiteral("CDXML"));
    xml.writeAttribute(QStringLiteral("CreationProgram"), QStringLiteral("reChemDrawable"));
    xml.writeAttribute(QStringLiteral("BondLength"), QString::number(BondLengthPoints));
    xml.writeAttribute(QStringLiteral("BoldWidth"), QStringLiteral("2"));
    xml.writeAttribute(QStringLiteral("LineWidth"), QStringLiteral("0.6"));
    xml.writeAttribute(QStringLiteral("MarginWidth"), QStringLiteral("1.6"));
    xml.writeAttribute(QStringLiteral("HashSpacing"), QStringLiteral("2.5"));
    xml.writeAttribute(QStringLiteral("LabelFont"), QStringLiteral("3"));
    xml.writeAttribute(QStringLiteral("LabelSize"), QStringLiteral("10"));
    xml.writeAttribute(QStringLiteral("LabelFace"), QStringLiteral("96"));
    xml.writeAttribute(QStringLiteral("CaptionFont"), QStringLiteral("3"));
    xml.writeAttribute(QStringLiteral("CaptionSize"), QStringLiteral("10"));
    xml.writeStartElement(QStringLiteral("colortable"));
    for (const char *rgb : {"1 1 1", "0 0 0"}) {
        const QStringList parts = QString::fromLatin1(rgb).split(QLatin1Char(' '));
        xml.writeEmptyElement(QStringLiteral("color"));
        xml.writeAttribute(QStringLiteral("r"), parts[0]);
        xml.writeAttribute(QStringLiteral("g"), parts[1]);
        xml.writeAttribute(QStringLiteral("b"), parts[2]);
    }
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("fonttable"));
    xml.writeEmptyElement(QStringLiteral("font"));
    xml.writeAttribute(QStringLiteral("id"), QStringLiteral("3"));
    xml.writeAttribute(QStringLiteral("charset"), QStringLiteral("iso-8859-1"));
    xml.writeAttribute(QStringLiteral("name"), QStringLiteral("Arial"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("page"));
    xml.writeAttribute(QStringLiteral("id"), QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("BoundingBox"), QStringLiteral("0 0 540 720"));
    xml.writeAttribute(QStringLiteral("HeightPages"), QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("WidthPages"), QStringLiteral("1"));

    int nextId = 2;
    QHash<int, int> nodeIds; // atom id -> cdxml id
    if (!molecule.atomData().isEmpty()) {
        xml.writeStartElement(QStringLiteral("fragment"));
        xml.writeAttribute(QStringLiteral("id"), QString::number(nextId++));
        for (const Atom &atom : molecule.atomData()) {
            const int id = nextId++;
            nodeIds.insert(atom.id, id);
            const bool nickname = Abbreviations::isAbbreviation(atom.element);
            xml.writeStartElement(QStringLiteral("n"));
            xml.writeAttribute(QStringLiteral("id"), QString::number(id));
            xml.writeAttribute(QStringLiteral("p"), pos(atom.x, atom.y));
            if (nickname) {
                xml.writeAttribute(QStringLiteral("NodeType"), QStringLiteral("Nickname"));
            } else {
                const int z = Elements::atomicNumber(atom.element);
                if (z != 6 && z > 0) xml.writeAttribute(QStringLiteral("Element"), QString::number(z)); // default is carbon
                if (atom.formalCharge) xml.writeAttribute(QStringLiteral("Charge"), QString::number(atom.formalCharge));
                if (atom.isotope) xml.writeAttribute(QStringLiteral("Isotope"), QString::number(atom.isotope));
                if (atom.radical) xml.writeAttribute(QStringLiteral("Radical"), atom.radical == 1 ? QStringLiteral("Doublet") : QStringLiteral("Triplet"));
            }
            const Molecule::AtomDisplay display = molecule.atomDisplay(atom);
            // Heteroatom and abbreviation labels are written out so the file looks the same
            // before ChemDraw regenerates them; plain carbon needs none.
            if (nickname || display.showLabel) {
                xml.writeStartElement(QStringLiteral("t"));
                xml.writeAttribute(QStringLiteral("p"), pos(atom.x, atom.y));
                xml.writeAttribute(QStringLiteral("LabelJustification"), display.labelLeft ? QStringLiteral("Right") : QStringLiteral("Left"));
                xml.writeStartElement(QStringLiteral("s"));
                xml.writeAttribute(QStringLiteral("font"), QStringLiteral("3"));
                xml.writeAttribute(QStringLiteral("size"), QStringLiteral("10"));
                xml.writeAttribute(QStringLiteral("color"), QStringLiteral("0"));
                xml.writeCharacters(nickname ? atom.element : display.text);
                xml.writeEndElement();
                xml.writeEndElement();
            }
            xml.writeEndElement();
        }
        for (const Bond &bond : molecule.bondData()) {
            if (!nodeIds.contains(bond.atomA) || !nodeIds.contains(bond.atomB)) continue;
            xml.writeStartElement(QStringLiteral("b"));
            xml.writeAttribute(QStringLiteral("id"), QString::number(nextId++));
            xml.writeAttribute(QStringLiteral("B"), QString::number(nodeIds.value(bond.atomA)));
            xml.writeAttribute(QStringLiteral("E"), QString::number(nodeIds.value(bond.atomB)));
            // Begin is the narrow end for wedges, as in our drawing (wide end at atomB).
            if (bond.order != 1) xml.writeAttribute(QStringLiteral("Order"), QString::number(bond.order));
            if (bond.style == QStringLiteral("wedge")) xml.writeAttribute(QStringLiteral("Display"), QStringLiteral("WedgeBegin"));
            else if (bond.style == QStringLiteral("dash")) xml.writeAttribute(QStringLiteral("Display"), QStringLiteral("WedgedHashBegin"));
            else if (bond.style == QStringLiteral("wavy")) xml.writeAttribute(QStringLiteral("Display"), QStringLiteral("Wavy"));
            else if (bond.style == QStringLiteral("aromatic")) xml.writeAttribute(QStringLiteral("Display2"), QStringLiteral("Dash"));
            if (bond.order == 2 && bond.style == QStringLiteral("plain") && bond.doubleSide != 0)
                xml.writeAttribute(QStringLiteral("DoublePosition"), bond.doubleSide > 0 ? QStringLiteral("Right") : QStringLiteral("Left"));
            xml.writeEndElement();
        }
        xml.writeEndElement(); // fragment
    }
    for (const QVariant &value : reactionItems) {
        const QVariantMap item = value.toMap();
        const QString type = item.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("arrow")) {
            writeArrow(xml, nextId, item);
        } else if (type == QStringLiteral("label")) {
            const QString text = item.value(QStringLiteral("text")).toString();
            const bool bold = item.contains(QStringLiteral("bold")) ? item.value(QStringLiteral("bold")).toBool() : ReactionLayer::isCompoundNumber(text);
            writeText(xml, nextId++, item.value(QStringLiteral("x1")).toDouble(), item.value(QStringLiteral("y1")).toDouble(), text, bold);
        }
    }
    xml.writeEndElement(); // page
    xml.writeEndElement(); // CDXML
    xml.writeEndDocument();
    return output;
}
