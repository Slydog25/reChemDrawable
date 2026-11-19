#pragma once

#include <QImage>
#include <QRectF>
#include <QString>
#include <QVariantList>

class Molecule;
class QPainter;

// One QPainter renderer for the whole scene (bonds, atom labels, charges, lone
// pairs, radicals, every arrow kind, text labels). It paints in model
// coordinates with the same geometry SvgWriter writes, so PNG, PDF and SVG
// match; the caller sets up the painter transform (scale/translate) and the
// target device. Works on QImage, QPdfWriter, or any other paint device.
class SceneRenderer {
public:
    // Margin (model units) added around the content for exports.
    static constexpr double ExportMargin = 32.0;
    // PNG: model units are CSS pixels (96 per inch); 300 dpi is 300/96 pixels per unit.
    static constexpr double PngScale = 300.0 / 96.0;
    // PDF: one model unit is 0.6 point, so a standard bond (48 units) is 28.8 pt (1 cm).
    static constexpr double PdfPointsPerUnit = 0.6;

    // Paints everything in model coordinates; fills nothing (caller paints background).
    static void paint(QPainter &painter, const Molecule &molecule, const QVariantList &reactionItems);
    // Tight bounds of everything drawn (labels and arrow curves included), no margin.
    // Null rectangle when nothing is drawn.
    static QRectF contentBounds(const Molecule &molecule, const QVariantList &reactionItems);
    // White background, tight bounds plus margin, scaled by `scale` pixels per unit.
    // Returns a null image when there is nothing to draw.
    static QImage renderImage(const Molecule &molecule, const QVariantList &reactionItems,
                              double scale = PngScale, double margin = ExportMargin);
    // Vector PDF whose single page is sized to the drawing plus margin (see
    // PdfPointsPerUnit). Returns false when nothing is drawn or the file cannot be written.
    static bool writePdf(const QString &path, const Molecule &molecule, const QVariantList &reactionItems,
                         const QString &title = QString());
};
