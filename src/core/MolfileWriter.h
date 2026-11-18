#pragma once

#include <QString>

class Molecule;

class MolfileWriter {
public:
    static QString writeV2000(const Molecule &molecule, const QString &title = QStringLiteral("reChemDrawable"));
};

