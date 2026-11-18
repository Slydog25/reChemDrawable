#pragma once

#include <QString>
#include <QVariantList>

class Molecule;

class SvgWriter {
public:
    static QString write(const Molecule &molecule, const QVariantList &reactionItems = {});
};
