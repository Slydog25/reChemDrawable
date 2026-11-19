#pragma once

#include <QImage>
#include <QSize>
#include <QVariantList>

class Molecule;

// Renders a small sketch of a drawing (bonds, element labels, arrows, text labels)
// straight from core data, so it works headless and does not depend on what part
// of the canvas is visible.
class ThumbnailWriter {
public:
    static constexpr int DefaultWidth = 240;
    static constexpr int DefaultHeight = 160;

    static QImage render(const Molecule &molecule, const QVariantList &reactionItems,
                         const QSize &size = QSize(DefaultWidth, DefaultHeight));
};
