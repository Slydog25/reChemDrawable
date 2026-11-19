#pragma once

#include <QString>
#include <QVariantList>

class Molecule;

// ChemDraw XML (CDXML) export. Conventions follow the CambridgeSoft CDXML
// reference (CDXML.DTD / "ChemDraw File Format" documentation): a CDXML root
// with fonttable/colortable and one page; a <fragment> holding <n> nodes (p =
// "x y" in points, Element = atomic number, Charge, Isotope, Radical) and <b>
// bonds (B/E node ids, Order, Display, DoublePosition); text as <t><s>..</s></t>;
// arrows as <arrow> (forward, dashed, no-go) or <graphic GraphicType="Line"
// ArrowType=...> (equilibrium, resonance, retrosynthetic), curved arrows as
// <curve CurvePoints=...>.
//
// Coordinates: model units are scaled so one standard bond (48) is ChemDraw's
// default 14.4 pt; y points down in both systems.
//
// Approximations: curved/fishhook arrows write the quadratic control point as a
// three-point CurvePoints list (ChemDraw expects cubic Bezier points, so it
// reads it as a smooth spline through them); lone pairs are not exported;
// group abbreviations become NodeType="Nickname" nodes whose label is the
// abbreviation text; wavy bonds use Display="Wavy"; "benzene" inner lines are
// exported as a plain alternating double bond.
class CdxmlWriter {
public:
    static constexpr double BondLengthPoints = 14.4;
    static QString write(const Molecule &molecule, const QVariantList &reactionItems = {});
};
