#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

// Group abbreviations (OMe, Boc, Ts, TBS, ...) drawn as a single labelled atom
// with one attachment bond, as in ChemDraw. They count fully toward formula,
// masses and SMILES.
namespace Abbreviations {

struct Info {
    QString name;      // label when the bond comes from the left, e.g. "OMe"
    QString reversed;  // label when the bond comes from the right, e.g. "MeO"
    // SMILES of the group with the attachment atom first. Ring closures use
    // %91-%99 so they never clash with the surrounding molecule's digits.
    QString smiles;
    QMap<QString, int> composition; // atoms of the group, attachment H excluded
};

const Info *find(const QString &name);
bool isAbbreviation(const QString &name);
// Names in palette order.
QStringList names();

} // namespace Abbreviations
