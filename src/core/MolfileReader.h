#pragma once

#include <QString>

class Molecule;

class MolfileReader {
public:
    static bool readV2000(const QString &contents, Molecule *molecule, QString *error = nullptr);
};

